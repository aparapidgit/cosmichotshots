#include "Race/RaceTrackerComponent.h"

#include "Chaos/ParticleHandle.h"
#include "Components/PrimitiveComponent.h"
#include "CosmicHotshotsGame.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Race/RaceDebug.h"
#include "Race/RaceTuningData.h"
#include "Race/TrackSplineActor.h"
#include "Race/TrackSplineTable.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleDebug.h"

namespace
{
	const FName CheckpointTag(TEXT("Checkpoint"));
	const TCHAR* CheckpointIndexPrefix = TEXT("CP_");

	/** Used when the level has no checkpoint markers: the lap is split evenly. */
	constexpr int32 FallbackCheckpointCount = 4;

	// Debug drawing only.
	constexpr float DebugLineStep = 500.f;
	constexpr float DebugLineThickness = 8.f;
	constexpr float DebugMarkerSize = 150.f;
	constexpr float DebugCheckpointHeight = 800.f;

	/** Position of a vehicle's physics body as seen by the current physics step, unset when it has none. */
	TOptional<FVector> ReadBodyLocation(const ARaceVehicle* Vehicle)
	{
		const UPrimitiveComponent* Root = Vehicle ? Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()) : nullptr;
		const FBodyInstance* BodyInstance = Root ? Root->GetBodyInstance() : nullptr;
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			return {};
		}
		FPhysicsActorHandle Proxy = BodyInstance->GetPhysicsActor();
		const Chaos::FRigidBodyHandle_Internal* Body = Proxy ? Proxy->GetPhysicsThreadAPI() : nullptr;
		if (!Body)
		{
			return {};
		}
		return FVector(Body->X());
	}
}

URaceTrackerComponent::URaceTrackerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Before physics: cars spawned this frame join the race at the frame's first physics step.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// The engine asserts that every component registered for the async physics tick is active.
	bAutoActivate = true;
}

void URaceTrackerComponent::BeginPlay()
{
	Super::BeginPlay();

	Track = ATrackSplineActor::FindTable(GetWorld());
	if (!Track.IsValid() || !Track->IsValid())
	{
		Track.Reset();
		UE_LOG(LogCosmicRace, Warning, TEXT("No track spline (ATrackSplineActor) in this level: race state is not tracked."));
		return;
	}

	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	const URaceTuningData* Tuning = Settings->RaceTuning.LoadSynchronous();
	if (!Tuning)
	{
		Tuning = GetDefault<URaceTuningData>();
	}

	Params.NumLaps = Tuning->NumLaps;
	Params.ProjectionWindow = Tuning->ProjectionWindow;
	Params.ReacquireDistance = Tuning->ReacquireDistance;
	Params.OvertakeConfirmTime = Tuning->OvertakeConfirmTime;
	Params.OvertakeMargin = Tuning->OvertakeMargin;
	Params.OvertakePairCooldown = Tuning->OvertakePairCooldown;
	Params.CloseOvertakeLateral = Tuning->CloseOvertakeLateral;
	Params.GapBucketSize = Tuning->GapBucketSize;
	Params.MinGapSpeed = Tuning->MinGapSpeed;
	BuildCheckpoints();

	UE_LOG(LogCosmicRace, Log, TEXT("RACE_SETUP lapLength=%.0f laps=%d checkpoints=%d"),
		Track->GetLapLength(), Params.NumLaps, Params.CheckpointDistances.Num());

	// The game mode may spawn cars in its pre-physics tick (test dummy); register them in the same frame.
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		PrimaryComponentTick.AddPrerequisite(GameMode, GameMode->PrimaryActorTick);
	}

	// Enabled here, not in the constructor, so the physics step never sees a component that is not active yet.
	SetAsyncPhysicsTickEnabled(true);
}

