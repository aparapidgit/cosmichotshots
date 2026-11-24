#include "Passive/FreyaPassive.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName FreyaSource(TEXT("Passive.Freya"));
}

TSubclassOf<UPassiveBase> UFreyaPassiveDefinition::GetPassiveClass() const
{
	return UFreyaPassive::StaticClass();
}

void UFreyaPassive::Step(const FPassiveStepContext& Context)
{
	const UFreyaPassiveDefinition* Tuning = CastChecked<UFreyaPassiveDefinition>(Definition);

	if (TimeSinceTrigger >= 0.f)
	{
		TimeSinceTrigger += Context.DeltaTime;
	}
	if (!Context.Race || Context.RacerId == INDEX_NONE)
	{
		return;
	}

	for (const FRaceEvent& Event : Context.Race->Events)
	{
		if (Event.Type != ERaceEventType::Overtake || Event.Racer != Context.RacerId || !Event.Closeness.bClose)
		{
			continue;
		}

		FVehicleModifierSpec Spec;
		Spec.SourceId = FreyaSource;
		Spec.Stat = EVehicleStat::MaxSpeed;
		Spec.Value = Tuning->VictimMaxSpeedMultiplier;
		Spec.Duration = Tuning->Duration;
		Spec.FadeIn = Tuning->FadeIn;
		Spec.FadeOut = Tuning->FadeOut;
		if (Context.QueueModifierOn(Event.Other, Spec))
		{
			++TriggerCount;
			TimeSinceTrigger = 0.f;
		}
	}
}

void UFreyaPassive::Deactivate(FVehicleModifierStack& Modifiers)
{
	// Nothing of hers sits on her own car; rivals already slowed simply run out.
}

FString UFreyaPassive::GetDebugString() const
{
	return TimeSinceTrigger >= 0.f
		? FString::Printf(TEXT("Freya close overtake: %d rivals slowed, last %.1f s ago"), TriggerCount, TimeSinceTrigger)
		: FString(TEXT("Freya close overtake: none yet"));
}

FString UFreyaPassive::GetLogState() const
{
	return FString::Printf(TEXT("Freya slowedRivals=%d"), TriggerCount);
}
