// Camera tuning: every number of the third-person race camera lives here.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "CameraTuningData.generated.h"

class UMaterialInterface;

/** Named base FOV choices offered to the player (Options menu, later). */
UENUM(BlueprintType)
enum class ECameraFovPreset : uint8
{
	Close,
	Normal,
	Wide,
};

/**
 * Tuning for the chase camera. FOV values are HORIZONTAL degrees on a 16:9 screen.
 * Units: centimetres, seconds, degrees.
 */
UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UCameraTuningData : public UDataAsset
{
	GENERATED_BODY()

public:
	UCameraTuningData();

	float GetPresetFov(ECameraFovPreset Preset) const;

	// ------------------------------------------------------------- Base FOV

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base FOV", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FovClose = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base FOV", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FovNormal = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base FOV", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FovWide = 110.f;

	/** Hard limits for the player's base FOV choice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base FOV", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float MinBaseFov = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base FOV", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float MaxBaseFov = 120.f;

	// -------------------------------------------------------------- Framing

	/** Camera distance behind the car by base FOV (X = base FOV, Y = cm). Close = nearer, Wide = farther. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	FRuntimeFloatCurve DistanceByBaseFov;

	/** Camera height above the car by base FOV (X = base FOV, Y = cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	FRuntimeFloatCurve HeightByBaseFov;

	/** The camera orbits and looks at a point this high above the car's centre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float PivotHeight = 80.f;

	/** The camera aims at a point this far ahead of the car, so more road is visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing", meta = (ClampMin = "0.0"))
	float LookAheadDistance = 400.f;

	// ------------------------------------------------------------------ Lag

	/** How fast the camera catches up with the car's position (1/s). Lower = more lag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lag", meta = (ClampMin = "0.1"))
	float PositionLagSpeed = 12.f;

	/** The camera never falls further behind its ideal position than this (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lag", meta = (ClampMin = "0.0"))
	float MaxPositionLag = 150.f;

	/** How fast the camera turns to the new direction (1/s). Lower = smoother, slower turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lag", meta = (ClampMin = "0.1"))
	float RotationLagSpeed = 7.f;

	/** How fast the camera swings round when look-back is pressed or released (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lag", meta = (ClampMin = "0.1"))
	float LookBackTurnSpeed = 25.f;

	// ------------------------------------------------------ Look direction

	/**
	 * 0 = camera looks where the car points, 1 = where the car actually moves.
	 * In between shows the direction of travel during drifts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look Direction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityLookBlend = 0.6f;

	/** Below this speed (km/h) the camera ignores the direction of travel and looks where the car points. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look Direction", meta = (ClampMin = "0.0"))
	float VelocityLookMinSpeedKmh = 20.f;

	/** At and above this speed (km/h) the direction of travel has its full weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look Direction", meta = (ClampMin = "0.0"))
	float VelocityLookFullSpeedKmh = 60.f;

	// ----------------------------------------------------------- Speed FOV

	/** FOV added at top speed (degrees), on top of the base FOV. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV", meta = (ClampMin = "0.0"))
	float SpeedFovOffset = 8.f;

	/** Extra FOV while boosting (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV", meta = (ClampMin = "0.0"))
	float BoostFovOffset = 10.f;

	/** Strength of the speed and boost FOV by base FOV (X = base FOV, Y = multiplier), so Close and Wide both feel fast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV")
	FRuntimeFloatCurve SpeedEffectByBaseFov;

	/** Base FOV plus all offsets never exceeds this (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float MaxTotalFov = 128.f;

	/** How fast the FOV follows speed changes (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV", meta = (ClampMin = "0.1"))
	float FovInterpSpeed = 5.f;

	/** Extra camera distance while boosting (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV", meta = (ClampMin = "0.0"))
	float BoostPullBack = 80.f;

	/** How fast the boost effect fades in and out (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed FOV", meta = (ClampMin = "0.1"))
	float BoostEffectInterpSpeed = 6.f;

	// --------------------------------------------------------- Speed lines

	/**
	 * Post-process material that draws the speed lines. It must have a scalar parameter
	 * named "Intensity" (0 = invisible). Empty = no speed lines.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed Lines")
	TSoftObjectPtr<UMaterialInterface> SpeedLinesMaterial;

	/** Speed lines start to appear at this fraction of top speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed Lines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedLinesStartSpeedRatio = 0.6f;

	/** Speed lines intensity at top speed without boost (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed Lines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedLinesTopSpeedIntensity = 0.45f;

	/** Speed lines intensity while boosting (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed Lines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedLinesBoostIntensity = 1.f;

	/** How fast the speed lines fade in and out (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed Lines", meta = (ClampMin = "0.1"))
	float SpeedLinesInterpSpeed = 6.f;

	// --------------------------------------------------------------- Shake

	/** Camera shake angle (degrees) for the strongest impact. Weaker hits shake proportionally less. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0.0"))
	float ImpactShakeMaxAngle = 1.2f;

	/** How long an impact shake lasts (s). It fades out over this time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0.01"))
	float ImpactShakeDuration = 0.35f;

	/** How fast the camera shakes (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0.0"))
	float ImpactShakeFrequency = 14.f;

	// ----------------------------------------------------------- Collision

	/** Radius of the probe that keeps the camera out of walls and hills (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "1.0"))
	float ProbeRadius = 25.f;

	/** How fast the camera moves back out after an obstacle is gone (1/s). Moving in is instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.1"))
	float CollisionRecoverySpeed = 4.f;

	// ------------------------------------------------------------ Viewport

	/**
	 * The vertical view is kept constant, so wide viewports (ultrawide, 2-player split-screen)
	 * see more to the sides. This caps the resulting horizontal FOV (degrees).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Viewport", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float MaxHorizontalFov = 140.f;
};
