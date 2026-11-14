// Race player controller: turns the local player's Enhanced Input into an
// FVehicleInputState and hands it to the possessed vehicle every frame.
// It contains no driving logic; an AI controller fills the same struct.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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

protected:
	virtual void SetupInputComponent() override;

private:
	float GetAxisValue(const UInputAction* Action) const;
	bool GetButtonValue(const UInputAction* Action) const;

	/** Loaded from project settings (Cosmic Hotshots > Input). */
	UPROPERTY(Transient)
	TObjectPtr<const UDrivingInputConfig> DrivingInput;
};
