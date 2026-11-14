// Driving input config: which Enhanced Input assets drive a race vehicle.
// The player controller reads this asset, so input assets are never hard-coded.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DrivingInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UDrivingInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Key and button bindings for driving. Added to the local player while they control a vehicle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 MappingPriority = 0;

	/** Axis1D, 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Throttle;

	/** Axis1D, 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Brake;

	/** Axis1D, -1 (left) .. +1 (right) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Steer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Drift;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Boost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> UseItem;

	/** Controller rumble strength (0..1) for the strongest impact. Weaker hits rumble proportionally less. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rumble", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ImpactRumbleMaxIntensity = 0.8f;

	/** How long an impact rumble lasts (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rumble", meta = (ClampMin = "0.0"))
	float ImpactRumbleDuration = 0.25f;

	/** Hold to look behind the car. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> LookBack;
};
