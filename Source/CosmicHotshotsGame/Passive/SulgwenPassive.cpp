#include "Passive/SulgwenPassive.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName SulgwenSource(TEXT("Passive.Sulgwen"));
}

TSubclassOf<UPassiveBase> USulgwenPassiveDefinition::GetPassiveClass() const
{
	return USulgwenPassive::StaticClass();
}

void USulgwenPassive::Step(const FPassiveStepContext& Context)
{
	const USulgwenPassiveDefinition* Tuning = CastChecked<USulgwenPassiveDefinition>(Definition);

	// The chain timer runs out: the chain is over; the modifiers fade out on their own.
	if (Level > 0)
	{
		TimeRemaining -= Context.DeltaTime;
		if (TimeRemaining <= 0.f)
		{
			Level = 0;
			TimeRemaining = 0.f;
		}
	}

	if (!Context.Race || Context.RacerId == INDEX_NONE)
	{
		return;
	}

	bool bOvertook = false;
	for (const FRaceEvent& Event : Context.Race->Events)
	{
		if (Event.Type != ERaceEventType::Overtake || Event.Racer != Context.RacerId)
		{
			continue;
		}
		if (Tuning->RivalCooldown > 0.f)
		{
			const double* LastTime = LastOvertakeOfRival.Find(Event.Other);
			if (LastTime && Event.Time - *LastTime < Tuning->RivalCooldown)
			{
				continue;
			}
			LastOvertakeOfRival.Add(Event.Other, Event.Time);
		}
		Level = FMath::Min(Level + 1, Tuning->MaxLevel);
		bOvertook = true;
	}

	if (!bOvertook)
	{
		return;
	}
	TimeRemaining = Tuning->BonusDuration;

	FVehicleModifierSpec Spec;
	Spec.SourceId = SulgwenSource;
	Spec.Op = EVehicleModifierOp::Multiply;
	Spec.Duration = Tuning->BonusDuration;
	Spec.FadeIn = Tuning->FadeIn;
	Spec.FadeOut = Tuning->DecayTime;

	Spec.Stat = EVehicleStat::MaxSpeed;
	Spec.Value = 1.f + Tuning->MaxSpeedPerLevel * Level;
	Context.Modifiers->AddOrRefresh(Spec);

	Spec.Stat = EVehicleStat::Acceleration;
	Spec.Value = 1.f + Tuning->AccelerationPerLevel * Level;
	Context.Modifiers->AddOrRefresh(Spec);
}

void USulgwenPassive::Deactivate(FVehicleModifierStack& Modifiers)
{
	Level = 0;
	TimeRemaining = 0.f;
	Modifiers.Remove(SulgwenSource);
}

FString USulgwenPassive::GetDebugString() const
{
	return Level > 0
		? FString::Printf(TEXT("Sulgwen chain: level %d, %.1f s left"), Level, TimeRemaining)
		: FString(TEXT("Sulgwen chain: off"));
}

FString USulgwenPassive::GetLogState() const
{
	return FString::Printf(TEXT("Sulgwen level=%d"), Level);
}
