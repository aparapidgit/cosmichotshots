#include "Race/RaceGameState.h"

#include "CosmicHotshotsGame.h"
#include "Net/UnrealNetwork.h"
#include "Race/RacePlayerState.h"
#include "Race/RaceTrackerComponent.h"
#include "Race/TrackSplineTable.h"
#include "Vehicle/RaceVehicle.h"

ARaceGameState::ARaceGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Tracker = CreateDefaultSubobject<URaceTrackerComponent>(TEXT("RaceTracker"));
}

void ARaceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARaceGameState, RacePhase);
	DOREPLIFETIME(ARaceGameState, NumLaps);
	DOREPLIFETIME(ARaceGameState, Standings);
}

void ARaceGameState::BeginPlay()
{
	Super::BeginPlay();

	RacePhase = Tracker->IsTracking() ? ERacePhase::Racing : ERacePhase::Inactive;
	NumLaps = Tracker->GetParams().NumLaps;
}

void ARaceGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Tracker->IsTracking())
	{
		BroadcastEvents();
	}
}

void ARaceGameState::BroadcastEvents()
{
	FRaceTrackerSnapshot Snapshot;
	TArray<FRaceEvent> Events;
	const bool bChanged = Tracker->TakeUpdate(Snapshot, Events);

	if (bChanged)
	{
		RaceTime = static_cast<float>(Snapshot.RaceTime);
		NumLaps = Snapshot.NumLaps;
		LeaderToSecondGap = Snapshot.LeaderToSecondGap;

		Standings.Reset(Snapshot.RankOrder.Num());
		for (const int32 Id : Snapshot.RankOrder)
		{
			const FRacerSimState& Racer = Snapshot.Racers[Id];
			FRacerStanding& Standing = Standings.AddDefaulted_GetRef();
			Standing.Vehicle = Tracker->GetRacerVehicle(Id);
			Standing.GridIndex = Id;
			Standing.Rank = Racer.Rank;
			Standing.CurrentLap = FRaceTrackerSim::GetCurrentLap(Racer, Snapshot.NumLaps);
			Standing.LapsCompleted = Racer.LapsCompleted;
			Standing.NextCheckpoint = FRaceTrackerSim::GetNextCheckpoint(Racer, Tracker->GetParams());
			Standing.Progress = static_cast<float>(Racer.Progress);
			Standing.TrackDistance = Racer.TrackDistance;
			Standing.LateralOffset = Racer.LateralOffset;
			Standing.GapToCarAhead = Racer.GapToCarAhead;
			Standing.bStarted = Racer.bStarted;
			Standing.bFinished = Racer.bFinished;
			Standing.FinishTime = static_cast<float>(Racer.FinishTime);
			Standing.BestLapTime = Racer.BestLapTime;
			Standing.LapTimes = Racer.LapTimes;
			Standing.bHuman = Racer.bHuman;
		}
		MirrorToPlayerStates();
	}

	for (const FRaceEvent& Event : Events)
	{
		ARaceVehicle* Vehicle = Tracker->GetRacerVehicle(Event.Racer);
		switch (Event.Type)
		{
		case ERaceEventType::LapCompleted:
			OnLapCompleted.Broadcast(Vehicle, Event.Lap, Event.LapTime);
			break;
		case ERaceEventType::RacerFinished:
			OnRacerFinished.Broadcast(Vehicle, Event.Place, static_cast<float>(Event.Time));
			break;
		case ERaceEventType::Overtake:
		{
			ARaceVehicle* Overtaken = Tracker->GetRacerVehicle(Event.Other);
#if !UE_BUILD_SHIPPING
			UE_LOG(LogCosmicRace, Log, TEXT("GS_OVERTAKE %s passed %s close=%d rankNow=%d"),
				*GetNameSafe(Vehicle), *GetNameSafe(Overtaken), Event.Closeness.bClose ? 1 : 0, GetRank(Vehicle));
#endif
			OnOvertake.Broadcast(Vehicle, Overtaken, Event.Closeness);
			break;
		}
		case ERaceEventType::RaceFinished:
			if (!bResultsBroadcast)
			{
				bResultsBroadcast = true;
				RacePhase = ERacePhase::Finished;
				BuildResults();
				Results.RaceTime = static_cast<float>(Event.Time);
#if !UE_BUILD_SHIPPING
				for (const FRaceResultEntry& Entry : Results.Entries)
				{
					UE_LOG(LogCosmicRace, Log, TEXT("RESULT place=%d %s finished=%d time=%.4f bestLap=%.4f laps=%d human=%d playerState=%d"),
						Entry.Place, *GetNameSafe(Entry.Vehicle), Entry.bFinished ? 1 : 0, Entry.TotalTime, Entry.BestLapTime,
						Entry.LapTimes.Num(), Entry.bHuman ? 1 : 0, Entry.PlayerState ? 1 : 0);
				}
#endif
				OnRaceFinished.Broadcast(Results);
			}
			break;
		}
	}
}

