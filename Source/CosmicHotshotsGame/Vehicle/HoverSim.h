// Hover physics step. Plain data in, forces out: no UObject access.
//
// It runs once per fixed physics step and only touches the physics-thread body
// handle, so it can later move unchanged into a Chaos sim callback (needed for
// network prediction) without rewriting the physics.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Vehicle/VehicleInputState.h"

namespace Chaos
{
	class FRigidBodyHandle_Internal;
}

/** Snapshot of the tunables used by one physics step (data asset values with console overrides applied). */
struct FHoverSimParams
{
	/** Number of samples the steering-by-speed curve is baked into. */
	static constexpr int32 SteeringSamples = 16;
	/** Speed ratio (speed / top speed) covered by the baked steering samples. */
	static constexpr float SteeringSampleRange = 1.5f;

	// Hover
	TArray<FVector, TInlineAllocator<8>> HoverPoints;
	float RideHeight = 0.f;
	float HoverFrequency = 0.f;
	float HoverDampingRatio = 0.f;
	float TraceRadius = 0.f;
	float GroundedExtraReach = 0.f;
	int32 MinGroundedPoints = 1;

	// Gravity
	float GravityAcceleration = 0.f;
	float AirGravityMultiplier = 1.f;

	// Drive (speeds in cm/s, accelerations in cm/s^2)
	float MaxSpeed = 1.f;
	float Acceleration = 0.f;
	float AccelFalloffExponent = 1.f;
	float BrakeDeceleration = 0.f;
	float ReverseMaxSpeed = 1.f;
	float ReverseAcceleration = 0.f;
	float BrakeToReverseSpeed = 0.f;
	float CoastDrag = 0.f;
	float OverspeedDrag = 0.f;
	float AirThrustMultiplier = 0.f;

	// Steering (radians per second)
	float MaxYawRate = 0.f;
	TStaticArray<float, SteeringSamples> SteeringBySpeed;
	float YawResponse = 0.f;
	bool bInvertSteeringInReverse = true;
	float AirSteeringMultiplier = 0.f;

	// Grip
	float LateralGrip = 0.f;

	// Drift (speeds in cm/s, angles in degrees)
	float DriftGrip = 0.f;
	float DriftYawMultiplier = 1.f;
	float DriftMinTurn = 0.f;
	float DriftMinSpeed = 0.f;
	float DriftEndSpeed = 0.f;
	float DriftChargeRate = 0.f;
	float DriftFullChargeSlipAngle = 1.f;
	/** Slip angle past which a drift turns into an uncontrolled slide. */
	float DriftCriticalSlipAngle = 180.f;
	/** Seconds for grip and steering to come back once the slide is below the critical angle again. */
	float DriftRecoveryTime = 0.f;
	float OverslideGripMultiplier = 1.f;
	float OverslideSteerMultiplier = 1.f;

	// Close pass (distances in cm, speed in cm/s)
	/** Half the body width: the sideways sweeps measure the gap from the body's side. */
	float BodyHalfWidth = 0.f;
	float ClosePassMinSpeed = 0.f;
	float ClosePassMaxDistance = 0.f;
	float ClosePassFullDistance = 0.f;
	float ClosePassContactDistance = 0.f;
	float ClosePassChargeRate = 0.f;
	float ClosePassHitLockout = 0.f;
	float ClosePassTraceRadius = 1.f;

	// Boost (the meter goes from 0 to MaxBoostCharge)
	/** Number of equal segments the meter is divided into. The active ability spends one whole segment. */
	int32 BoostSegments = 1;
	/** Meter drained per second while the boost button is held. */
	float BoostDrainRate = 0.f;
	/** Meter spent the moment a boost starts. */
	float BoostStartCost = 0.f;
	/** A boost cannot start below this charge. */
	float BoostMinStartCharge = 0.f;
	float BoostImpulse = 0.f;
	float BoostMaxSpeed = 1.f;
	float BoostAccelMultiplier = 1.f;
	/** Dev only: keeps the boost meter full. */
	bool bInfiniteBoost = false;

	/** Pull or push from a track zone for this step (cm/s^2, world space). Acts on the ground only. */
	FVector ExternalAcceleration = FVector::ZeroVector;

	// Air
	float AirLevelStrength = 0.f;
	float AirLevelDamping = 0.f;

	// Control loss after a strong hit (wobble, spin-out)
	float ControlLossGripMultiplier = 1.f;
	float ControlLossSteerMultiplier = 1.f;

	/** Steering multiplier for a speed ratio, linearly interpolated from the baked samples. */
	float GetSteeringMultiplier(float SpeedRatio) const;
};

