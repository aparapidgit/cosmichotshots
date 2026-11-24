// Race AI driver simulation: one computer driver's decisions for one fixed physics step.
//
// Plain C++ (like FHoverSim), called from the vehicle's physics step, so an AI race is
// identical at every frame rate. The driver:
//  - follows the track line with a sideways offset: towards the inside of corners, plus a
//    small personal offset so the field spreads out;
//  - reads the curvature of the track ahead, picks a speed for every corner and brakes early
//    enough to reach it;
//  - moves aside for a slower car ahead, or slows down when there is no room;
//  - backs off when it is stuck against something;
//  - optionally drifts sharp corners and boosts on straights (difficulty).
// It only produces an FVehicleInputState: the same throttle, brake, steering and buttons a
// human has. It uses the same vehicle physics, character stats and passives as everybody.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/VehicleInputState.h"

class FTrackSplineTable;

/** Difficulty and car values the driver works with (copied once; see UAIDifficultyData). */
struct FRaceAIDriverParams
{
	/** The car's top speed (cm/s) and usable half width of the track for its centre (cm). */
	float TopSpeed = 4700.f;
	float UsableHalfWidth = 1250.f;

	float TopSpeedFactor = 0.95f;
	float CornerAcceleration = 1800.f;
	float PlannedBraking = 2000.f;
	float ApexOffset = 700.f;
	float ApexFullCurvature = 0.00012f;
	/** This driver's own line offset (cm, signed). */
	float PersonalOffset = 0.f;
	float LineChangeSpeed = 600.f;
	float SteeringSpeed = 6.f;
	bool bUseDrift = false;
	float DriftMinCurvature = 0.00012f;
	float DriftMinSpeed = 2200.f;
	bool bUseBoost = true;
	float BoostMinCharge = 0.3f;
};

/** Another car, as the race tracker saw it after the previous step. */
struct FRaceAIOtherCar
{
	float TrackDistance = 0.f;
	float LateralOffset = 0.f;
	/** Speed along the track (cm/s). */
	float Speed = 0.f;
};

/** What the driver sees in one step. */
struct FRaceAIDriverInput
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Velocity = FVector::ZeroVector;
	float DeltaTime = 0.f;
	/** Seconds the car has been simulating (the driver waits for it to settle first). */
	double ElapsedSimTime = 0.0;
	/** Boost meter 0..1. */
	float BoostCharge = 0.f;
	TConstArrayView<FRaceAIOtherCar> Others;
};

enum class ERaceAIDriverMode : uint8
{
	Waiting,
	Driving,
	Braking,
	Avoiding,
	Reversing
};

/** Carries over between steps; also what the debug display shows. */
struct FRaceAIDriverState
{
	/** Distance along the track line (cm); negative = not placed yet. */
	float TrackDistance = -1.f;
	float LateralOffset = 0.f;
	/** Sideways offset the driver is steering towards (cm), moved smoothly. */
	float TargetLateral = 0.f;
	float TargetSpeed = 0.f;
	float Steer = 0.f;
	float StuckTime = 0.f;
	float ReverseTimeRemaining = 0.f;
	bool bBoostHeld = false;
	ERaceAIDriverMode Mode = ERaceAIDriverMode::Waiting;
	FVector AimPoint = FVector::ZeroVector;
};

struct COSMICHOTSHOTSGAME_API FRaceAIDriverSim
{
	static FVehicleInputState Step(const FTrackSplineTable& Track, const FRaceAIDriverParams& Params,
		const FRaceAIDriverInput& Input, FRaceAIDriverState& State);
};
