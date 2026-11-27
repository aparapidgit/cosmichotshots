#include "Ability/VehicleAbilityComponent.h"

#include "CosmicHotshotsGame.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Race/RaceGameState.h"
#include "Race/RaceTrackerComponent.h"
#include "Race/TrackSplineActor.h"
#include "Race/TrackSplineTable.h"
#include "Vehicle/HoverSim.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	TAutoConsoleVariable<float> CVarUseAbilityEvery(
		TEXT("ch.Test.UseAbilityEvery"), 0.f,
		TEXT("Test helper: the human player's ability button is pressed every this many seconds of race time. 0 = off."), ECVF_Cheat);

	/** Spread of the computer drivers' use delay over the drivers: driver id x this, wrapped into 0..1. */
	constexpr float AbilityAIDelaySpread = 0.37f;

	const TCHAR* AbilityBlockText(EAbilityBlock Block)
	{
		switch (Block)
		{
		case EAbilityBlock::Cooldown: return TEXT("still running or cooling down");
		case EAbilityBlock::NoEnergy: return TEXT("not a whole segment in the meter");
		case EAbilityBlock::Condition: return TEXT("condition not met");
		default: return TEXT("");
		}
	}
}

UVehicleAbilityComponent::UVehicleAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	Track = ATrackSplineActor::FindTable(GetWorld());
	VehicleName = GetNameSafe(GetOwner());
}

void UVehicleAbilityComponent::SetDefinition(const UActiveAbilityDefinition* InDefinition)
{
	if (Ability && Ability->GetDefinition() == InDefinition)
	{
		return;
	}
	Ability = nullptr;
	const TSubclassOf<UActiveAbilityBase> AbilityClass = InDefinition ? InDefinition->GetAbilityClass() : nullptr;
	if (AbilityClass)
	{
		Ability = NewObject<UActiveAbilityBase>(this, AbilityClass);
		Ability->Initialize(InDefinition);
	}

	FScopeLock Lock(&HudLock);
	HudState = FAbilityHudState();
	HudState.bHasAbility = Ability != nullptr;
	HudState.DisplayName = InDefinition ? InDefinition->DisplayName : FText::GetEmpty();
}

void UVehicleAbilityComponent::QueueEffect(const FAbilityRemoteEffect& Effect)
{
	QueuedEffects.Add(Effect);
}

FAbilityHudState UVehicleAbilityComponent::GetHudState() const
{
	FScopeLock Lock(&HudLock);
	return HudState;
}

