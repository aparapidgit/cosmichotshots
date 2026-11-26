#include "AI/RaceAIController.h"

#include "AI/AIDifficultyData.h"
#include "CosmicHotshotsGame.h"
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

	TAutoConsoleVariable<float> CVarAILog(
		TEXT("ch.Debug.AILog"), 0.f,
		TEXT("Seconds between AI_STEP log lines of every computer driver (line, aim, wall gap, buttons). 0 = off."), ECVF_Cheat);

	const FName RubberBandSource(TEXT("AI.RubberBand"));

	constexpr float AIKmhToCmPerSecond = 100000.f / 3600.f;
	constexpr float AICmPerSecondToKmh = 1.f / AIKmhToCmPerSecond;

	// The steering-by-speed curve is sampled at this many points over speed / top speed = 0 .. this range.
	constexpr int32 AISteeringSamples = 16;
	constexpr float AISteeringSampleRange = 1.5f;

	// Limits for values bent by a driver's personality.
	constexpr float MinTopSpeedFactor = 0.3f;
	constexpr float MinBoostStartCharge = 0.1f;
	constexpr float MinCornerSteeringShare = 0.2f;

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

	// The same seed always gives the same driver: nothing random happens while driving (mistakes are
	// rolled from MistakeSeed, the lap and the place on the track). The order of the draws is fixed.
	// The seed is scrambled first: neighbouring seeds (grid slots 1, 2, 3 ...) would otherwise give neighbouring drivers.
	FRandomStream Stream(static_cast<int32>(MurmurFinalize32(static_cast<uint32>(InSeed))));
	Personality.LineBias = Stream.FRandRange(-1.f, 1.f);
	Personality.Courage = Stream.FRandRange(-1.f, 1.f);
	Personality.Pace = Stream.FRand();
	Personality.WallHugDraw = Stream.FRand();
	Personality.WallCaution = Stream.FRand();
	Personality.BoostPatience = Stream.FRandRange(-1.f, 1.f);
	Personality.MistakeSeed = static_cast<int32>(Stream.GetUnsignedInt());
	Personality.Aggression = Stream.FRand();
}

const UAIDifficultyData* ARaceAIController::GetDifficulty() const
{
	return Difficulty ? Difficulty.Get() : GetDefault<UAIDifficultyData>();
}

void ARaceAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TakeVehicle(Cast<ARaceVehicle>(InPawn), /*bInOnlyAfterFinish=*/false);
}

void ARaceAIController::DriveAfterFinish(ARaceVehicle* Vehicle)
{
	TakeVehicle(Vehicle, /*bInOnlyAfterFinish=*/true);
}

void ARaceAIController::TakeVehicle(ARaceVehicle* Vehicle, bool bInOnlyAfterFinish)
{
	DrivenVehicle = Vehicle;
	bOnlyAfterFinish = bInOnlyAfterFinish;
	bTookOver = false;
	RacerId = INDEX_NONE;
	VehicleName = GetNameSafe(Vehicle);
	LapStats = FLapStats();
	StatsLap = INDEX_NONE;

	// Who drives this car (not for the stand-by driver of a human's car: it may never drive).
	if (Vehicle && !bInOnlyAfterFinish)
	{
		UE_LOG(LogCosmicRace, Log, TEXT("AI_DRIVER %s line=%.2f courage=%.2f pace=%.2f wallHug=%.2f wallCaution=%.2f boostPatience=%.2f aggression=%.2f"),
			*VehicleName, Personality.LineBias, Personality.Courage, Personality.Pace, Personality.WallHugDraw, Personality.WallCaution, Personality.BoostPatience,
			Personality.Aggression);
	}

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
	SteeringBySpeedSamples.Reset();
	if (Vehicle)
	{
		if (const UVehicleTuningData* Tuning = Vehicle->GetStats()->GetTuning())
		{
			const FRichCurve* SteeringCurve = Tuning->SteeringBySpeed.GetRichCurveConst();
			for (int32 Index = 0; Index < AISteeringSamples; ++Index)
			{
				SteeringBySpeedSamples.Add(SteeringCurve->Eval(AISteeringSampleRange * Index / (AISteeringSamples - 1), 1.f));
			}
		}
		Vehicle->GetHoverMovement()->SetStepInputSource(this);
	}
}

void ARaceAIController::ReleaseVehicle()
{
	if (const ARaceVehicle* Vehicle = DrivenVehicle.Get())
	{
		Vehicle->GetHoverMovement()->ClearStepInputSource(this);
	}
	DrivenVehicle = nullptr;
	RacerId = INDEX_NONE;
}

