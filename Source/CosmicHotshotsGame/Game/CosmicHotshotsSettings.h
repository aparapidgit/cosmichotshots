// Project settings for Cosmic Hotshots (Project Settings > Game > Cosmic Hotshots).
// Holds the default data assets that C++ classes need, so no asset path is hard-coded.
// Stored in Config/DefaultGame.ini.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CosmicHotshotsSettings.generated.h"

class UCameraTuningData;
class UCharacterDefinition;
class UDrivingInputConfig;
class UVehicleTuningData;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cosmic Hotshots"))
class COSMICHOTSHOTSGAME_API UCosmicHotshotsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	/**
	 * Finds a roster character by its position in Characters ("0", "1", ...) or by asset name
	 * (case-insensitive, full name or the part after the last underscore). Null when not found.
	 */
	const UCharacterDefinition* FindCharacter(const FString& IndexOrName) const;

	/** Playable characters, in roster order. Menus and dev commands read this list. */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TArray<TSoftObjectPtr<UCharacterDefinition>> Characters;

	/** Character of vehicles that were not given one. Empty = plain base tuning. */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TSoftObjectPtr<UCharacterDefinition> DefaultCharacter;

	/** Tuning used by vehicles whose character has no base tuning of its own. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	TSoftObjectPtr<UVehicleTuningData> DefaultVehicleTuning;

	/** Tuning used by race cameras that have no tuning asset of their own. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	TSoftObjectPtr<UCameraTuningData> CameraTuning;

	/** Input actions and bindings used while driving. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UDrivingInputConfig> DrivingInput;
};
