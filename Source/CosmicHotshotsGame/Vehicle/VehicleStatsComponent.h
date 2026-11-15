// Vehicle stats: the single place that answers "what are this vehicle's final values".
//
// Final value = base tuning (UVehicleTuningData) x character stat modifiers.
// In Phase 2 the shared modifier stack (passives, items) multiplies in here too,
// so movement, impacts and camera never need to know where a bonus came from.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicle/VehicleStatModifiers.h"
#include "VehicleStatsComponent.generated.h"

class UVehicleTuningData;

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class COSMICHOTSHOTSGAME_API UVehicleStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleStatsComponent();

	void SetBase(const UVehicleTuningData* InTuning, const FVehicleStatModifiers& InCharacterModifiers);

	const UVehicleTuningData* GetTuning() const { return Tuning; }

	/** Combined multipliers currently applied on top of the base tuning. */
	const FVehicleStatModifiers& GetModifiers() const { return CharacterModifiers; }

	UFUNCTION(BlueprintPure, Category = "Vehicle Stats")
	float GetMaxSpeedKmh() const;

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
};