bool ARaceAIController::WantsControl(const FVehicleStepInputContext& Context)
{
	if (!bOnlyAfterFinish || bTookOver)
	{
		return true;
	}

	// Standing by on a human's car: take over once that car has finished the race (state after the previous step).
	const FRaceStepView* Race = FindRaceView(Context.SimTime);
	bTookOver = Race && Race->Racers.IsValidIndex(RacerId) && Race->Racers[RacerId].bFinished;
	return bTookOver;
}

const FRaceStepView* ARaceAIController::FindRaceView(float SimTime)
{
	if (!Tracker.IsValid())
	{
		const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
		Tracker = RaceState ? RaceState->GetTracker() : nullptr;
	}
	const URaceTrackerComponent* RaceTracker = Tracker.Get();
	if (!RaceTracker)
	{
		return nullptr;
	}
	if (RacerId == INDEX_NONE)
	{
		RacerId = RaceTracker->FindRacerId(DrivenVehicle.Get());
	}
	return &RaceTracker->GetViewForStep(SimTime);
}

void ARaceAIController::OnUnPossess()
{
	ReleaseVehicle();

	Super::OnUnPossess();
}

void ARaceAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseVehicle();

	Super::EndPlay(EndPlayReason);
}

FRaceAIDriverParams ARaceAIController::BuildParams(const FVehicleStepInputContext& Context) const
{
	const UAIDifficultyData* Data = GetDifficulty();
	const ARaceVehicle* Vehicle = DrivenVehicle.Get();

	FRaceAIDriverParams Params;
	if (Vehicle)
	{
		// Final top speed: character, passives and rubber banding included, so the driver uses what the car can do.
		Params.TopSpeed = Vehicle->GetStats()->GetMaxSpeedKmh() * AIKmhToCmPerSecond;
		if (const UVehicleTuningData* Tuning = Vehicle->GetStats()->GetTuning())
		{
			Params.DriftMinSpeed = Tuning->DriftMinSpeedKmh * AIKmhToCmPerSecond;
			Params.BoostTopSpeed = Tuning->BoostMaxSpeedKmh * AIKmhToCmPerSecond;
			Params.DriftCriticalSlipAngle = Tuning->DriftCriticalSlipAngle;
			Params.BodyHalfWidth = Tuning->BodySize.Y * 0.5f;
			Params.DriftMinTurn = Tuning->DriftMinTurn;
			Params.DriftYawMultiplier = Tuning->DriftYawMultiplier;
			Params.DriftGrip = Tuning->DriftGrip * Vehicle->GetStats()->GetModifiers().Grip;

			// Steering: the sim looks up the full-lock turn rate at any speed, like the car's own physics.
			Params.FullYawRate = FMath::DegreesToRadians(Tuning->MaxYawRate) * Vehicle->GetStats()->GetModifiers().Steering;
			Params.SteeringBySpeed = SteeringBySpeedSamples;
			Params.SteeringSampleRange = AISteeringSampleRange;
		}
	}
	if (Track.IsValid())
	{
		Params.TrackHalfWidth = Track->GetHalfWidth();
		Params.UsableHalfWidth = FMath::Max(Track->GetHalfWidth() - Data->WallMargin, 0.f);
	}

	// Difficulty, bent by this driver's personality.
	const float Courage = 1.f + Personality.Courage * Data->CornerCourageSpread;
	Params.TopSpeedFactor = FMath::Max(Data->TopSpeedFactor - Personality.Pace * Data->TopSpeedSpread, MinTopSpeedFactor);
	Params.CornerAcceleration = Data->CornerAcceleration * Courage;
	Params.CornerSteeringShare = FMath::Clamp(Data->CornerSteeringShare * Courage, MinCornerSteeringShare, 1.f);
	Params.PlannedBraking = Data->PlannedBraking * Courage;
	Params.ApexOffset = Data->ApexOffset;
	Params.ApexFullCurvature = Data->ApexFullCurvature;
	Params.PersonalOffset = Personality.LineBias * Data->PersonalLineSpread;
	Params.LineChangeSpeed = Data->LineChangeSpeed;
	Params.SteeringSpeed = Data->SteeringSpeed;
	Params.bUseDrift = Data->bUseDrift;
	Params.DriftMinCurvature = Data->DriftMinCurvature;
	Params.DriftMaxSlipAngle = Data->DriftMaxSlipAngle;
	Params.DriftMinCornerTime = Data->DriftMinCornerTime;
	Params.BoostMaxCurvature = Data->BoostMaxCurvature;
	const bool bHugsWalls = Data->WallHugClearance > 0.f && Personality.WallHugDraw < Data->WallHugShare;
	Params.WallHugClearance = bHugsWalls ? Data->WallHugClearance + Personality.WallCaution * Data->WallHugClearanceSpread : 0.f;
	Params.DefendShift = Data->DefendMaxShift * Personality.Aggression;
	Params.InsidePassShare = 1.f - Personality.Aggression;
	Params.OvertakeBoostCharge = Data->OvertakeBoostCharge;
	Params.MistakeChance = Data->MistakeChance;
	Params.MistakeLiftTime = Data->MistakeLiftTime;
	Params.MistakeWideOffset = Data->MistakeWideOffset;
	Params.MistakeSeed = Personality.MistakeSeed;
	Params.ItemBoxMaxSidestep = Data->ItemBoxMaxSidestep;
	Params.bUseBoost = Data->bUseBoost;
	Params.BoostMinCharge = FMath::Clamp(Data->BoostMinCharge + Personality.BoostPatience * Data->BoostChargeSpread, MinBoostStartCharge, 1.f);
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
			RacerId = RaceTracker->FindRacerId(DrivenVehicle.Get());
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
	UpdateLapStats(Context);

	FRaceAIDriverInput Input;
	Input.Location = Context.Location;
	Input.Rotation = Context.Rotation;
	Input.Velocity = Context.Velocity;
	Input.DeltaTime = Context.DeltaTime;
	Input.ElapsedSimTime = Context.ElapsedSimTime;
	Input.BoostCharge = Context.State ? Context.State->DriftBoost.BoostCharge : 0.f;
	Input.bBoosting = Context.State && Context.State->DriftBoost.bBoosting;
	Input.DriftDirection = Context.State && Context.State->DriftBoost.bDrifting ? Context.State->DriftBoost.DriftDirection : 0.f;
	if (Context.State)
	{
		Input.WallClearanceLeft = Context.State->WallProbes[0].Clearance;
		Input.WallClearanceRight = Context.State->WallProbes[1].Clearance;
	}
	Input.Others = OtherCars;
	if (const URaceTrackerComponent* RaceTracker = Tracker.Get())
	{
		const FRaceStepView& Race = RaceTracker->GetViewForStep(Context.SimTime);
		Input.Lap = Race.Racers.IsValidIndex(RacerId) ? Race.Racers[RacerId].LapsCompleted : 0;
	}
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

	const FVehicleInputState Out = FRaceAIDriverSim::Step(*Track, BuildParams(Context), Input, DriverState);

#if !UE_BUILD_SHIPPING
	const float LogInterval = CVarAILog.GetValueOnAnyThread();
	if (LogInterval > 0.f && Context.ElapsedSimTime >= NextStepLogTime)
	{
		NextStepLogTime = Context.ElapsedSimTime + LogInterval;
		UE_LOG(LogCosmicRace, Log, TEXT("AI_STEP %s t=%.3f dist=%.0f lateral=%.0f target=%.0f lineError=%.0f hug=%d kmh=%.1f targetKmh=%.1f steer=%.2f yawRate=%.1f slip=%.1f throttle=%.2f brake=%.2f drift=%d boost=%d charge=%.3f wallL=%.0f wallR=%.0f mode=%d"),
			*VehicleName, Context.ElapsedSimTime, DriverState.TrackDistance, DriverState.LateralOffset, DriverState.TargetLateral, DriverState.LineError,
			DriverState.bHuggingWall ? 1 : 0, Context.Velocity.Size() * AICmPerSecondToKmh, DriverState.TargetSpeed * AICmPerSecondToKmh,
			Out.Steer, Context.State ? Context.State->YawRate : 0.f, Context.State ? Context.State->SlipAngle : 0.f, Out.Throttle, Out.Brake, Out.bDriftRight ? 1 : (Out.bDriftLeft ? -1 : 0), Out.bBoost ? 1 : 0, Input.BoostCharge,
			Input.WallClearanceLeft, Input.WallClearanceRight, static_cast<int32>(DriverState.Mode));
	}
#endif
	return Out;
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

void ARaceAIController::UpdateLapStats(const FVehicleStepInputContext& Context)
{
#if !UE_BUILD_SHIPPING
	const URaceTrackerComponent* RaceTracker = Tracker.Get();
	if (!RaceTracker || !Context.State)
	{
		return;
	}
	const FRaceStepView& Race = RaceTracker->GetViewForStep(Context.SimTime);
	if (!Race.Racers.IsValidIndex(RacerId) || !Race.Racers[RacerId].bStarted)
	{
		return;
	}

	// A lap was completed in the previous step: report what the car did in it.
	const int32 Lap = Race.Racers[RacerId].LapsCompleted;
	if (StatsLap != INDEX_NONE && Lap != StatsLap)
	{
		UE_LOG(LogCosmicRace, Log, TEXT("AI_LAP %s t=%.4f lap=%d boost=%.2fs boosts=%d drift=%.2fs closePass=%.2fs touches=%d mistakes=%d overslides=%d lineError=%.0f max=%.0f"),
			*VehicleName, Race.RaceTime, Lap, LapStats.BoostSeconds, LapStats.Boosts, LapStats.DriftSeconds, LapStats.ClosePassSeconds, LapStats.Touches, LapStats.Mistakes,
			LapStats.Overslides, LapStats.LineErrorSum / FMath::Max(LapStats.Seconds, UE_SMALL_NUMBER), LapStats.MaxLineError);
		LapStats = FLapStats();
	}

	const FDriftBoostState& DriftBoost = Context.State->DriftBoost;
	if (StatsLap == INDEX_NONE)
	{
		StatsBoostCount = DriftBoost.BoostCount;
		StatsLockout = DriftBoost.ClosePassLockoutRemaining;
		StatsOverslide = DriftBoost.OverslideTimeRemaining;
	}
	StatsLap = Lap;

	LapStats.BoostSeconds += DriftBoost.bBoosting ? Context.DeltaTime : 0.f;
	LapStats.DriftSeconds += DriftBoost.bDrifting ? Context.DeltaTime : 0.f;
	LapStats.ClosePassSeconds += DriftBoost.bClosePassCharging ? Context.DeltaTime : 0.f;
	LapStats.Boosts += DriftBoost.BoostCount - StatsBoostCount;
	StatsBoostCount = DriftBoost.BoostCount;
	// The lockout only counts down; a jump up is a new touch or hit.
	LapStats.Touches += DriftBoost.ClosePassLockoutRemaining > StatsLockout ? 1 : 0;
	LapStats.Mistakes += DriverState.MistakeCount - StatsMistakeCount;
	StatsMistakeCount = DriverState.MistakeCount;
	LapStats.Overslides += DriftBoost.OverslideTimeRemaining > StatsOverslide ? 1 : 0;
	StatsOverslide = DriftBoost.OverslideTimeRemaining;
	const float LineError = FMath::Abs(DriverState.LateralOffset - DriverState.TargetLateral);
	LapStats.MaxLineError = FMath::Max(LapStats.MaxLineError, LineError);
	LapStats.LineErrorSum += LineError * Context.DeltaTime;
	LapStats.Seconds += Context.DeltaTime;
	StatsLockout = DriftBoost.ClosePassLockoutRemaining;
#endif
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
	const ARaceVehicle* Vehicle = DrivenVehicle.Get();
	if (!Vehicle || (bOnlyAfterFinish && !bTookOver))
	{
		return;
	}

	const TCHAR* ModeText = TEXT("waiting");
	FColor Colour = FColor::White;
	switch (DriverState.Mode)
	{
	case ERaceAIDriverMode::Driving: ModeText = TEXT("driving"); Colour = FColor::Green; break;
	case ERaceAIDriverMode::Braking: ModeText = TEXT("braking"); Colour = FColor::Red; break;
	case ERaceAIDriverMode::Avoiding: ModeText = TEXT("overtaking"); Colour = FColor::Yellow; break;
	case ERaceAIDriverMode::Defending: ModeText = TEXT("defending"); Colour = FColor::Cyan; break;
	case ERaceAIDriverMode::Reversing: ModeText = TEXT("reversing"); Colour = FColor::Magenta; break;
	default: break;
	}

	const UWorld* World = GetWorld();
	DrawDebugLine(World, Vehicle->GetActorLocation(), DriverState.AimPoint, Colour, false, -1.f, 0, 4.f);
	DrawDebugPoint(World, DriverState.AimPoint, AIDebugPointSize, Colour, false, -1.f);
	DrawDebugString(World, Vehicle->GetActorLocation() + FVector::UpVector * AIDebugTextHeight,
		FString::Printf(TEXT("%s%s%s  target %.0f km/h  rubber x%.3f\ncourage %+.1f  pace -%.1f  %s  boost %+.1f  aggression %.1f"), ModeText,
			DriverState.bHuggingWall ? TEXT(" WALL") : TEXT(""),
			DriverState.Mistake != ERaceAIMistake::None ? TEXT(" MISTAKE") : TEXT(""),
			DriverState.TargetSpeed * AICmPerSecondToKmh, RubberBandMultiplier,
			Personality.Courage, Personality.Pace,
			GetDifficulty()->WallHugClearance > 0.f && Personality.WallHugDraw < GetDifficulty()->WallHugShare ? TEXT("wall hugger") : TEXT("open line"),
			Personality.BoostPatience, Personality.Aggression),
		nullptr, Colour, 0.f);
}
