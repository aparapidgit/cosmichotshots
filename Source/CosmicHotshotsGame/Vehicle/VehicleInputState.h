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

	/** Hold to drift to the left. The button picks the drift direction; steering only shapes the line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bDriftLeft = false;

	/** Hold to drift to the right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bDriftRight = false;

	/** Hold to boost; the boost meter drains while it is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bBoost = false;

	/** Press to use the character's active ability; it spends one whole segment of the boost meter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bUseAbility = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bUseItem = false;
};
