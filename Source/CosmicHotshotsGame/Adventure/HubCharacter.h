// The player's on-foot body in the Adventure hub and the controller that drives it.
//
// AHubCharacter is a plain third-person character: a capsule with a placeholder body in the colour of
// the campaign's player character (Spark) and a spring-arm camera that follows the control rotation.
// AHubPlayerController turns Enhanced Input into walking, running and looking, finds the portal or
// closed gate the character stands at, shows the prompt and the hub HUD, and enters a portal on Interact.
// Neither holds progress: what is open comes from UAdventureSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HubCharacter.generated.h"

class AAdventurePortal;
class UCameraComponent;
class UInputAction;
class UOnFootInputConfig;
class USpringArmComponent;
class UStaticMeshComponent;
class USystemInputConfig;

UCLASS()
class COSMICHOTSHOTSGAME_API AHubCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHubCharacter();

	/** Hold to run: switches between the walk and the run speed of the hub tuning. */
	void SetRunning(bool bRunning);

protected:
	virtual void BeginPlay() override;

private:
	/** Placeholder body until the real character model exists. */
	UPROPERTY(VisibleAnywhere, Category = "Hub")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Hub")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Hub")
	TObjectPtr<UCameraComponent> Camera;
};

UCLASS()
class COSMICHOTSHOTSGAME_API AHubPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void SetupInputComponent() override;

private:
	/** Hub HUD, the unlock message of the last race and the test hooks. Once, in the first tick. */
	void SetUpHub();

	/** Walking, running and the camera from this frame's input. */
	void ApplyMovementInput(float DeltaTime);

	/** Finds the portal or closed gate the character stands at and writes the prompt. */
	void UpdateNearby();

	/** Interact button: enters the portal the character stands at, when it is open. */
	void HandleInteractPressed();

	/** Starts the race of a portal's level, or says why not. */
	void TryEnterPortal(const AAdventurePortal* Portal);

	void HandlePausePressed();

	void ShowMessage(const FText& Message);

	FVector2D GetAxis2D(const UInputAction* Action) const;
	bool GetButton(const UInputAction* Action) const;

	UPROPERTY(Transient)
	TObjectPtr<const UOnFootInputConfig> OnFootInput;

	UPROPERTY(Transient)
	TObjectPtr<const USystemInputConfig> SystemInput;

	UPROPERTY(Transient)
	TObjectPtr<class UHubHudWidget> HudWidget;

	/** Portal within reach right now. */
	UPROPERTY(Transient)
	TObjectPtr<const AAdventurePortal> NearbyPortal;

	bool bHubSetUp = false;

	/** Seconds the current message still stays on screen. */
	float MessageTimeRemaining = 0.f;
};
