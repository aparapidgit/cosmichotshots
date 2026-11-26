// Race AI driver simulation: one computer driver's decisions for one fixed physics step.
//
// Plain C++ (like FHoverSim), called from the vehicle's physics step, so an AI race is
// identical at every frame rate. The driver:
//  - follows the track line with a sideways offset: towards the inside of corners, plus a
//    small personal offset so the field spreads out; a driver that dares runs right along
//    the inside wall, which charges the boost meter (close pass);
//  - reads the curvature of the track ahead, picks a speed for every corner and brakes early
//    enough to reach it;
//  - goes around a slower car ahead on a side it commits to (the inside of the next corner if it is
//    aggressive enough and there is room), boosts past it, or slows down when there is no room;
//  - knows about a car alongside and keeps to its own side of it;
//  - defends against a faster car behind with ONE move towards that car's side, never weaving;
//  - backs off when it is stuck against something;
//  - optionally drifts sharp corners and boosts where the track allows (difficulty);
//  - makes small seeded mistakes (lifts, runs wide, forgets to boost), as often as its difficulty says.
// It only produces an FVehicleInputState: the same throttle, brake, steering and buttons a
// human has. It uses the same vehicle physics, character stats and passives as everybody.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/VehicleInputState.h"

class FTrackSplineTable;

/**
 * What makes one driver differ from the next. Drawn once from the driver's seed; the difficulty says
 * how far each value may move the driving (UAIDifficultyData, "Variety").
 */
struct FRaceAIPersonality
{
	/** -1..+1: where in the personal line spread the driver sits. */
	float LineBias = 0.f;
	/** -1..+1: timid .. brave in corners and on the brakes. */
	float Courage = 0.f;
	/** 0..1: how much of the top-speed spread the driver gives away on straights. */
	float Pace = 0.f;
	/** 0..1: compared with the difficulty's share of wall huggers. */
	float WallHugDraw = 0.f;
	/** 0..1: how much extra gap a wall hugger keeps. */
	float WallCaution = 0.f;
	/** -1..+1: burns the boost at once .. saves it up. */
	float BoostPatience = 0.f;
	/** 0..1: how hard the driver defends and how readily it dives for the inside when overtaking. */
	float Aggression = 0.f;
	/** Seed of this driver's mistakes. */
	int32 MistakeSeed = 0;
};

/** Difficulty and car values the driver works with (built every step; see UAIDifficultyData). */
struct FRaceAIDriverParams
{
	/** The car's top speed (cm/s) and usable half width of the track for its centre (cm). */
	float TopSpeed = 4700.f;
	float UsableHalfWidth = 1250.f;
	/** Turn rate (radians per second) the car reaches with full steering at its current speed. */
	float MaxYawRate = 1.f;
	/** In a drift: share of the full turn the car makes with the steering fully against it, and the drift's turn multiplier. */
	float DriftMinTurn = 0.35f;
	float DriftYawMultiplier = 1.3f;
	/** The driver leaves its line by at most this much (cm) to collect an item box. */
	float ItemBoxMaxSidestep = 1100.f;
	/** Centre line to wall (cm) and half the width of the car's body (cm). */
	float TrackHalfWidth = 1500.f;
	float BodyHalfWidth = 100.f;
	/** Gap to the inside wall the driver aims for in corners (cm); 0 = does not hug walls. */
	float WallHugClearance = 0.f;
	/** The car's top speed while its boost runs (cm/s). */
	float BoostTopSpeed = 6250.f;

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
	float DriftMaxSlipAngle = 22.f;
	/** The car's own limit (degrees): a drift past it becomes an uncontrolled slide. */
	float DriftCriticalSlipAngle = 30.f;
	bool bUseBoost = true;
	float BoostMinCharge = 0.3f;
	float BoostMaxCurvature = 0.00002f;
	/** Defending: how far (cm) the driver moves once towards the side of a faster car closing in from behind. 0 = never. */
	float DefendShift = 0.f;
	/** Overtaking: 0..1, how readily the driver takes the inside instead of the side with more room; meter needed to boost past. */
	float InsidePassShare = 0.5f;
	float OvertakeBoostCharge = 0.2f;
	/** Small seeded mistakes: chance per stretch of track, and their size. */
	float MistakeChance = 0.f;
	float MistakeLiftTime = 0.f;
	float MistakeWideOffset = 0.f;
	int32 MistakeSeed = 0;
};

