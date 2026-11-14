// Vehicle input: the only way a driver (human or AI) controls an ARaceVehicle.

#pragma once

#include "CoreMinimal.h"
#include "VehicleInputState.generated.h"

/**
 * Driver intent for one vehicle. PlayerController and AIController write it,
 * the vehicle reads it. The vehicle never knows who the driver is.
 */
USTRUCT(BlueprintType)
struct FVehicleInputState
{
	GENERATED_BODY()

	/** 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float Throttle = 0.f;

	/** 0..1, brakes and then reverses */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float Brake = 0.f;

	/** -1 (left) .. +1 (right) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float Steer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bDrift = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bBoost = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bUseItem = false;
};
