#include "Passive/Slot7Passive.h"

#include "Vehicle/HoverSim.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName Slot7Source(TEXT("Passive.Slot7"));
}

TSubclassOf<UPassiveBase> USlot7PassiveDefinition::GetPassiveClass() const
{
	return USlot7Passive::StaticClass();
}

void USlot7Passive::Step(const FPassiveStepContext& Context)
{
	const USlot7PassiveDefinition* Tuning = CastChecked<USlot7PassiveDefinition>(Definition);

	// A wall or car hit: the chain is gone at once, and a drift in progress no longer counts.
	const bool bHit = LastImpactCount >= 0 && Context.ImpactCount != LastImpactCount;
	LastImpactCount = Context.ImpactCount;
	if (bHit)
	{
		bDriftSpoiled = true;
		if (Level > 0)
		{
			Level = 0;
			TimeRemaining = 0.f;
			Context.Modifiers->Remove(Slot7Source, Tuning->HitFadeOut);
		}
	}

	// No new counted drift in time: the chain is over; the modifier decays on its own.
	if (Level > 0)
	{
		TimeRemaining -= Context.DeltaTime;
		if (TimeRemaining <= 0.f)
		{
			Level = 0;
			TimeRemaining = 0.f;
		}
	}

	if (!Context.Vehicle)
	{
		return;
	}

	const FDriftBoostState& Drift = Context.Vehicle->DriftBoost;
	if (Drift.bDrifting)
	{
		if (!bWasDrifting)
		{
			DriftTime = 0.f;
			MaxSlipAngle = 0.f;
			bDriftSpoiled = false;
		}
		DriftTime += Context.DeltaTime;
		MaxSlipAngle = FMath::Max(MaxSlipAngle, FMath::Abs(Context.Vehicle->SlipAngle));
		bDriftSpoiled |= Drift.IsOversliding();
		bWasDrifting = true;
		return;
	}

	if (!bWasDrifting)
	{
		return;
	}
	bWasDrifting = false;

	// The drift just ended. Ending in a loss of control spoils it too.
	const bool bCounts = !bDriftSpoiled && !Drift.IsOversliding()
		&& DriftTime >= Tuning->MinDriftDuration && MaxSlipAngle >= Tuning->MinSlipAngle;
	if (!bCounts)
	{
		return;
	}

	Level = FMath::Min(Level + 1, Tuning->MaxLevel);
	TimeRemaining = Tuning->ChainTimeout;

	FVehicleModifierSpec Spec;
	Spec.SourceId = Slot7Source;
	Spec.Stat = EVehicleStat::MaxSpeed;
	Spec.Value = 1.f + Tuning->MaxSpeedPerLevel * Level;
	Spec.Duration = Tuning->ChainTimeout;
	Spec.FadeIn = Tuning->FadeIn;
	Spec.FadeOut = Tuning->DecayTime;
	Context.Modifiers->AddOrRefresh(Spec);
}

void USlot7Passive::Deactivate(FVehicleModifierStack& Modifiers)
{
	Level = 0;
	TimeRemaining = 0.f;
	bWasDrifting = false;
	Modifiers.Remove(Slot7Source);
}

FString USlot7Passive::GetDebugString() const
{
	return Level > 0
		? FString::Printf(TEXT("Slot7 drift chain: level %d, %.1f s left"), Level, TimeRemaining)
		: FString(TEXT("Slot7 drift chain: off"));
}

FString USlot7Passive::GetLogState() const
{
	return FString::Printf(TEXT("Slot7 level=%d"), Level);
}
