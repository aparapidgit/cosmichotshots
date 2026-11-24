#include "Passive/SparkPassive.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName SparkSource(TEXT("Passive.Spark"));
}

TSubclassOf<UPassiveBase> USparkPassiveDefinition::GetPassiveClass() const
{
	return USparkPassive::StaticClass();
}

void USparkPassive::Step(const FPassiveStepContext& Context)
{
	const USparkPassiveDefinition* Tuning = CastChecked<USparkPassiveDefinition>(Definition);

	// Last place: counts only on the track, after the start line and before the finish.
	bool bLast = false;
	if (Context.Race && Context.Race->Racers.IsValidIndex(Context.RacerId))
	{
		const FRaceStepRacer& Self = Context.Race->Racers[Context.RacerId];
		bLast = Self.bInRace && Self.bStarted && !Self.bFinished
			&& Context.Race->NumInRace >= Tuning->MinRacers
			&& Self.Rank == Context.Race->NumInRace;
	}

	if (bLast == bActive)
	{
		PendingTime = 0.f;
		return;
	}

	PendingTime += Context.DeltaTime;
	if (PendingTime < (bLast ? Tuning->ActivateDelay : Tuning->DeactivateDelay))
	{
		return;
	}

	bActive = bLast;
	PendingTime = 0.f;
	if (bActive)
	{
		FVehicleModifierSpec Spec;
		Spec.SourceId = SparkSource;
		Spec.Stat = EVehicleStat::MaxSpeed;
		Spec.Op = EVehicleModifierOp::Multiply;
		Spec.Value = Tuning->MaxSpeedMultiplier;
		Spec.FadeIn = Tuning->FadeIn;
		Spec.FadeOut = Tuning->FadeOut;
		Context.Modifiers->AddOrRefresh(Spec);
	}
	else
	{
		Context.Modifiers->Remove(SparkSource);
	}
}

void USparkPassive::Deactivate(FVehicleModifierStack& Modifiers)
{
	bActive = false;
	PendingTime = 0.f;
	Modifiers.Remove(SparkSource);
}

FString USparkPassive::GetDebugString() const
{
	return FString::Printf(TEXT("Spark comeback: %s"), bActive ? TEXT("ACTIVE (last place)") : TEXT("off"));
}

FString USparkPassive::GetLogState() const
{
	return FString::Printf(TEXT("Spark active=%d"), bActive ? 1 : 0);
}