/** What one hover point saw and did in the last step (debug and HUD). */
struct FHoverPointState
{
	FVector WorldPosition = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	bool bOverGround = false;
	/** Clearance below the point in cm (valid when bOverGround). */
	float Height = 0.f;
	/** Spring compression in cm (0 when the ground is out of the spring's reach). */
	float Compression = 0.f;
};

/**
 * Drift and boost state. Unlike the rest of FHoverSimState this is not just a report:
 * it carries over from one physics step to the next.
 */
struct FDriftBoostState
{
	static constexpr float MaxBoostCharge = 1.f;

	bool bDrifting = false;
	/** +1 = drifting to the right, -1 = to the left. Locked when the drift starts. */
	float DriftDirection = 0.f;
	/**
	 * Seconds of reduced grip and steering left after a drift went past the critical slip angle.
	 * Held at the full recovery time while the slide is still past that angle. 0 = full control.
	 */
	float OverslideTimeRemaining = 0.f;
	/** After an overslide a new drift needs a fresh button press, not a button that was simply kept held. */
	bool bDriftNeedsNewPress = false;
	/**
	 * Boost meter, 0..MaxBoostCharge, shown in equal segments. Filled by drifting (and later by
	 * near misses and pickups). Holding boost drains it continuously; the active ability spends
	 * one whole segment.
	 */
	float BoostCharge = 0.f;
	/** True while the boost is running (button held and meter not empty). */
	bool bBoosting = false;
	/** Number of boosts started so far. Lets camera/VFX/audio notice a new boost. */
	int32 BoostCount = 0;
	/** Number of times the active ability button spent a segment. An ability system reacts to this. */
	int32 AbilityUseCount = 0;
	/** Seconds left without close-pass charging after touching or hitting a wall or object. */
	float ClosePassLockoutRemaining = 0.f;
	/** True when the last step charged the meter from a close pass. */
	bool bClosePassCharging = false;

	bool IsBoosting() const { return bBoosting; }
	bool IsOversliding() const { return OverslideTimeRemaining > 0.f; }
};

/** One sideways sweep that measures the gap to a wall (debug and HUD). */
struct FWallProbeState
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	/** Gap between the side of the body and the wall in cm. Negative = no wall within reach. */
	float Clearance = -1.f;
};

/** Result of the last physics step. */
struct FHoverSimState
{
	static constexpr int32 NumWallProbes = 2;

	FDriftBoostState DriftBoost;

	/** Sideways sweeps: [0] = left, [1] = right. */
	FWallProbeState WallProbes[NumWallProbes];
	/** Smallest gap to a wall on either side in cm. Negative = no wall within reach. */
	float WallClearance = -1.f;

	/** Seconds of reduced grip and steering left after a strong hit. Carries over between steps like DriftBoost. */
	float ControlLossTimeRemaining = 0.f;
	TArray<FHoverPointState, TInlineAllocator<8>> Points;
	int32 PointsOverGround = 0;
	bool bGrounded = false;
	/** Average clearance of the points that are over ground, in cm. Negative when none is. */
	float AverageHeight = -1.f;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	/** Speed along the car's nose (negative in reverse) and to its right, in cm/s. */
	float ForwardSpeed = 0.f;
	float LateralSpeed = 0.f;
	/** Turn rate around the car's up axis in degrees per second (positive = turning right). */
	float YawRate = 0.f;
	/** Angle between where the car points and where it moves, in degrees (positive = sliding to the right). */
	float SlipAngle = 0.f;
	float Yaw = 0.f;
	float DeltaTime = 0.f;
};

/**
 * Sweeps a sphere from Start to End against the track.
 * Returns true on a hit and writes how far the sphere centre travelled.
 */
using FHoverTraceFunction = TFunctionRef<bool(const FVector& Start, const FVector& End, float Radius, float& OutDistance)>;

struct FHoverSim
{
	/**
	 * Applies hover, gravity, thrust, steering, grip, drift, boost and air control to the body
	 * for one physics step.
	 * @param bBoostPressed    true on the step right after the boost button went down. Starts a boost
	 *                         even if the button was released again before this step ran.
	 * @param bAbilityPressed  true on the step right after the ability button went down (press, not hold).
	 */
	static void Step(
		Chaos::FRigidBodyHandle_Internal& Body,
		const FHoverSimParams& Params,
		const FVehicleInputState& Input,
		bool bBoostPressed,
		bool bAbilityPressed,
		FHoverTraceFunction Trace,
		float DeltaTime,
		FHoverSimState& OutState);
};