/** Another car, as the race tracker saw it after the previous step. */
struct FRaceAIOtherCar
{
	float TrackDistance = 0.f;
	float LateralOffset = 0.f;
	/** Speed along the track (cm/s). */
	float Speed = 0.f;
};

/** Where an item box sits on the track. */
struct FRaceAIBoxSpot
{
	float TrackDistance = 0.f;
	float LateralOffset = 0.f;
	/** Can be collected right now. */
	bool bActive = false;
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
	/** The boost was running after the car's previous step. */
	bool bBoosting = false;
	/** Drift state after the car's previous step: +1 drifting right, -1 left, 0 not drifting. */
	float DriftDirection = 0.f;
	/** Gap between the car's side and the wall to its left and right (cm) after the previous step; negative = no wall in reach. */
	float WallClearanceLeft = -1.f;
	float WallClearanceRight = -1.f;
	/** Laps this car has completed (mistakes are seeded per lap). */
	int32 Lap = 0;
	TConstArrayView<FRaceAIOtherCar> Others;
	/** Item boxes on the track; the driver lines up for the nearest one of a row ahead. */
	TConstArrayView<FRaceAIBoxSpot> Boxes;
};

/** A small slip-up that lasts for one stretch of track. */
enum class ERaceAIMistake : uint8
{
	None,
	/** Off the throttle for a moment. */
	Lift,
	/** Runs wide of its line. */
	Wide,
	/** Forgets to boost. */
	NoBoost
};

enum class ERaceAIDriverMode : uint8
{
	Waiting,
	Driving,
	Braking,
	Avoiding,
	Defending,
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
	/** Which side of the track the driver is on for the corners: -1 = fully left, +1 = fully right. */
	float LineSide = 0.f;
	/** How far the car is from its line (cm, positive = the line is to its right); along a wall, the error in the gap to it. */
	float LineError = 0.f;
	/** Aim shift (cm) that removes a steady error in the gap to the wall being hugged. */
	float WallHugTrim = 0.f;
	/** Driving along the inside wall right now. */
	bool bHuggingWall = false;
	float TargetSpeed = 0.f;
	float Steer = 0.f;
	float StuckTime = 0.f;
	float ReverseTimeRemaining = 0.f;
	bool bBoostHeld = false;
	/** Overtaking: the side the driver committed to (+1 right, -1 left, 0 none) and how long (s) the commitment still holds without a car to pass. */
	float PassSide = 0.f;
	float PassHoldRemaining = 0.f;
	/** Defending: holding DefendLateral against a car behind; false again once that car is alongside or gone. */
	bool bDefending = false;
	float DefendLateral = 0.f;
	float DefendHoldRemaining = 0.f;
	/** Mistake of the current stretch of track, the stretch it was decided for, and how many there were so far. */
	ERaceAIMistake Mistake = ERaceAIMistake::None;
	int32 MistakeStretch = INDEX_NONE;
	float MistakeLiftRemaining = 0.f;
	int32 MistakeCount = 0;
	/** Drift button the driver holds: +1 right, -1 left, 0 none. */
	float DriftHeld = 0.f;
	ERaceAIDriverMode Mode = ERaceAIDriverMode::Waiting;
	FVector AimPoint = FVector::ZeroVector;
};

struct COSMICHOTSHOTSGAME_API FRaceAIDriverSim
{
	static FVehicleInputState Step(const FTrackSplineTable& Track, const FRaceAIDriverParams& Params,
		const FRaceAIDriverInput& Input, FRaceAIDriverState& State);
};
