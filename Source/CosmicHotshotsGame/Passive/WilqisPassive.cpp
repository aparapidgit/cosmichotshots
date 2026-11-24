#include "Passive/WilqisPassive.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName WilqisSource(TEXT("Passive.Wilqis"));
}

TSubclassOf<UPassiveBase> UWilqisPassiveDefinition::GetPassiveClass() const
{
	return UWilqisPassive::StaticClass();
}

void UWilqisPassive::Step(const FPassiveStepContext& Context)
{
	const UWilqisPassiveDefinition* Tuning = CastChecked<UWilqisPassiveDefinition>(Definition);

	// Not in 1st place, and the car directly ahead is far enough away.
	bool bWanted = false;
	LastGapSeconds = -1.f;
	if (Context.Race && Context.Race->Racers.IsValidIndex(Context.RacerId))
	{
		const FRaceStepRacer& Self = Context.Race->Racers[Context.RacerId];
		const FRaceGap& Gap = Self.GapToCarAhead;
		if (Self.bInRace && Self.bStarted && !Self.bFinished && Self.Rank > 1 && Gap.bValid)
		{
			LastGapSeconds = Gap.TimeSeconds;
			// Hysteresis: on at the larger gap, off only at the smaller one.
			bWanted = Switch.bOn ? Gap.TimeSeconds > Tuning->DeactivateGapSeconds : Gap.TimeSeconds >= Tuning->ActivateGapSeconds;
		}
	}

	if (!Switch.Update(bWanted, Context.DeltaTime, Tuning->SwitchDelay, Tuning->SwitchDelay))
	{
		return;
	}

	if (Switch.bOn)
	{
		FVehicleModifierSpec Spec;
		Spec.SourceId = WilqisSource;
		Spec.Stat = EVehicleStat::MaxSpeed;
		Spec.Value = Tuning->MaxSpeedMultiplier;
		Spec.FadeIn = Tuning->FadeIn;
		Spec.FadeOut = Tuning->FadeOut;
		Context.Modifiers->AddOrRefresh(Spec);
	}
	else
	{
		Context.Modifiers->Remove(WilqisSource);
	}
}

void UWilqisPassive::Deactivate(FVehicleModifierStack& Modifiers)
{
	Switch.Reset();
	Modifiers.Remove(WilqisSource);
}

FString UWilqisPassive::GetDebugString() const
{
	return FString::Printf(TEXT("Wilqis %s: %s (gap %.2f s)"), TEXT("chase"), Switch.bOn ? TEXT("ACTIVE") : TEXT("off"), LastGapSeconds);
}

FString UWilqisPassive::GetLogState() const
{
	return FString::Printf(TEXT("Wilqis active=%d"), Switch.bOn ? 1 : 0);
}
