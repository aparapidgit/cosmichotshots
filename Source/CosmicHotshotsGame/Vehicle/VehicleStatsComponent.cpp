#include "Vehicle/VehicleStatsComponent.h"

#include "Vehicle/VehicleModifierComponent.h"
#include "Vehicle/VehicleTuningData.h"

UVehicleStatsComponent::UVehicleStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleStatsComponent::SetBase(const UVehicleTuningData* InTuning, const FVehicleStatModifiers& InCharacterModifiers)
{
	Tuning = InTuning;
	CharacterModifiers = InCharacterModifiers;
}

void UVehicleStatsComponent::SetModifierSource(const UVehicleModifierComponent* InModifiers)
{
	ModifierSource = InModifiers;
}

FVehicleStatModifiers UVehicleStatsComponent::GetModifiers() const
{
	FVehicleStatModifiers Combined = CharacterModifiers;
	if (ModifierSource)
	{
		const FVehicleStatModifiers& Stack = ModifierSource->GetStack().GetTotals();
		Combined.MaxSpeed *= Stack.MaxSpeed;
		Combined.Acceleration *= Stack.Acceleration;
		Combined.Steering *= Stack.Steering;
		Combined.Grip *= Stack.Grip;
		Combined.RamPower *= Stack.RamPower;
		Combined.Stability *= Stack.Stability;
	}
	return Combined;
}

float UVehicleStatsComponent::GetMaxSpeedKmh() const
{
	return Tuning ? Tuning->MaxSpeedKmh * GetModifiers().MaxSpeed : 0.f;
}

float UVehicleStatsComponent::GetMassKg() const
{
	return Tuning ? Tuning->MassKg * CharacterModifiers.Mass : 0.f;
}

float UVehicleStatsComponent::GetRamPower() const
{
	return Tuning ? Tuning->RamPower * GetModifiers().RamPower : 1.f;
}

float UVehicleStatsComponent::GetStability() const
{
	return Tuning ? Tuning->Stability * GetModifiers().Stability : 1.f;
}
