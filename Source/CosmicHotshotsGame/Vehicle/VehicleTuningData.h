// Vehicle tuning: every number of the hover physics lives here, never in code.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "VehicleTuningData.generated.h"

class UMaterialInterface;

/**
 * Base tuning for a hovercar. Per-character differences are small modifiers
 * applied on top of this asset (see UCharacterDefinition, added later).
 * Units: centimetres, seconds, kilograms, degrees.
 */
UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UVehicleTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UVehicleTuningData();

	// ---------------------------------------------------------------- Body

	/** Full size of the body (length, width, height). Collision is two lengthwise capsules filling this box. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "1.0"))
	FVector BodySize = FVector(400.0, 200.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "1.0"))
	float MassKg = 1000.f;

	/** Engine angular damping of the rigid body (1/s). Calms leftover wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.0"))
	float AngularDamping = 1.f;

	/** Friction of the body against walls and other cars. Keep near zero so it never snags on seams. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.0"))
	float BodyFriction = 0.f;

	/** Bounciness of the body. Arcade impulses are added by the impact system, so keep at zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BodyRestitution = 0.f;

	/**
	 * Material of the grey-box placeholder car, used while a character has no hovercar mesh.
	 * It must have a vector parameter named "Colour"; the vehicle sets it to the character colour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UMaterialInterface> PlaceholderMaterial;

	/** Colour of the placeholder car when the vehicle has no character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	FLinearColor PlaceholderColour = FLinearColor(0.35f, 0.35f, 0.35f);

	/** How hard this car hits others. Scales the knock-back it GIVES in a collision. 1 = neutral. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.1"))
	float RamPower = 1.f;

	/** How well this car resists hits. Divides the knock-back it RECEIVES in a collision. 1 = neutral. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.1"))
	float Stability = 1.f;

	// -------------------------------------------------------------- Impact

	/** Arcade bounce between two cars: fraction of the closing speed that is thrown back, before RamPower and Stability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float VehicleBounce = 0.6f;

	/** A hit never changes the car's speed by more than this (cm/s), so no hit launches a car off the track. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float MaxImpactSpeedChange = 1200.f;

	/** Hits weaker than this (cm/s of closing speed) are ignored: no bounce, no feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 80.f;

	/** After a hit with the same car or wall, further hits with it are ignored for this long (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float ImpactCooldown = 0.2f;

	/** Received speed change (cm/s) from which a hit causes a wobble (brief loss of grip and steering). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float WobbleThreshold = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float WobbleDuration = 0.4f;

	/** Received speed change (cm/s) from which a hit causes a spin-out. Always short and readable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float SpinOutThreshold = 750.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float SpinOutDuration = 0.8f;

	/** Turn rate added by a spin-out (degrees per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float SpinOutYawRate = 260.f;

	/** Grip multiplier while wobbling or spinning out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ControlLossGripMultiplier = 0.35f;

	/** Steering multiplier while wobbling or spinning out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ControlLossSteerMultiplier = 0.4f;

	/** Bounce off a wall: fraction of the speed into the wall that is thrown back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0"))
	float WallBounce = 0.35f;

	/** Share of the speed ALONG the wall lost in a hard wall hit (0..1). Light scrapes lose proportionally less. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallSpeedLoss = 0.35f;

	/** Speed into the wall (cm/s) that counts as a hard hit for WallSpeedLoss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "1.0"))
	float WallHardHitSpeed = 1500.f;

	// --------------------------------------------------------------- Hover

	/** Clearance between the underside of the body and the ground when resting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "1.0"))
	float RideHeight = 60.f;

	/** Hover points in body space (on the underside). Each one traces down and pushes up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover")
	TArray<FVector> HoverPoints;

	/** Natural frequency of the hover spring in Hz. Higher = stiffer, follows the ground more tightly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0.1"))
	float HoverFrequency = 3.f;

	/** Damping ratio of the hover spring. 1 = no bounce, below 1 = some bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0.0"))
	float HoverDampingRatio = 0.7f;

	/** Radius of the sphere swept by each hover point. Smooths small steps between track segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0.0"))
	float HoverTraceRadius = 15.f;

	/** How far below the spring's reach a point still counts as "over ground" (grounded test only, no force). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0.0"))
	float GroundedExtraReach = 60.f;

	/** Vehicle is grounded when at least this many hover points are over ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "1"))
	int32 MinGroundedPoints = 2;

	// ------------------------------------------------------------- Gravity

	/** Downward acceleration while grounded (cm/s^2). Engine gravity is disabled for the vehicle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (ClampMin = "0.0"))
	float GravityAcceleration = 1500.f;

	/** Gravity multiplier while airborne, so jumps feel snappy instead of floaty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (ClampMin = "0.0"))
	float AirGravityMultiplier = 1.5f;

	// --------------------------------------------------------------- Drive

	/** Top speed on throttle alone, in km/h. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "1.0"))
	float MaxSpeedKmh = 170.f;

	/** Forward acceleration at standstill with full throttle (cm/s^2). Fades out towards top speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0"))
	float Acceleration = 2200.f;

	/** Shape of the acceleration fade: accel * (1 - (speed / max)^exponent). Higher = strong pull for longer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.1"))
	float AccelFalloffExponent = 2.f;

	/** Deceleration with full brake while moving forward (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0"))
	float BrakeDeceleration = 4000.f;

	/** Top speed in reverse, in km/h. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "1.0"))
	float ReverseMaxSpeedKmh = 50.f;

	/** Acceleration in reverse at standstill (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0"))
	float ReverseAcceleration = 1500.f;

	/** Below this forward speed (km/h) the brake starts reversing instead of braking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0"))
	float BrakeToReverseSpeedKmh = 3.f;

	/** Speed lost per second with no throttle and no brake, as a fraction of current speed (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0"))
	float CoastDrag = 0.35f;

	/** How fast speed above the top speed bleeds off (1/s). Matters after boosts and downhill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0"))
	float OverspeedDrag = 1.5f;

	/** Throttle strength while airborne, as a fraction of ground thrust. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirThrustMultiplier = 0.2f;

	// ------------------------------------------------------------ Steering

	/** Turn rate with full steering, in degrees per second, before the speed curve is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0"))
	float MaxYawRate = 110.f;

	/**
	 * Turn rate multiplier by speed. X = speed / top speed (0..1+), Y = multiplier.
	 * Low at standstill (no spinning on the spot), full at low speed, reduced at top speed for stability.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering")
	FRuntimeFloatCurve SteeringBySpeed;

	/** How quickly the car reaches the requested turn rate (1/s). Higher = sharper, more direct steering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0"))
	float YawResponse = 8.f;

	/** Car-like steering while reversing (steering right swings the nose left). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering")
	bool bInvertSteeringInReverse = true;

	/** Turn rate while airborne, as a fraction of ground steering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirSteeringMultiplier = 0.4f;

	// ---------------------------------------------------------------- Grip

	/** Sideways speed removed per second, as a fraction (1/s). Low = slidey hover feel, high = on rails. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.0"))
	float LateralGrip = 6.f;

	// --------------------------------------------------------------- Drift

	/** Sideways grip while drifting (1/s). Lower than LateralGrip so the car slides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0"))
	float DriftGrip = 2.f;

	/** Turn rate multiplier while drifting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0"))
	float DriftYawMultiplier = 1.3f;

	/**
	 * A drift always turns in the direction it was started. This is the steering amount (0..1)
	 * when the stick is pushed fully AGAINST the drift (widest line); fully into the drift gives 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriftMinTurn = 0.35f;

	/** Minimum forward speed (km/h) to start a drift. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0"))
	float DriftMinSpeedKmh = 60.f;

	/** A drift ends when forward speed falls below this (km/h). Lower than the start speed to avoid flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0"))
	float DriftEndSpeedKmh = 40.f;

	/** Minimum steering amount (0..1) needed to start a drift; its sign picks the drift direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriftMinSteer = 0.2f;

	/** Boost charge gained per second of drifting at full slip (the boost meter goes from 0 to 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.0"))
	float DriftChargeRate = 0.35f;

	/** Slip angle (degrees) at which a drift charges at the full rate. Smaller slides charge proportionally less. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = "0.1"))
	float DriftFullChargeSlipAngle = 12.f;

	// --------------------------------------------------------------- Boost

	/** Boost meter spent by one boost (the meter goes from 0 to 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoostCost = 0.34f;

	/** How long one boost lasts, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0.0"))
	float BoostDuration = 1.2f;

	/** Instant forward speed added when a boost starts (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0.0"))
	float BoostImpulse = 350.f;

	/** Top speed while boosting, in km/h. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "1.0"))
	float BoostMaxSpeedKmh = 225.f;

	/** Acceleration multiplier while boosting. The boost pushes at full throttle even when the throttle is released. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0.0"))
	float BoostAccelMultiplier = 1.8f;

	// ----------------------------------------------------------------- Air

	/** How strongly the car levels itself (pitch and roll) while airborne (1/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0"))
	float AirLevelStrength = 30.f;

	/** Damping of pitch and roll rotation while airborne (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0"))
	float AirLevelDamping = 8.f;
};
