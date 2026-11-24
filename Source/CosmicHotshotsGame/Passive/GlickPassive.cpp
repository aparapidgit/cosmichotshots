#include "Passive/GlickPassive.h"

#include "Vehicle/VehicleModifierStack.h"

namespace
{
	const FName GlickSource(TEXT("Passive.Glick"));
}

TSubclassOf<UPassiveBase> UGlickPassiveDefinition::GetPassiveClass() const
{
	return UGlickPassive::StaticClass();
}

void UGlickPassive::Step(const FPassiveStepContext& Context)
{
	if (bApplied)
	{
		return;
	}
	bApplied = true;

	const UGlickPassiveDefinition* Tuning = CastChecked<UGlickPassiveDefinition>(Definition);

	// Constant for as long as the passive is on: no fades, so other cars always read the same value.
	FVehicleModifierSpec Spec;
	Spec.SourceId = GlickSource;
	Spec.Stat = EVehicleStat::SideRamPower;
	Spec.Value = Tuning->SideRamPowerMultiplier;
	Context.Modifiers->AddOrRefresh(Spec);

	Spec.Stat = EVehicleStat::SideSpeedLoss;
	Spec.Value = Tuning->SideSpeedLossMultiplier;
	Context.Modifiers->AddOrRefresh(Spec);
}

void UGlickPassive::Deactivate(FVehicleModifierStack& Modifiers)
{
	bApplied = false;
	Modifiers.Remove(GlickSource);
}

FString UGlickPassive::GetDebugString() const
{
	return FString::Printf(TEXT("Glick side contact: %s"), bApplied ? TEXT("ON (pushes harder, loses less)") : TEXT("off"));
}

FString UGlickPassive::GetLogState() const
{
	return FString::Printf(TEXT("Glick active=%d"), bApplied ? 1 : 0);
}
