#include "AI/RaceAIController.h"

#include "AI/AIDifficultyData.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Item/ItemBox.h"
#include "Item/ItemSystemComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Math/RandomStream.h"
#include "Race/RaceGameState.h"
#include "Race/RaceTrackerComponent.h"
#include "Race/TrackSplineActor.h"
#include "Race/TrackSplineTable.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/HoverSim.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierStack.h"
#include "Vehicle/VehicleStatsComponent.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	TAutoConsoleVariable<int32> CVarRubberBanding(
		TEXT("ch.AI.RubberBanding"), 1,
		TEXT("0 = switch off AI rubber banding (the catch-up bonus and the slow-down for computer drivers)."), ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarDebugAI(
		TEXT("ch.Debug.AI"), 0,
		TEXT("1 = draw every computer driver's aim point, target speed, state and rubber-band factor."), ECVF_Cheat);

	const FName RubberBandSource(TEXT("AI.RubberBand"));

	constexpr float AIKmhToCmPerSecond = 100000.f / 3600.f;
	constexpr float AICmPerSecondToKmh = 1.f / AIKmhToCmPerSecond;

	/** The car's centre stays this far (cm) from the walls: half a car width plus a safety margin. */
	constexpr float AIWallMargin = 250.f;

	// Debug drawing only.
	constexpr float AIDebugPointSize = 14.f;
	constexpr float AIDebugTextHeight = 220.f;
}

ARaceAIController::ARaceAIController()
{
	PrimaryActorTick.bCanEverTick = true;

}

void ARaceAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Computer drivers get a player state too, so standings and results treat every driver alike.
	if (IsValid(this) && GetWorld() && GetWorld()->IsGameWorld())
	{
		InitPlayerState();
	}
}

void ARaceAIController::Configure(const UAIDifficultyData* InDifficulty, int32 InSeed)
{
	Difficulty = InDifficulty;

	// The same seed always gives the same driver: nothing random happens while driving.
	FRandomStream Stream(InSeed);
	PersonalLine = Stream.FRandRange(-1.f, 1.f);
}

const UAIDifficultyData* ARaceAIController::GetDifficulty() const
{
	return Difficulty ? Difficulty.Get() : GetDefault<UAIDifficultyData>();
}

void ARaceAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Track = ATrackSplineActor::FindTable(GetWorld());

	// Where the item boxes are along the track. They exist by now: boxes are placed before the drivers.
	BoxSpots.Reset();
	if (Track.IsValid())
	{
		TArray<const AItemBox*> BoxActors;
		for (TActorIterator<AItemBox> It(GetWorld()); It; ++It)
		{
			BoxActors.Add(*It);
		}
		BoxActors.Sort([](const AItemBox& A, const AItemBox& B) { return A.GetName() < B.GetName(); });
		for (const AItemBox* Box : BoxActors)
		{
			const FTrackProjection Projection = Track->Project(Box->GetActorLocation());
			FRaceAIBoxSpot& Spot = BoxSpots.AddDefaulted_GetRef();
			Spot.TrackDistance = Projection.Distance;
			Spot.LateralOffset = Projection.LateralOffset;
		}
	}
	if (const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(InPawn))
	{
		Vehicle->GetHoverMovement()->SetStepInputSource(this);
	}
}

void ARaceAIController::OnUnPossess()
{
	if (const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn()))
	{
		Vehicle->GetHoverMovement()->ClearStepInputSource(this);
	}
	RacerId = INDEX_NONE;

	Super::OnUnPossess();
}

void ARaceAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn()))
	{
		Vehicle->GetHoverMovement()->ClearStepInputSource(this);
	}

	Super::EndPlay(EndPlayReason);
}

