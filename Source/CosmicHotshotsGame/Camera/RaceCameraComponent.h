// Race camera: third-person chase camera for one vehicle.
//
// Follows the car with position and rotation lag, looks partly along the
// direction of travel (so drifts show where the car is going), stays out of
// walls, and widens the FOV with speed and boost on top of the player's base
// FOV. One camera per vehicle, so every local player has their own.
// It contains no gameplay logic; it only reads the vehicle's state.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTuningData.h"
#include "RaceCameraComponent.generated.h"

class UMaterialInstanceDynamic;

UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class COSMICHOTSHOTSGAME_API URaceCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	URaceCameraComponent();

	/** Player's base FOV (horizontal degrees on 16:9). Clamped to the tuning limits. */
	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void SetBaseFov(float InBaseFov);

	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void SetBaseFovPreset(ECameraFovPreset Preset);

	UFUNCTION(BlueprintPure, Category = "Race Camera")
	float GetBaseFov() const;

	/** Player's speed lines strength, 0 (off) to 1 (full). */
	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void SetSpeedLinesScale(float InScale) { SpeedLinesScale = FMath::Clamp(InScale, 0.f, 1.f); }

	/** Player's camera shake strength, 0 (off) to 1 (full). */
	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void SetShakeScale(float InScale) { ShakeScale = FMath::Clamp(InScale, 0.f, 1.f); }

	/** Starts a short shake. Strength is 0..1 (see FVehicleImpactInfo::Strength). */
	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void AddImpactShake(float Strength);

	/** Hold to look behind the car. */
	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void SetLookBack(bool bInLookBack) { bLookBack = bInLookBack; }

	/** Puts the camera at its ideal spot behind the car with no lag (spawn, respawn, teleport). */
	UFUNCTION(BlueprintCallable, Category = "Race Camera")
	void SnapBehindVehicle();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Tuning asset. When empty, the project default camera tuning is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race Camera")
	TObjectPtr<const UCameraTuningData> Tuning;

private:
	const UCameraTuningData* GetActiveTuning() const;

	/** Unit vector the camera should look along: a blend of where the car points and where it moves. */
	FVector ComputeDesiredLookDirection(const UCameraTuningData& ActiveTuning) const;

	void UpdateCamera(float DeltaTime, bool bSnap);

	/** Width / height of this player's viewport (split-screen aware). 16:9 when unknown. */
	float GetViewportAspectRatio() const;

	/** Converts a horizontal FOV on 16:9 to the vertical FOV this camera is driven with, capped for very wide viewports. */
	float ToVerticalFov(float HorizontalFovOn16By9, const UCameraTuningData& ActiveTuning) const;

	/** Player's camera shake strength with the dev console override applied. */
	float GetShakeScale() const;

	/** Player's speed lines strength with the dev console override applied. */
	float GetSpeedLinesScale() const;

	void LogState(float DeltaTime, float HorizontalFov, float Distance);

	/** Base FOV chosen by the player. Negative = use the Normal preset. */
	float BaseFovOverride = -1.f;

	bool bLookBack = false;
	bool bHasSnapped = false;

	// Smoothed state.
	FVector SmoothedPivot = FVector::ZeroVector;
	/** Direction the camera looks along, as pitch and yaw (no roll). */
	FRotator SmoothedLookRotation = FRotator::ZeroRotator;
	float SmoothedFov = 0.f;
	float BoostEffect = 0.f;
	/** 0..1 fraction of the ideal camera arm that is free of obstacles. */
	float ArmFraction = 1.f;

	/** Player's camera shake strength, 0 (off) to 1 (full). Set from Options later. */
	float ShakeScale = 1.f;
	/** Strength (0..1) and time left of the running impact shake. */
	float ShakeStrength = 0.f;
	float ShakeTimeRemaining = 0.f;

	/** Player's speed lines strength, 0 (off) to 1 (full). Set from Options later. */
	float SpeedLinesScale = 1.f;
	float SpeedLinesIntensity = 0.f;

	/** Per-camera instance of the speed lines post-process material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpeedLinesMaterialInstance;

	double NextLogTime = 0.0;
	double ElapsedTime = 0.0;
};