void URaceTrackerComponent::BuildCheckpoints()
{
	const URaceTuningData* Tuning = GetDefault<UCosmicHotshotsSettings>()->RaceTuning.LoadSynchronous();
	const float Tolerance = Tuning ? Tuning->CheckpointTolerance : GetDefault<URaceTuningData>()->CheckpointTolerance;
	const float LapLength = Track->GetLapLength();

	// Checkpoint index from the "CP_<index>" tag -> distance along the line.
	TMap<int32, float> Markers;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* Actor = *It;
		if (!Actor->ActorHasTag(CheckpointTag))
		{
			continue;
		}

		int32 Index = INDEX_NONE;
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(CheckpointIndexPrefix) && TagString.RightChop(FCString::Strlen(CheckpointIndexPrefix)).IsNumeric())
			{
				Index = FCString::Atoi(*TagString.RightChop(FCString::Strlen(CheckpointIndexPrefix)));
			}
		}
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogCosmicRace, Warning, TEXT("Checkpoint %s has no CP_<index> tag; ignored."), *Actor->GetName());
			continue;
		}

		const FTrackProjection Projection = Track->Project(Actor->GetActorLocation());
		if (FMath::Abs(Projection.LateralOffset) > Tolerance)
		{
			UE_LOG(LogCosmicRace, Warning, TEXT("Checkpoint CP_%d (%s) is %.0f cm beside the track line (tolerance %.0f): map and track spline do not match?"),
				Index, *Actor->GetName(), Projection.LateralOffset, Tolerance);
		}
		Markers.Add(Index, Projection.Distance);
	}

	Params.CheckpointDistances.Reset();
	Params.CheckpointDistances.Add(0.f);

	Markers.KeySort(TLess<int32>());
	bool bValid = Markers.Num() > 0;
	float Previous = 0.f;
	for (const TPair<int32, float>& Marker : Markers)
	{
		float Distance = Marker.Value;
		if (Marker.Key == 0)
		{
			// The start/finish marker must sit on spline point 0 (distance 0 or, just before it, LapLength).
			const float FromStart = FMath::Min(Distance, LapLength - Distance);
			if (FromStart > Tolerance)
			{
				UE_LOG(LogCosmicRace, Warning, TEXT("CP_0 is %.0f cm from the start of the track spline (tolerance %.0f)."), FromStart, Tolerance);
			}
			continue;
		}
		if (Distance <= Previous)
		{
			UE_LOG(LogCosmicRace, Warning, TEXT("CP_%d lies at %.0f cm, not after the previous checkpoint (%.0f cm): checkpoints are out of order."),
				Marker.Key, Distance, Previous);
			bValid = false;
			break;
		}
		Params.CheckpointDistances.Add(Distance);
		Previous = Distance;
	}

	if (!bValid)
	{
		Params.CheckpointDistances.Reset();
		for (int32 Index = 0; Index < FallbackCheckpointCount; ++Index)
		{
			Params.CheckpointDistances.Add(LapLength * Index / FallbackCheckpointCount);
		}
		UE_LOG(LogCosmicRace, Warning, TEXT("Using %d evenly spaced checkpoints instead of the level's markers."), FallbackCheckpointCount);
	}
	else
	{
		UE_LOG(LogCosmicRace, Log, TEXT("Checkpoints: %d markers on the track line, all within %.0f cm."), Markers.Num(), Tolerance);
	}
}

void URaceTrackerComponent::RegisterNewVehicles()
{
	for (TActorIterator<ARaceVehicle> It(GetWorld()); It; ++It)
	{
		ARaceVehicle* Vehicle = *It;
		if (FindRacerId(Vehicle) != INDEX_NONE)
		{
			continue;
		}

		FScopeLock ScopeLock(&Lock);
		Vehicles.Add(Vehicle);
		PendingHumanFlags.Add(Vehicle->IsPlayerControlled());
	}
}

int32 URaceTrackerComponent::FindRacerId(const ARaceVehicle* Vehicle) const
{
	FScopeLock ScopeLock(&Lock);
	for (int32 Id = 0; Id < Vehicles.Num(); ++Id)
	{
		if (Vehicles[Id].Get() == Vehicle)
		{
			return Id;
		}
	}
	return INDEX_NONE;
}

ARaceVehicle* URaceTrackerComponent::GetRacerVehicle(int32 RacerId) const
{
	FScopeLock ScopeLock(&Lock);
	return Vehicles.IsValidIndex(RacerId) ? Vehicles[RacerId].Get() : nullptr;
}

FString URaceTrackerComponent::GetRacerName(int32 RacerId) const
{
	const ARaceVehicle* Vehicle = GetRacerVehicle(RacerId);
	return Vehicle ? Vehicle->GetName() : FString::Printf(TEXT("Racer%d"), RacerId);
}

void URaceTrackerComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);

	if (!Track.IsValid())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 LapsOverride = CosmicHotshots::RaceCVars::Laps.GetValueOnAnyThread();
	if (LapsOverride > 0)
	{
		Params.NumLaps = LapsOverride;
	}
#endif

	// Adopt drivers registered since the last step, then read every body's position.
	{
		FScopeLock ScopeLock(&Lock);
		for (const bool bHuman : PendingHumanFlags)
		{
			FRaceTrackerSim::AddRacer(StepState, bHuman);
		}
		PendingHumanFlags.Reset();

		StepLocations.Reset(Vehicles.Num());
		for (const TWeakObjectPtr<ARaceVehicle>& Vehicle : Vehicles)
		{
			StepLocations.Add(ReadBodyLocation(Vehicle.Get()));
		}
	}

	StepEvents.Reset();
	FRaceTrackerSim::Step(StepState, *Track, Params, StepLocations, DeltaTime, StepEvents);

	LogStep(StepEvents);

	// Keep the two newest step views for other components' physics steps.
	Swap(NewestView, OlderView);
	NewestView.Racers.SetNum(StepState.Racers.Num());
	NewestView.NumInRace = 0;
	for (int32 Id = 0; Id < StepState.Racers.Num(); ++Id)
	{
		const FRacerSimState& Racer = StepState.Racers[Id];
		FRaceStepRacer& View = NewestView.Racers[Id];
		View.bInRace = Racer.bInitialised && Racer.bActive;
		View.bStarted = Racer.bStarted;
		View.bFinished = Racer.bFinished;
		View.Rank = Racer.Rank;
		View.LapsCompleted = Racer.LapsCompleted;
		View.Progress = Racer.Progress;
		View.GapToCarAhead = Racer.GapToCarAhead;
		NewestView.NumInRace += View.bInRace ? 1 : 0;
	}
	NewestView.Events = StepEvents;
	NewestView.LeaderToSecondGap = StepState.LeaderToSecondGap;
	NewestView.RaceTime = StepState.RaceTime;
	NewestView.bRaceFinished = StepState.bRaceFinished;
	NewestViewSimTime = SimTime;
	bHasView = true;

	FScopeLock ScopeLock(&Lock);
	PublishedSnapshot.Racers = StepState.Racers;
	PublishedSnapshot.RankOrder = StepState.RankOrder;
	PublishedSnapshot.LeaderToSecondGap = StepState.LeaderToSecondGap;
	PublishedSnapshot.RaceTime = StepState.RaceTime;
	PublishedSnapshot.NumLaps = Params.NumLaps;
	PublishedSnapshot.bRaceFinished = StepState.bRaceFinished;
	PublishedEvents.Append(StepEvents);
	bSnapshotChanged = true;
}

void URaceTrackerComponent::LogStep(const TArray<FRaceEvent>& Events)
{
#if !UE_BUILD_SHIPPING
	for (const FRaceEvent& Event : Events)
	{
		switch (Event.Type)
		{
		case ERaceEventType::LapCompleted:
			UE_LOG(LogCosmicRace, Log, TEXT("LAP %s t=%.4f lap=%d lapTime=%.4f"),
				*GetRacerName(Event.Racer), Event.Time, Event.Lap, Event.LapTime);
			break;
		case ERaceEventType::RacerFinished:
			UE_LOG(LogCosmicRace, Log, TEXT("FINISH %s t=%.4f place=%d"),
				*GetRacerName(Event.Racer), Event.Time, Event.Place);
			break;
		case ERaceEventType::Overtake:
			UE_LOG(LogCosmicRace, Log, TEXT("OVERTAKE %s passed %s t=%.4f lateral=%.1f gap=%.1f close=%d closeness=%.3f"),
				*GetRacerName(Event.Racer), *GetRacerName(Event.Other), Event.Time,
				Event.Closeness.LateralDistance, Event.Closeness.GapAtConfirm, Event.Closeness.bClose ? 1 : 0, Event.Closeness.Closeness);
			break;
		case ERaceEventType::RaceFinished:
			UE_LOG(LogCosmicRace, Log, TEXT("RACE_FINISHED t=%.4f"), Event.Time);
			break;
		}
	}

	// Periodic state lines, on the same interval as the vehicle log (ch.Debug.VehicleLog).
	const float LogInterval = CosmicHotshots::VehicleCVars::LogInterval.GetValueOnAnyThread();
	if (LogInterval > 0.f && StepState.RaceTime >= NextLogTime)
	{
		NextLogTime = StepState.RaceTime + LogInterval;
		for (const int32 Id : StepState.RankOrder)
		{
			const FRacerSimState& Racer = StepState.Racers[Id];
			if (!Racer.bInitialised)
			{
				continue;
			}
			UE_LOG(LogCosmicRace, Log, TEXT("RACE %s t=%.3f rank=%d lap=%d cp=%d progress=%.1f dist=%.1f lateral=%.1f gapAhead=%.1f gapAheadTime=%.3f started=%d finished=%d"),
				*GetRacerName(Id), StepState.RaceTime, Racer.Rank, FRaceTrackerSim::GetCurrentLap(Racer, Params.NumLaps),
				FRaceTrackerSim::GetNextCheckpoint(Racer, Params), Racer.Progress, Racer.TrackDistance, Racer.LateralOffset,
				Racer.GapToCarAhead.DistanceCm, Racer.GapToCarAhead.TimeSeconds, Racer.bStarted ? 1 : 0, Racer.bFinished ? 1 : 0);
		}
	}
#endif
}