void ARaceGameState::BuildResults()
{
	// Standings already list finishers by place, then the others by progress.
	Results = FRaceResultData();
	Results.NumLaps = NumLaps;
	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		const FRacerStanding& Standing = Standings[Index];
		FRaceResultEntry& Entry = Results.Entries.AddDefaulted_GetRef();
		Entry.Vehicle = Standing.Vehicle;
		Entry.PlayerState = Standing.Vehicle ? Standing.Vehicle->GetPlayerState() : nullptr;
		Entry.Character = Standing.Vehicle ? Standing.Vehicle->GetCharacter() : nullptr;
		Entry.Place = Index + 1;
		Entry.bFinished = Standing.bFinished;
		Entry.TotalTime = Standing.bFinished ? Standing.FinishTime : 0.f;
		Entry.BestLapTime = Standing.BestLapTime;
		Entry.LapTimes = Standing.LapTimes;
		Entry.bHuman = Standing.bHuman;
	}
}

void ARaceGameState::MirrorToPlayerStates() const
{
	for (const FRacerStanding& Standing : Standings)
	{
		ARacePlayerState* PlayerState = Standing.Vehicle ? Standing.Vehicle->GetPlayerState<ARacePlayerState>() : nullptr;
		if (PlayerState)
		{
			PlayerState->SetRaceStanding(Standing);
		}
	}
}

const FRacerStanding* ARaceGameState::FindStanding(const ARaceVehicle* Vehicle) const
{
	if (!Vehicle)
	{
		return nullptr;
	}
	return Standings.FindByPredicate([Vehicle](const FRacerStanding& Standing) { return Standing.Vehicle == Vehicle; });
}

int32 ARaceGameState::GetRank(const ARaceVehicle* Vehicle) const
{
	const FRacerStanding* Standing = FindStanding(Vehicle);
	return Standing ? Standing->Rank : 0;
}

ARaceVehicle* ARaceGameState::GetLeader() const
{
	return Standings.IsEmpty() ? nullptr : Standings[0].Vehicle.Get();
}

ARaceVehicle* ARaceGameState::GetCarAhead(const ARaceVehicle* Vehicle) const
{
	const FRacerStanding* Standing = FindStanding(Vehicle);
	if (!Standing)
	{
		return nullptr;
	}
	const int32 Index = static_cast<int32>(Standing - Standings.GetData());
	return Index > 0 ? Standings[Index - 1].Vehicle.Get() : nullptr;
}

FRaceGap ARaceGameState::GetGapToCarAhead(const ARaceVehicle* Vehicle) const
{
	const FRacerStanding* Standing = FindStanding(Vehicle);
	return Standing ? Standing->GapToCarAhead : FRaceGap();
}

FVector ARaceGameState::GetTrackDirectionAt(const FVector& Location) const
{
	const TSharedPtr<const FTrackSplineTable>& Track = Tracker->GetTrack();
	if (!Track.IsValid())
	{
		return FVector::ForwardVector;
	}
	return Track->Project(Location).Direction;
}

bool ARaceGameState::GetTrackFrameAtDistance(float Distance, FTrackFrame& OutFrame) const
{
	const TSharedPtr<const FTrackSplineTable>& Track = Tracker->GetTrack();
	if (!Track.IsValid())
	{
		return false;
	}
	OutFrame = Track->GetFrameAtDistance(Distance);
	return true;
}

float ARaceGameState::GetLapLength() const
{
	const TSharedPtr<const FTrackSplineTable>& Track = Tracker->GetTrack();
	return Track.IsValid() ? Track->GetLapLength() : 0.f;
}
