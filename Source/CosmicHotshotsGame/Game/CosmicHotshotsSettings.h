// Project settings for Cosmic Hotshots (Project Settings > Game > Cosmic Hotshots).
// Holds the default data assets that C++ classes need, so no asset path is hard-coded.
// Stored in Config/DefaultGame.ini.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CosmicHotshotsSettings.generated.h"

class UAIDifficultyData;
class UCameraTuningData;
class UCharacterDefinition;
class UDrivingInputConfig;
class UItemLootTable;
class URaceTuningData;
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

	/** Most vehicles on the grid, humans and computer drivers together. Menus, race setup, grid and AI all use this one value. */
	static constexpr int32 MaxGridSize = 8;

	/** Difficulty by index, clamped to the list; the class defaults of UAIDifficultyData when the list is empty. */
	const UAIDifficultyData* GetAIDifficulty(int32 Index) const;

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

	/** Race rules (laps, overtake detection, gaps). Empty = class defaults of URaceTuningData. */
	UPROPERTY(Config, EditAnywhere, Category = "Race")
	TSoftObjectPtr<URaceTuningData> RaceTuning;

	/** Which items the item boxes give. Empty = boxes give nothing. */
	UPROPERTY(Config, EditAnywhere, Category = "Items")
	TSoftObjectPtr<UItemLootTable> ItemLootTable;

	/** Difficulty levels of computer drivers, easiest first. */
	UPROPERTY(Config, EditAnywhere, Category = "AI")
	TArray<TSoftObjectPtr<UAIDifficultyData>> AIDifficulties;

	/** Index into AIDifficulties used when a race does not say otherwise. */
	UPROPERTY(Config, EditAnywhere, Category = "AI", meta = (ClampMin = "0"))
	int32 DefaultAIDifficulty = 1;

	/** Input actions and bindings used while driving. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UDrivingInputConfig> DrivingInput;
};
