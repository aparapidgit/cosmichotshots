// Race game mode: spawns a race vehicle at a PlayerStart. The race rules (laps,
// ranking, overtakes, results) live in ARaceGameState and its URaceTrackerComponent.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RaceGameMode.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API ARaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARaceGameMode();

	/**
	 * Picks grid slots in a fixed order (PlayerStarts sorted by name, one per player),
	 * so every run starts from the same place. Proper grid assignment comes with the race manager.
	 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void Tick(float DeltaSeconds) override;

private:
	/**
	 * Test helper (ch.Test.SpawnDummyAhead): spawns a stationary dummy car ahead of the first player's car.
	 * Runs in the first pre-physics tick, before any physics step, so every frame rate gets the same race.
	 */
	void SpawnTestDummy();

	/**
	 * Test helper (ch.Test.Bots): fills the free grid slots with test bots that drive the test autopilot,
	 * each with its own line, throttle and roster character. Same first-tick timing as the dummy. NOT the race AI.
	 */
	void SpawnTestBots();

	/** Grid slot of a human player (join order; the first player's slot can be overridden with ch.Test.PlayerGridSlot). */
	int32 GetPlayerGridSlot(int32 PlayerIndex, int32 NumSlots) const;

	/** Hides the test track's placeholder cubes for item boxes and, when item boxes are on, puts real AItemBox actors there. */
	void ReplaceItemBoxPlaceholders();

	/** Grid slots (at most MaxGridSize) the human players did not take, front to back. */
	TArray<int32> FindFreeGridSlots();

	/**
	 * Puts computer drivers on the free grid slots: one ARaceVehicle and one ARaceAIController each, with a
	 * character nobody else drives. How many and how good comes from ch.Race.AIOpponents / ch.Race.AIDifficulty
	 * until the race setup menu provides it.
	 */
	void SpawnAIOpponents();

	/** PlayerStarts in grid order (sorted by name). */
	TArray<class APlayerStart*> GetSortedPlayerStarts() const;

	/** Grid slots without a car yet, front to back. Filled once in the first tick. */
	TArray<int32> FreeGridSlots;

	/** The first tick has put the test cars and the computer drivers on the grid. */
	bool bGridFilled = false;
};
