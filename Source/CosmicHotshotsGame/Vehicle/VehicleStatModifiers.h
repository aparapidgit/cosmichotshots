// Stat modifiers: small multipliers on top of the base vehicle tuning.

#pragma once

#include "CoreMinimal.h"
#include "VehicleStatModifiers.generated.h"

/**
 * Multipliers applied to the base UVehicleTuningData values. 1 = unchanged.
 * A character's permanent stat differences are one of these; it is also the
 * shape that temporary modifiers (passives, items) will combine into later.
 */
USTRUCT(BlueprintType)
struct FVehicleStatModifiers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float MaxSpeed = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float Acceleration = 1.f;

	/** Turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float Steering = 1.f;

	/** Sideways grip, both normal and while drifting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float Grip = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float Mass = 1.f;

	/** Knock-back this car gives in a collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float RamPower = 1.f;

	/** Resistance to knock-back this car receives in a collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.1"))
	float Stability = 1.f;
};
