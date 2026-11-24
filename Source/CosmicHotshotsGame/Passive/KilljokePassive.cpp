#include "Passive/KilljokePassive.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName KilljokeSource(TEXT("Passive.Killjoke"));
}

TSubclassOf<UPassiveBase> UKilljokePassiveDefinition::GetPassiveClass() const
{
	return UKilljokePassive::StaticClass();
}

void UKilljokePassive::Step(const FPassiveStepContext& Context)
{
	const UKilljokePassiveDefinition* Tuning = CastChecked<UKilljokePassiveDefinition>(Definition);

	// Not the leader, and the leader has pulled away from 2nd place.
	bool bWanted = false;
	LastGapSeconds = -1.f;
	if (Context.Race && Context.Race->Racers.IsValidIndex(Context.RacerId))
	{
		const FRaceStepRacer& Self = Context.Race->Racers[Context.RacerId];
		const FRaceGap& Gap = Context.Race->LeaderToSecondGap;
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
		Spec.SourceId = KilljokeSource;
		Spec.Stat = EVehicleStat::MaxSpeed;
		Spec.Value = Tuning->MaxSpeedMultiplier;
		Spec.FadeIn = Tuning->FadeIn;
		Spec.FadeOut = Tuning->FadeOut;
		Context.Modifiers->AddOrRefresh(Spec);
	}
	else
	{
		Context.Modifiers->Remove(KilljokeSource);
	}
}

void UKilljokePassive::Deactivate(FVehicleModifierStack& Modifiers)
{
	Switch.Reset();
	Modifiers.Remove(KilljokeSource);
}

FString UKilljokePassive::GetDebugString() const
{
	return FString::Printf(TEXT("Killjoke %s: %s (gap %.2f s)"), TEXT("leader hunt"), Switch.bOn ? TEXT("ACTIVE") : TEXT("off"), LastGapSeconds);
}

FString UKilljokePassive::GetLogState() const
{
	return FString::Printf(TEXT("Killjoke active=%d"), Switch.bOn ? 1 : 0);
}