FRaceAIDriverParams ARaceAIController::BuildParams() const
{
	const UAIDifficultyData* Data = GetDifficulty();
	const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn());

	FRaceAIDriverParams Params;
	if (Vehicle)
	{
		// Final top speed: character, passives and rubber banding included, so the driver uses what the car can do.
		Params.TopSpeed = Vehicle->GetStats()->GetMaxSpeedKmh() * AIKmhToCmPerSecond;
		if (const UVehicleTuningData* Tuning = Vehicle->GetStats()->GetTuning())
		{
			Params.DriftMinSpeed = Tuning->DriftMinSpeedKmh * AIKmhToCmPerSecond;
		}
	}
	if (Track.IsValid())
	{
		Params.UsableHalfWidth = FMath::Max(Track->GetHalfWidth() - AIWallMargin, 0.f);
	}

	Params.TopSpeedFactor = Data->TopSpeedFactor;
	Params.CornerAcceleration = Data->CornerAcceleration;
	Params.PlannedBraking = Data->PlannedBraking;
	Params.ApexOffset = Data->ApexOffset;
	Params.ApexFullCurvature = Data->ApexFullCurvature;
	Params.PersonalOffset = PersonalLine * Data->PersonalLineSpread;
	Params.LineChangeSpeed = Data->LineChangeSpeed;
	Params.SteeringSpeed = Data->SteeringSpeed;
	Params.bUseDrift = Data->bUseDrift;
	Params.DriftMinCurvature = Data->DriftMinCurvature;
	Params.bUseBoost = Data->bUseBoost;
	Params.BoostMinCharge = Data->BoostMinCharge;
	return Params;
}

FVehicleInputState ARaceAIController::GetStepInput(const FVehicleStepInputContext& Context)
{
	if (!Track.IsValid())
	{
		return FVehicleInputState();
	}

	// The other cars, as the race tracker saw them after the previous step (the same for every car,
	// whichever one happens to step first).
	OtherCars.Reset();
	if (!Tracker.IsValid())
	{
		const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
		Tracker = RaceState ? RaceState->GetTracker() : nullptr;
	}
	if (const URaceTrackerComponent* RaceTracker = Tracker.Get())
	{
		if (RacerId == INDEX_NONE)
		{
			RacerId = RaceTracker->FindRacerId(Cast<ARaceVehicle>(GetPawn()));
		}
		const FRaceStepView& Race = RaceTracker->GetViewForStep(Context.SimTime);
		for (int32 Id = 0; Id < Race.Racers.Num(); ++Id)
		{
			const FRaceStepRacer& Racer = Race.Racers[Id];
			if (Id == RacerId || !Racer.bInRace)
			{
				continue;
			}
			FRaceAIOtherCar& Other = OtherCars.AddDefaulted_GetRef();
			Other.TrackDistance = Racer.TrackDistance;
			Other.LateralOffset = Racer.LateralOffset;
			Other.Speed = Racer.ProgressSpeed;
		}
	}

	UpdateRubberBand(Context);

	FRaceAIDriverInput Input;
	Input.Location = Context.Location;
	Input.Rotation = Context.Rotation;
	Input.Velocity = Context.Velocity;
	Input.DeltaTime = Context.DeltaTime;
	Input.ElapsedSimTime = Context.ElapsedSimTime;
	Input.BoostCharge = Context.State ? Context.State->DriftBoost.BoostCharge : 0.f;
	Input.Others = OtherCars;
	// Only boxes that can be collected are worth lining up for (state after the previous step).
	if (!ItemSystem.IsValid())
	{
		const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
		ItemSystem = RaceState ? RaceState->GetItemSystem() : nullptr;
	}
	if (const UItemSystemComponent* Items = ItemSystem.Get())
	{
		const TArray<bool>& BoxesActive = Items->GetBoxesActiveForStep(Context.SimTime);
		for (int32 BoxIndex = 0; BoxIndex < BoxSpots.Num(); ++BoxIndex)
		{
			BoxSpots[BoxIndex].bActive = BoxesActive.IsValidIndex(BoxIndex) && BoxesActive[BoxIndex];
		}
	}
	Input.Boxes = BoxSpots;

	return FRaceAIDriverSim::Step(*Track, BuildParams(), Input, DriverState);
}

