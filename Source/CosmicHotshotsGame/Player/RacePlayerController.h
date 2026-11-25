// Race player controller: turns the local player's Enhanced Input into an
// FVehicleInputState and hands it to the possessed vehicle every frame.
// It contains no driving logic; an AI controller fills the same struct.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Race/RaceTypes.h"
#include "Vehicle/VehicleImpactComponent.h"
#include "RacePlayerController.generated.h"

class UDrivingInputConfig;
class UInputAction;

UCLASS()
class COSMICHOTSHOTSGAME_API ARacePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/**
	 * Dev console command: "SetCharacter 1" or "SetCharacter TestHeavy".
	 * Switches this player's vehicle to a character from the project roster.
	 */
	UFUNCTION(Exec)
	void SetCharacter(const FString& IndexOrName);

	/**
	 * Dev console command: "GiveItem Empty". Puts the item with that id (from the loot table) into this
	 * player's item slot, ready to use.
	 */
	UFUNCTION(Exec)
	void GiveItem(const FString& ItemId);

protected:
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	/** Controller rumble for this player's own impacts. */
	UFUNCTION()
	void HandleVehicleImpact(const FVehicleImpactInfo& ImpactInfo);

	float GetAxisValue(const UInputAction* Action) const;
	bool GetButtonValue(const UInputAction* Action) const;

	/**
	 * Race screens of this player: the start / finish overlay, the character select while the race waits for
	 * its setup, and the results when the race is over. Called once, in the first tick (console variables
	 * from the command line are not there yet in BeginPlay).
	 */
	void SetUpRaceScreens();

	UFUNCTION()
	void HandleRaceFinished(const FRaceResultData& Results);

	/** Pause button: pauses the game and opens the pause menu, while a race is running and no other screen is up. */
	void HandlePausePressed();

	/** Loaded from project settings (Cosmic Hotshots > Input). Pause and other non-driving actions. */
	UPROPERTY(Transient)
	TObjectPtr<const class USystemInputConfig> SystemInput;

	bool bRaceScreensSetUp = false;

	UPROPERTY(Transient)
	TObjectPtr<class URaceOverlayWidget> OverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<class URaceScreenWidget> ScreenWidget;

	/** Loaded from project settings (Cosmic Hotshots > Input). */
	UPROPERTY(Transient)
	TObjectPtr<const UDrivingInputConfig> DrivingInput;
};
