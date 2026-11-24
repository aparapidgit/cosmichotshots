// Game flow: the setup of the next race and the result of the last one.
//
// A GameInstance subsystem, so it survives level changes. Menus fill FRaceSessionConfig and
// call StartRace(); the race game mode reads the config when the track map has loaded. Single
// Race and (later) Adventure Mode start races the same way. While no config is set, the race
// falls back to the dev console variables (ch.Race.AIOpponents, ch.Race.Laps, ...), which is
// what the headless tests use.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Race/RaceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFlowSubsystem.generated.h"

class UCharacterDefinition;

/** One race track: what menus show and which map to load. */
UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UTrackDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track")
	FText DisplayName;

	/** Map to open for a race on this track. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath Map;

	/** Laps when the race setup does not say otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "1"))
	int32 DefaultLaps = 3;
};

/** Everything that defines one race. */
USTRUCT(BlueprintType)
struct FRaceSessionConfig
{
	GENERATED_BODY()

	/** Character of each human player, in player order. An empty entry = the project default character. */
	UPROPERTY(BlueprintReadWrite, Category = "Race")
	TArray<TObjectPtr<const UCharacterDefinition>> HumanCharacters;

	/** Computer drivers on the grid. Humans + computer drivers never exceed UCosmicHotshotsSettings::MaxGridSize. */
	UPROPERTY(BlueprintReadWrite, Category = "Race")
	int32 NumAIOpponents = 7;

	/** Index into the project's AI difficulties. Negative = the project default. */
	UPROPERTY(BlueprintReadWrite, Category = "Race")
	int32 AIDifficulty = -1;

	/** 0 = the track's default lap count. */
	UPROPERTY(BlueprintReadWrite, Category = "Race")
	int32 NumLaps = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Race")
	bool bItemBoxes = true;

	/** Null = stay on the current map (dev). */
	UPROPERTY(BlueprintReadWrite, Category = "Race")
	TObjectPtr<const UTrackDefinition> Track;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The setup the next (or the running) race uses. */
	void SetRaceConfig(const FRaceSessionConfig& InConfig);
	void ClearRaceConfig();
	bool HasRaceConfig() const { return bHasConfig; }

	/** True when the player still has to set the race up (no config yet, and menus are not skipped): the race waits. */
	bool IsWaitingForRaceSetup() const;
	const FRaceSessionConfig& GetRaceConfig() const { return Config; }

	/** Laps of the configured race, 0 when the config leaves it to the race tuning. */
	int32 GetConfiguredLaps() const;

	/** Loads the configured track's map (or reloads the current map) and so starts the race. */
	void StartRace(const UObject* WorldContext);

	/** Results of the last finished race (for a results screen that outlives the race level). */
	void SetLastResults(const FRaceResultData& InResults) { LastResults = InResults; }
	const FRaceResultData& GetLastResults() const { return LastResults; }

private:
	UPROPERTY(Transient)
	FRaceSessionConfig Config;

	UPROPERTY(Transient)
	FRaceResultData LastResults;

	bool bHasConfig = false;
};
