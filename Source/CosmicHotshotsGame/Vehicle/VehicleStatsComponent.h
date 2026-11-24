// Vehicle stats: the single place that answers "what are this vehicle's final values".
//
// Final value = base tuning (UVehicleTuningData) x character stat modifiers x the totals of
// the vehicle's modifier stack (passives, items, abilities). Movement, impacts and camera
// never need to know where a bonus came from.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicle/VehicleStatModifiers.h"
#include "VehicleStatsComponent.generated.h"

class UVehicleModifierComponent;
class UVehicleTuningData;

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class COSMICHOTSHOTSGAME_API UVehicleStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleStatsComponent();

	void SetBase(const UVehicleTuningData* InTuning, const FVehicleStatModifiers& InCharacterModifiers);

	/** The vehicle's modifier stack; its totals multiply into every value below. Optional. */
	void SetModifierSource(const UVehicleModifierComponent* InModifiers);

	const UVehicleTuningData* GetTuning() const { return Tuning; }

	/** Combined multipliers currently applied on top of the base tuning: character x modifier stack. */
	FVehicleStatModifiers GetModifiers() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle Stats")
	float GetMaxSpeedKmh() const;

	/** Mass comes from the character only; the modifier stack never changes it. */
	UFUNCTION(BlueprintPure, Category = "Vehicle Stats")
	float GetMassKg() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle Stats")
	float GetRamPower() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle Stats")
	float GetStability() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UVehicleTuningData> Tuning;

	UPROPERTY(Transient)
	FVehicleStatModifiers CharacterModifiers;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleModifierComponent> ModifierSource;
};