FVehicleZoneStep UVehicleAbilityComponent::StepAbility(float DeltaTime, float SimTime, const FVector& Location, const FVector& Velocity,
	bool bAbilityPressed, FHoverSimState& State, FVehicleModifierStack* Modifiers, int32 BoostSegments, bool& bOutLegacyPress)
{
	FVehicleZoneStep Result;
	bOutLegacyPress = false;

	// --- What abilities asked of this car in an earlier step: start it now. (Same step = wait for the next one.)
	for (int32 Index = 0; Index < QueuedEffects.Num();)
	{
		const FAbilityRemoteEffect& Effect = QueuedEffects[Index];
		if (Effect.QueuedAtSimTime >= SimTime)
		{
			++Index;
			continue;
		}
		if (Effect.bRelocate && Track.IsValid() && Track->IsValid() && !Result.bTeleport)
		{
			// The same height above the road as now, facing along the track, speed kept.
			const FTrackFrame Here = Track->GetFrameAtDistance(Track->Project(Location).Distance);
			const FTrackFrame There = Track->GetFrameAtDistance(Effect.TrackDistance);
			Result.bTeleport = true;
			Result.TeleportLocation = There.Location + There.Right * Effect.Lateral + FVector(0.0, 0.0, Location.Z - Here.Location.Z);
			Result.TeleportRotation = FRotationMatrix::MakeFromX(There.Direction.GetSafeNormal2D()).ToQuat();
			Result.TeleportVelocity = There.Direction.GetSafeNormal2D() * Velocity.Size2D();
			UE_LOG(LogCosmicRace, Log, TEXT("ABILITY %s moved to dist=%.0f lateral=%.0f"), *VehicleName, Effect.TrackDistance, Effect.Lateral);
		}
		else if (Effect.Duration > 0.f)
		{
			FActivePull& Pull = Pulls.AddDefaulted_GetRef();
			Pull.Acceleration = Effect.Acceleration;
			Pull.Remaining = Effect.Duration;
		}
		QueuedEffects.RemoveAt(Index);
	}
	for (int32 Index = Pulls.Num() - 1; Index >= 0; --Index)
	{
		Result.Acceleration += Pulls[Index].Acceleration;
		Pulls[Index].Remaining -= DeltaTime;
		if (Pulls[Index].Remaining <= 0.f)
		{
			Pulls.RemoveAt(Index);
		}
	}

	// --- This car's own ability.
	if (!Ability)
	{
		// No ability designed for this character: the old placeholder (the physics spends a segment).
		bOutLegacyPress = bAbilityPressed;
		return Result;
	}

	if (!Tracker.IsValid())
	{
		const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
		Tracker = RaceState ? RaceState->GetTracker() : nullptr;
	}
	URaceTrackerComponent* RaceTracker = Tracker.Get();
	if (RaceTracker && RacerId == INDEX_NONE)
	{
		RacerId = RaceTracker->FindRacerId(Cast<ARaceVehicle>(GetOwner()));
	}
	const FRaceStepView* Race = RaceTracker ? &RaceTracker->GetViewForStep(SimTime) : nullptr;

	const UActiveAbilityDefinition* Def = Ability->GetDefinition();
	ActiveRemaining = FMath::Max(ActiveRemaining - DeltaTime, 0.f);
	if (ActiveRemaining <= 0.f)
	{
		CooldownRemaining = FMath::Max(CooldownRemaining - DeltaTime, 0.f);
	}

	FDriftBoostState& Boost = State.DriftBoost;
	const float SegmentSize = FDriftBoostState::MaxBoostCharge / FMath::Max(BoostSegments, 1);
	const bool bEnoughEnergy = Boost.BoostCharge >= SegmentSize - UE_KINDA_SMALL_NUMBER;

	// The ability needs a race it is part of (its place, its rivals).
	const bool bInRace = Race && Race->Racers.IsValidIndex(RacerId) && Race->Racers[RacerId].bInRace
		&& Race->Racers[RacerId].bStarted && !Race->Racers[RacerId].bFinished && Track.IsValid() && Track->IsValid() && Modifiers;
	FAbilityContext Context;
	Context.Race = Race;
	Context.RacerId = RacerId;
	Context.SimTime = SimTime;
	Context.Modifiers = Modifiers;
	Context.Tracker = RaceTracker;
	Context.Track = Track.Get();
	const bool bConditionMet = bInRace && Ability->CanActivate(Context);
	const bool bOffCooldown = ActiveRemaining <= 0.f && CooldownRemaining <= 0.f;
	const bool bCanFire = bOffCooldown && bEnoughEnergy && bConditionMet;

	bool bPress = bAbilityPressed;
	if (bInRace)
	{
		const bool bHuman = Race->Racers[RacerId].bHuman;
#if !UE_BUILD_SHIPPING
		// Test helper: press the human's button at fixed race times.
		const float TestEvery = CVarUseAbilityEvery.GetValueOnAnyThread();
		if (bHuman && TestEvery > 0.f)
		{
			if (NextTestPress <= 0.0)
			{
				NextTestPress = TestEvery;
			}
			if (Race->RaceTime >= NextTestPress)
			{
				NextTestPress += TestEvery;
				bPress = true;
			}
		}
#endif
		// Computer drivers (simple rule until the second AI pass): fire a little after it became possible.
		if (!bHuman)
		{
			AIReadyTime = bCanFire ? AIReadyTime + DeltaTime : 0.f;
			const float Delay = FMath::Lerp(Def->AIUseDelayMin, Def->AIUseDelayMax, FMath::Frac(RacerId * AbilityAIDelaySpread));
			bPress = bCanFire && AIReadyTime >= Delay;
		}
	}

	if (bPress)
	{
		const EAbilityBlock Block = !bOffCooldown ? EAbilityBlock::Cooldown : !bEnoughEnergy ? EAbilityBlock::NoEnergy
			: !bConditionMet ? EAbilityBlock::Condition : EAbilityBlock::None;
		const double RaceTime = Race ? Race->RaceTime : 0.0;
		if (Block == EAbilityBlock::None)
		{
			// It fires: only now is the segment spent.
			const float Before = Boost.BoostCharge;
			Boost.BoostCharge = FMath::Max(Boost.BoostCharge - SegmentSize, 0.f);
			Ability->Activate(Context);
			ActiveRemaining = Def->ActiveTime;
			CooldownRemaining = Def->Cooldown;
			AIReadyTime = 0.f;
			UE_LOG(LogCosmicRace, Log, TEXT("ABILITY %s t=%.4f %s fired, meter %.3f -> %.3f"),
				*VehicleName, RaceTime, *Def->GetName(), Before, Boost.BoostCharge);
		}
		else
		{
			UE_LOG(LogCosmicRace, Log, TEXT("ABILITY %s t=%.4f %s not fired (%s), meter %.3f kept"),
				*VehicleName, RaceTime, *Def->GetName(), AbilityBlockText(Block), Boost.BoostCharge);
		}
	}

	{
		FScopeLock Lock(&HudLock);
		HudState.ActiveRemaining = ActiveRemaining;
		HudState.CooldownRemaining = CooldownRemaining;
		HudState.CooldownTotal = Def->Cooldown;
		HudState.bEnoughEnergy = bEnoughEnergy;
		HudState.bConditionMet = bConditionMet;
	}
	return Result;
}
