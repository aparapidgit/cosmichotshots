#include "Passive/Slot8Passive.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName Slot8Source(TEXT("Passive.Slot8"));
}

TSubclassOf<UPassiveBase> USlot8PassiveDefinition::GetPassiveClass() const
{
	return USlot8Passive::StaticClass();
}

void USlot8Passive::Step(const FPassiveStepContext& Context)
{
	const USlot8PassiveDefinition* Tuning = CastChecked<USlot8PassiveDefinition>(Definition);

	EMode Wanted = EMode::Off;
	if (Context.Race && Context.Race->Racers.IsValidIndex(Context.RacerId))
	{
		const FRaceStepRacer& Self = Context.Race->Racers[Context.RacerId];
		if (Self.bInRace && Self.bStarted && !Self.bFinished)
		{
			if (Context.Race->NumInRace >= Tuning->MinRacersForTopRule)
			{
				Wanted = Self.Rank <= Tuning->TopPlaces ? EMode::TopPlaces : EMode::Off;
			}
			else
			{
				Wanted = Self.Rank == Tuning->SmallFieldPlace ? EMode::SmallField : EMode::Off;
			}
		}
	}

	if (Wanted == Mode)
	{
		PendingTime = 0.f;
		return;
	}
	PendingTime += Context.DeltaTime;
	if (PendingTime < Tuning->SwitchDelay)
	{
		return;
	}
	Mode = Wanted;
	PendingTime = 0.f;

	if (Mode == EMode::Off)
	{
		Context.Modifiers->Remove(Slot8Source);
		return;
	}

	const bool bTop = Mode == EMode::TopPlaces;
	FVehicleModifierSpec Spec;
	Spec.SourceId = Slot8Source;
	Spec.FadeIn = Tuning->Fade;
	Spec.FadeOut = Tuning->Fade;

	Spec.Stat = EVehicleStat::HitRecoveryTime;
	Spec.Value = bTop ? Tuning->TopRecoveryTimeMultiplier : Tuning->SmallFieldRecoveryTimeMultiplier;
	Context.Modifiers->AddOrRefresh(Spec);

	Spec.Stat = EVehicleStat::HitSpeedLoss;
	Spec.Value = bTop ? Tuning->TopSpeedLossMultiplier : Tuning->SmallFieldSpeedLossMultiplier;
	Context.Modifiers->AddOrRefresh(Spec);
}

void USlot8Passive::Deactivate(FVehicleModifierStack& Modifiers)
{
	Mode = EMode::Off;
	PendingTime = 0.f;
	Modifiers.Remove(Slot8Source);
}

FString USlot8Passive::GetDebugString() const
{
	const TCHAR* ModeText = Mode == EMode::TopPlaces ? TEXT("ACTIVE (top places)") : Mode == EMode::SmallField ? TEXT("ACTIVE (2nd, small field)") : TEXT("off");
	return FString::Printf(TEXT("Slot8 defend: %s"), ModeText);
}

FString USlot8Passive::GetLogState() const
{
	return FString::Printf(TEXT("Slot8 mode=%d"), static_cast<int32>(Mode));
}
