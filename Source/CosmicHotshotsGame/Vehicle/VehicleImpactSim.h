// Impact reaction, run inside the fixed physics step.
//
// Takes the contacts the physics engine reported for this vehicle in the previous
// step (UVehicleContactSubsystem). How hard a contact hit is the closing speed the
// solver actually took away (the push on this car along the normal, i.e. impulse /
// reduced mass); contacts the solver only prepared for push nothing and are ignored.
// Then it applies the arcade response: wall bounce with
// partial speed loss, knock-back between cars (RamPower, Stability, masses), short
// wobble or spin-out, boost loss on wall hits and the close-pass lockout. Because it
// runs per physics step and reads velocities through the physics-thread handle, the
// result is identical at every frame rate. Plain data in, velocity changes out.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/VehicleContactSubsystem.h"
#include "Vehicle/VehicleImpactComponent.h"

struct FHoverSimState;

namespace Chaos
{
	class FRigidBodyHandle_Internal;
}

/** Snapshot of the impact tunables (from UVehicleTuningData) and this vehicle's final stats. */
struct FVehicleImpactParams
{
	float MinImpactSpeed = 0.f;
	float ImpactCooldown = 0.f;
	float VehicleBounce = 0.f;
	float MaxImpactSpeedChange = 0.f;
	float WobbleThreshold = 0.f;
	float WobbleDuration = 0.f;
	float SpinOutThreshold = 0.f;
	float SpinOutDuration = 0.f;
	/** Degrees per second. */
	float SpinOutYawRate = 0.f;
	float WallBounce = 0.f;
	float WallSpeedLoss = 0.f;
	float WallHardHitSpeed = 1.f;
	float WallBoostLossMin = 0.f;
	float WallBoostLossMax = 0.f;
	float ClosePassHitLockout = 0.f;
	/** This vehicle's final Stability (character modifiers applied). */
	float Stability = 1.f;

	// Impact-only multipliers from the modifier stack (passives). 1 = no change.

	/** Knock-back received in side contact with another car. */
	float SideSpeedLoss = 1.f;
	/** Length of wobble and spin-out. */
	float HitRecoveryTime = 1.f;
	/** Knock-back received from cars and speed lost along walls. */
	float HitSpeedLoss = 1.f;
	/** A contact is a SIDE contact when its normal is within this cosine of the car's sideways axis. */
	float SideContactMinCos = 0.7f;
};

/** A raw contact with what the game knows about the other body. */
struct FVehicleContact
{
	FVehicleRawContact Raw;
	/** The actor that was hit (for feedback only; may be null). */
	AActor* OtherActor = nullptr;
	bool bOtherIsVehicle = false;
	float OtherMass = 0.f;
	float OtherRamPower = 1.f;
	/** The other car's SideRamPower when the contact is on ITS side, else 1. Multiplies OtherRamPower. */
	float OtherSideRamPower = 1.f;
};

/** Carries over between physics steps. */
struct FVehicleImpactState
{
	/** Simulation time until which further hits with a body are ignored (one hit, not one per step of contact). */
	TMap<const void*, double> CooldownEndTimes;
};

struct FVehicleImpactSim
{
	/**
	 * Applies the impact response for the contacts of the previous step.
	 * Writes control loss, boost loss and the close-pass lockout into SimState and adds
	 * one entry per counted impact to OutImpacts (for camera shake, rumble, VFX, audio).
	 */
	static void Apply(
		Chaos::FRigidBodyHandle_Internal& Body,
		const FVehicleImpactParams& Params,
		TConstArrayView<FVehicleContact> Contacts,
		double SimTime,
		FVehicleImpactState& State,
		FHoverSimState& SimState,
		TArray<FVehicleImpactInfo>& OutImpacts);
};
