// Project settings for Cosmic Hotshots (Project Settings > Game > Cosmic Hotshots).
// Holds the default data assets that C++ classes need, so no asset path is hard-coded.
// Stored in Config/DefaultGame.ini.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CosmicHotshotsSettings.generated.h"

class UDrivingInputConfig;
class UVehicleTuningData;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cosmic Hotshots"))
class COSMICHOTSHOTSGAME_API UCosmicHotshotsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	/** Tuning used by vehicles that have no tuning asset of their own. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	TSoftObjectPtr<UVehicleTuningData> DefaultVehicleTuning;

	/** Input actions and bindings used while driving. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UDrivingInputConfig> DrivingInput;
};
