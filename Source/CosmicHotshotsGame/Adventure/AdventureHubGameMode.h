// Game mode of the Adventure hub map.
//
// The hub map itself is nearly empty (light, sky, a PlayerStart): this game mode BUILDS the placeholder
// hub from the campaign and the hub tuning before the player is spawned. One open room per section in a
// row, walls around, a gate in the wall to the next section and one portal per level. A hub map that
// already has portals placed by hand is left as it is. The map selects this game mode in its World
// Settings (GameMode Override).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AdventureHubGameMode.generated.h"

class UAdventureCampaignDefinition;
class UAdventureHubTuningData;

UCLASS()
class COSMICHOTSHOTSGAME_API AAdventureHubGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAdventureHubGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** The player appears at the hub entrance, or in front of the portal of the race they come back from. */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

private:
	/** Spawns floors, walls, gates and portals for every section of the campaign. */
	void BuildHub(const UAdventureCampaignDefinition& Campaign, const UAdventureHubTuningData& Tuning);

	/** One grey block of the hub: a scaled placeholder cube with collision. */
	void SpawnBlock(const UAdventureHubTuningData& Tuning, const FVector& Centre, const FVector& Size, const FLinearColor& Colour);
};
