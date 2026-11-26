// Adventure Mode state: the save slot in use, the progress, the unlock rules and starting an Adventure race.
//
// A GameInstance subsystem, so it survives the level changes between hub and races. The hub, the portals
// and the menus only ask and tell this subsystem; none of them holds progress. A race is started the same
// way as a Single Race (FRaceSessionConfig through UGameFlowSubsystem), and the one end-of-race event
// comes back here (HandleRaceFinished) to unlock the next level.

#pragma once

#include "CoreMinimal.h"
#include "Race/RaceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AdventureSubsystem.generated.h"

class UAdventureCampaignDefinition;
class UAdventureLevelDefinition;
class UAdventureSaveGame;

/** What the hub shows after an Adventure race. */
USTRUCT()
struct FAdventureRaceOutcome
{
	GENERATED_BODY()

	/** INDEX_NONE = no Adventure race has finished since the hub last asked. */
	int32 LevelIndex = INDEX_NONE;
	int32 Place = 0;
	bool bWon = false;

	/** The win opened this level for the first time (INDEX_NONE = nothing new: a replay, or the last level). */
	int32 UnlockedLevelIndex = INDEX_NONE;

	/** The win opened the gate of this section (INDEX_NONE = no gate opened). */
	int32 OpenedSectionIndex = INDEX_NONE;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UAdventureSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Save slots the Adventure menu offers. */
	static constexpr int32 NumSaveSlots = 3;

	static UAdventureSubsystem* Get(const UObject* WorldContext);

	/** The project's campaign (Project Settings > Game > Cosmic Hotshots > Adventure Campaign). Null when not set. */
	const UAdventureCampaignDefinition* GetCampaign() const;

	// --- save slots

	bool DoesSlotExist(int32 SlotIndex) const;

	/** Levels won in a slot, without making it the active one. 0 for an empty slot. */
	int32 GetSlotLevelsWon(int32 SlotIndex) const;

	/** Starts a fresh adventure in the slot (whatever was there is replaced) and makes it the active one. */
	void StartNewGame(int32 SlotIndex);

	/** Makes an existing slot the active one. False when the slot is empty. */
	bool LoadGame(int32 SlotIndex);

	bool HasActiveGame() const { return ActiveSave != nullptr; }
	int32 GetActiveSlot() const { return ActiveSlot; }

	// --- progress and unlock rules (of the active game)

	bool IsLevelWon(int32 LevelIndex) const;

	/** Open to race: the first level, or the level before it is won. ch.Adventure.UnlockAll opens everything. */
	bool IsLevelUnlocked(int32 LevelIndex) const;

	/** The gate is open: the first section, or every level of the section before is won. */
	bool IsSectionOpen(int32 SectionIndex) const;

	int32 GetNumLevelsWon() const;

	bool IsIntroSeen() const;
	void MarkIntroSeen();
	bool IsSectionIntroSeen(int32 SectionIndex) const;
	void MarkSectionIntroSeen(int32 SectionIndex);

	/** Dev: forgets everything the active game has won and seen. */
	void ResetProgress();

	// --- races

	/**
	 * Starts the race of a level: the campaign's player character against the level's rivals, on the level's
	 * track. False (and nothing happens) when there is no active game, no such level, or the level is locked.
	 */
	bool StartLevel(const UObject* WorldContext, int32 LevelIndex);

	/** The level of the running (or just finished) Adventure race. INDEX_NONE in any other race. */
	int32 GetCurrentLevel() const { return CurrentLevelIndex; }

	/** Any other kind of race is about to start, or the player left for the main menu. */
	void ClearCurrentLevel() { CurrentLevelIndex = INDEX_NONE; }

	/** The single end-of-race event. Does nothing unless an Adventure race is running. */
	void HandleRaceFinished(const FRaceResultData& Results);

	/** Result of the last Adventure race, for the hub's unlock feedback. Asking clears it. */
	FAdventureRaceOutcome ConsumeLastOutcome();

private:
	FString GetSlotName(int32 SlotIndex) const;
	void SaveActiveGame() const;

	UPROPERTY(Transient)
	TObjectPtr<UAdventureSaveGame> ActiveSave;

	int32 ActiveSlot = INDEX_NONE;
	int32 CurrentLevelIndex = INDEX_NONE;
	FAdventureRaceOutcome LastOutcome;
};
