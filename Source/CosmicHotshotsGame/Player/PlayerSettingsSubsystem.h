// Player settings: what one local player chose in Options, saved between sessions.
//
// One subsystem per local player (split-screen players each get their own), stored in its own save
// slot "PlayerSettings_<local player index>". Options screens write here; the subsystem saves at once
// and applies the change to that player's race camera, so Options opened from the pause menu previews live.
// Without a window (headless tests) nothing is loaded or saved: tests always run on the defaults.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "PlayerSettingsSubsystem.generated.h"

class APlayerController;
class UCameraTuningData;
class URaceCameraComponent;

/** Strength of an optional effect (camera shake, speed lines). */
UENUM(BlueprintType)
enum class EEffectIntensity : uint8
{
	Off,
	Low,
	Normal,
};

USTRUCT(BlueprintType)
struct FPlayerSettingsData
{
	GENERATED_BODY()

	/** Base FOV (horizontal degrees on 16:9). Negative = the Normal preset of the camera tuning. */
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	float BaseFov = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	EEffectIntensity CameraShake = EEffectIntensity::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	EEffectIntensity SpeedLines = EEffectIntensity::Normal;
};

/** The save file of one local player's settings. */
UCLASS()
class COSMICHOTSHOTSGAME_API UPlayerSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPlayerSettingsData Settings;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UPlayerSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** The settings subsystem of the player behind this controller. Null for controllers without a local player. */
	static UPlayerSettingsSubsystem* Get(const APlayerController* PlayerController);

	/** Loaded from the save slot on first use. */
	const FPlayerSettingsData& GetSettings();

	/** Base FOV in degrees with the "Normal preset" default resolved and the tuning limits applied. */
	float GetBaseFov();

	void SetBaseFov(float InBaseFov);
	void SetCameraShake(EEffectIntensity Intensity);
	void SetSpeedLines(EEffectIntensity Intensity);

	/** Hands the camera settings to a race camera (call when this player gets a vehicle). */
	void ApplyToCamera(URaceCameraComponent* Camera);

	/** The camera tuning the Options screens and the settings use (project default, else class defaults). */
	static const UCameraTuningData* GetCameraTuning();

private:
	void EnsureLoaded();

	/** Saves the settings and applies them to the vehicle this player drives now, if any. */
	void HandleSettingsChanged();

	/** False without a window, or with ch.Settings.Persist 0: the settings stay in memory only. */
	bool ShouldPersist() const;

	FString GetSlotName() const;

	float ToScale(EEffectIntensity Intensity) const;

	UPROPERTY(Transient)
	FPlayerSettingsData Settings;

	bool bLoaded = false;
};
