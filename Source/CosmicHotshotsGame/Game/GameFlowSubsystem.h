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

	/**
	 * Character of each computer driver, in grid order. Empty (or shorter than NumAIOpponents) = the race
	 * takes the free roster characters in roster order. Whoever builds the config resolves any random choice.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Race")
	TArray<TObjectPtr<const UCharacterDefinition>> AICharacters;

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
	/** The flow subsystem of the game the object lives in. Null without a world or a game instance. */
	static UGameFlowSubsystem* Get(const UObject* WorldContext);

	/**
	 * Leaves the race (or any other map) for the main menu map and forgets the race setup.
	 * Without a main menu map in the project settings, the current map is reloaded instead.
	 * bOpenRaceSetup = the menu opens straight on the Single Race setup ("Change setup" on the results).
	 */
	void OpenMainMenu(const UObject* WorldContext, bool bOpenRaceSetup = false);

	/**
	 * Draws Count characters at random from the race characters nobody in Taken drives (fewer when not enough
	 * are left). Menus call this while they build a race setup, so nothing random is left for the race itself.
	 * OutSeed = the seed used (ch.Race.AICharacterSeed, or a new one every time).
	 */
	static TArray<const UCharacterDefinition*> DrawRivalCharacters(int32 Count, const TArray<const UCharacterDefinition*>& Taken, int32& OutSeed);

	/** Opens the Adventure hub map (after the Adventure menu, and when the player comes back from a race). */
	void OpenAdventureHub(const UObject* WorldContext);

	/** True once after OpenMainMenu(..., true): the main menu asks this when it starts, and then shows the race setup. */
	bool ConsumeRaceSetupRequest();

	/**
	 * The Single Race setup the menus are filling in. It outlives the race, so "Change setup" starts from the
	 * last choices. AICharacters is ignored here: StartSingleRace picks them.
	 */
	FRaceSessionConfig& EditSingleRaceSetup() { return SingleRaceSetup; }

	/**
	 * Turns the Single Race setup into the race config and starts the race. The rivals' characters are drawn
	 * here, at random from the characters the humans did not take, so nothing random is left for the race itself.
	 */
	void StartSingleRace(const UObject* WorldContext);

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

	UPROPERTY(Transient)
	FRaceSessionConfig SingleRaceSetup;

	bool bHasConfig = false;
	bool bRaceSetupRequested = false;
};