void ARaceAIController::UpdateRubberBand(const FVehicleStepInputContext& Context)
{
	const UAIDifficultyData* Data = GetDifficulty();
	const URaceTrackerComponent* RaceTracker = Tracker.Get();
	RubberBandMultiplier = 1.f;
	if (!Context.Modifiers)
	{
		return;
	}

	// The best human still racing; rubber banding follows the gap to that car.
	bool bHumanFound = false;
	double BestHumanProgress = 0.0;
	double OwnProgress = 0.0;
	bool bOwnRacing = false;
	if (RaceTracker && CVarRubberBanding.GetValueOnAnyThread() != 0)
	{
		const FRaceStepView& Race = RaceTracker->GetViewForStep(Context.SimTime);
		for (int32 Id = 0; Id < Race.Racers.Num(); ++Id)
		{
			const FRaceStepRacer& Racer = Race.Racers[Id];
			if (Id == RacerId)
			{
				OwnProgress = Racer.Progress;
				bOwnRacing = Racer.bInRace && Racer.bStarted && !Racer.bFinished;
			}
			else if (Racer.bHuman && Racer.bInRace && !Racer.bFinished && (!bHumanFound || Racer.Progress > BestHumanProgress))
			{
				bHumanFound = true;
				BestHumanProgress = Racer.Progress;
			}
		}
	}

	if (!bHumanFound || !bOwnRacing)
	{
		if (Context.Modifiers->HasSource(RubberBandSource))
		{
			Context.Modifiers->Remove(RubberBandSource);
		}
		return;
	}

	// Positive gap = the human is ahead of this driver.
	const float Gap = static_cast<float>(BestHumanProgress - OwnProgress);
	const float Amount = FMath::Clamp(Gap / Data->RubberBandFullGap, -1.f, 1.f);
	RubberBandMultiplier = 1.f + (Amount >= 0.f ? Amount * Data->RubberBandCatchUp : Amount * Data->RubberBandSlowDown);

	// The gap changes smoothly, so the value needs no fade of its own.
	FVehicleModifierSpec Spec;
	Spec.SourceId = RubberBandSource;
	Spec.Stat = EVehicleStat::MaxSpeed;
	Spec.Value = RubberBandMultiplier;
	Context.Modifiers->AddOrRefresh(Spec);
}

void ARaceAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (CVarDebugAI.GetValueOnGameThread() != 0)
	{
		DrawDebug();
	}
#endif
}

void ARaceAIController::DrawDebug() const
{
	const APawn* Vehicle = GetPawn();
	if (!Vehicle)
	{
		return;
	}

	const TCHAR* ModeText = TEXT("waiting");
	FColor Colour = FColor::White;
	switch (DriverState.Mode)
	{
	case ERaceAIDriverMode::Driving: ModeText = TEXT("driving"); Colour = FColor::Green; break;
	case ERaceAIDriverMode::Braking: ModeText = TEXT("braking"); Colour = FColor::Red; break;
	case ERaceAIDriverMode::Avoiding: ModeText = TEXT("avoiding"); Colour = FColor::Yellow; break;
	case ERaceAIDriverMode::Reversing: ModeText = TEXT("reversing"); Colour = FColor::Magenta; break;
	default: break;
	}

	const UWorld* World = GetWorld();
	DrawDebugLine(World, Vehicle->GetActorLocation(), DriverState.AimPoint, Colour, false, -1.f, 0, 4.f);
	DrawDebugPoint(World, DriverState.AimPoint, AIDebugPointSize, Colour, false, -1.f);
	DrawDebugString(World, Vehicle->GetActorLocation() + FVector::UpVector * AIDebugTextHeight,
		FString::Printf(TEXT("%s  target %.0f km/h  rubber x%.3f"), ModeText, DriverState.TargetSpeed * AICmPerSecondToKmh, RubberBandMultiplier),
		nullptr, Colour, 0.f);
}