const FRaceStepView& URaceTrackerComponent::GetViewForStep(float SimTime) const
{
	// Tracker already stepped in this physics step: its newest view is of the current step, so hand out the one before.
	return (bHasView && NewestViewSimTime == SimTime) ? OlderView : NewestView;
}

bool URaceTrackerComponent::TakeUpdate(FRaceTrackerSnapshot& OutSnapshot, TArray<FRaceEvent>& OutEvents)
{
	FScopeLock ScopeLock(&Lock);
	OutEvents = MoveTemp(PublishedEvents);
	PublishedEvents.Reset();
	if (!bSnapshotChanged)
	{
		return false;
	}
	OutSnapshot = PublishedSnapshot;
	bSnapshotChanged = false;
	return true;
}

void URaceTrackerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Track.IsValid())
	{
		return;
	}

	// Vehicles are possessed right after they spawn, so the human/AI flag is right by the first tick.
	RegisterNewVehicles();

#if !UE_BUILD_SHIPPING
	if (CosmicHotshots::RaceCVars::DebugTrack.GetValueOnGameThread() != 0)
	{
		DrawDebug();
	}
#endif
}

void URaceTrackerComponent::DrawDebug() const
{
	const UWorld* World = GetWorld();
	const float LapLength = Track->GetLapLength();

	for (float Distance = 0.f; Distance < LapLength; Distance += DebugLineStep)
	{
		const FTrackFrame From = Track->GetFrameAtDistance(Distance);
		const FTrackFrame To = Track->GetFrameAtDistance(FMath::Min(Distance + DebugLineStep, LapLength));
		DrawDebugLine(World, From.Location, To.Location, FColor::Cyan, false, -1.f, 0, DebugLineThickness);
	}

	for (int32 Index = 0; Index < Params.CheckpointDistances.Num(); ++Index)
	{
		const FTrackFrame Frame = Track->GetFrameAtDistance(Params.CheckpointDistances[Index]);
		DrawDebugLine(World, Frame.Location, Frame.Location + FVector::UpVector * DebugCheckpointHeight,
			Index == 0 ? FColor::White : FColor::Yellow, false, -1.f, 0, DebugLineThickness);
	}

	FScopeLock ScopeLock(&Lock);
	for (int32 Id = 0; Id < Vehicles.Num() && Id < PublishedSnapshot.Racers.Num(); ++Id)
	{
		const ARaceVehicle* Vehicle = Vehicles[Id].Get();
		if (!Vehicle)
		{
			continue;
		}
		const FTrackFrame Frame = Track->GetFrameAtDistance(PublishedSnapshot.Racers[Id].TrackDistance);
		DrawDebugLine(World, Vehicle->GetActorLocation(), Frame.Location, FColor::Magenta, false, -1.f, 0, DebugLineThickness);
		DrawDebugPoint(World, Frame.Location, DebugMarkerSize, FColor::Magenta, false, -1.f);
	}
}
