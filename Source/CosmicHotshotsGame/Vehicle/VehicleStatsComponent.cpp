#include "Vehicle/VehicleStatsComponent.h"

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

float UVehicleStatsComponent::GetMaxSpeedKmh() const
{
	return Tuning ? Tuning->MaxSpeedKmh * CharacterModifiers.MaxSpeed : 0.f;
}

float UVehicleStatsComponent::GetMassKg() const
{
	return Tuning ? Tuning->MassKg * CharacterModifiers.Mass : 0.f;
}

float UVehicleStatsComponent::GetRamPower() const
{
	return Tuning ? Tuning->RamPower * CharacterModifiers.RamPower : 1.f;
}

float UVehicleStatsComponent::GetStability() const
{
	return Tuning ? Tuning->Stability * CharacterModifiers.Stability : 1.f;
}
