// Game mode and player controller of the main menu map.
//
// The menu map has no vehicle and no race: the game mode spawns no pawn, and the player controller
// only puts the main menu screen on the viewport. The map selects this game mode in its World Settings
// (GameMode Override); race maps keep the project default, ARaceGameMode.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "MenuGameMode.generated.h"

class URaceScreenWidget;

UCLASS()
class COSMICHOTSHOTSGAME_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMenuGameMode();
};

UCLASS()
class COSMICHOTSHOTSGAME_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void PlayerTick(float DeltaTime) override;

private:
	/**
	 * Shows the main menu. Called once, in the first tick: console variables from the command line are
	 * not there yet in BeginPlay. With ch.Test.AutoMenu the menu acts by itself instead (headless tests).
	 */
	void SetUpMenu();

	/** Test helpers ch.Test.ScreenshotTime / ch.Test.AutoQuitTime, measured from the moment the menu map started. */
	void TickTestHelpers(float DeltaTime);

	bool bMenuSetUp = false;

	/** Seconds since the first tick, for the test helpers. */
	float MenuTime = 0.f;
	bool bMenuScreenshotTaken = false;

	UPROPERTY(Transient)
	TObjectPtr<URaceScreenWidget> MenuWidget;
};
