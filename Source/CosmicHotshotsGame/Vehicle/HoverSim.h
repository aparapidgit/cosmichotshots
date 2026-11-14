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

	// Air
	float AirLevelStrength = 0.f;
	float AirLevelDamping = 0.f;

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

/** Result of the last physics step. */
struct FHoverSimState
{
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
	/** Applies hover, gravity, thrust, steering, grip and air control to the body for one physics step. */
	static void Step(
		Chaos::FRigidBodyHandle_Internal& Body,
		const FHoverSimParams& Params,
		const FVehicleInputState& Input,
		FHoverTraceFunction Trace,
		float DeltaTime,
		FHoverSimState& OutState);
};
