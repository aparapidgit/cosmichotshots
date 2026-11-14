// Race game mode. Minimal for the Phase 1 prototype: spawns a race vehicle at a
// PlayerStart. Race rules (laps, ranking, results) are added in Phase 2.

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
};
