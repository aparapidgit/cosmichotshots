// Race tracker simulation: one fixed physics step of the race rules for every driver.
//
// Plain C++ (like FHoverSim) so the whole race state advances in the fixed step, in one
// place, always in the same order (registration = grid order). That is what makes laps,
// ranks, gaps and overtakes identical at 30, 60 and 144 FPS.
//
// Per step and driver: project the car onto the track line (near its previous position),
// unwrap the lap distance into total progress, pass checkpoint gates in order (laps and the
// finish), record passing times for time gaps, then rank everybody, measure gaps and look
// for overtakes. Events are written to a list with the time they happened (interpolated
// inside the step); the game thread broadcasts them later.
//
// Progress: Lap x LapLength + distance in the lap. The grid sits behind the start line,
// so a car there starts with a small negative progress; crossing the line starts lap 1.
// Driving backwards lowers progress again, so nothing can be farmed by going back and forth.

#pragma once

#include "CoreMinimal.h"
#include "Race/RaceTypes.h"

class FTrackSplineTable;

struct FRaceTrackerParams
{
	int32 NumLaps = 3;
	float ProjectionWindow = 3000.f;
	float ReacquireDistance = 2500.f;
	float OvertakeConfirmTime = 0.4f;
	float OvertakeMargin = 200.f;
	float OvertakePairCooldown = 3.f;
	float CloseOvertakeLateral = 600.f;
	float GapBucketSize = 1000.f;
	float MinGapSpeed = 1000.f;

	/** Distance of each checkpoint in the lap, ascending; [0] = 0 = start/finish line. */
	TArray<float> CheckpointDistances;
};

struct FRacerSimState
{
	/** Registration order (grid order); also the index in FRaceTrackerState::Racers. */
	int32 Id = INDEX_NONE;
	bool bHuman = false;
	/** False while the car has no physics body (not spawned yet, destroyed). Inactive cars keep their last state. */
	bool bActive = false;
	bool bInitialised = false;

	float TrackDistance = 0.f;
	float LateralOffset = 0.f;
	double Progress = 0.0;
	double PrevProgress = 0.0;
	/** Progress change per second over the last step (cm/s). */
	float ProgressSpeed = 0.f;

	/** Number of checkpoint gates passed over the whole race. Gate g = checkpoint g % NumCheckpoints of lap g / NumCheckpoints. */
	int32 GatesPassed = 0;
	bool bStarted = false;
	int32 LapsCompleted = 0;
	double LapStartTime = 0.0;
	/** False when the car joined mid-lap: that first lap has no valid time. */
	bool bLapTimeValid = false;
	TArray<float> LapTimes;
	float BestLapTime = 0.f;
	bool bFinished = false;
	double FinishTime = 0.0;
	int32 FinishPlace = 0;

	int32 Rank = 0;
	FRaceGap GapToCarAhead;

	/** Race time when the car first reached each GapBucketSize stretch of progress (index 0 = BucketOrigin). */
	TArray<double> BucketTimes;
	/** Buckets before this one were passed before the car joined and have no time. */
	int32 FirstBucket = 0;
	/** Next bucket without a time yet. */
	int32 NextBucket = 0;
};

/** Order of two cars (A = lower id), for overtake detection. */
struct FRacePairState
{
	/** +1 = A ahead, -1 = B ahead, 0 = not known yet. */
	int8 ConfirmedOrder = 0;
	/** Order seen this step, with the margin as hysteresis. */
	int8 RawOrder = 0;
	/** Sign of the progress difference without margin, to catch the moment the cars are level. */
	int8 LevelSign = 0;
	/** How long RawOrder has differed from ConfirmedOrder. */
	float PendingTime = 0.f;
	/** Sideways distance at the last moment the cars were level; negative = not seen. */
	float LateralAtLevel = -1.f;
	/** Race time of the last counted overtake: [0] = A over B, [1] = B over A. */
	double LastOvertakeTime[2] = { -1.0e9, -1.0e9 };
};

enum class ERaceEventType : uint8
{
	LapCompleted,
	RacerFinished,
	Overtake,
	RaceFinished
};

struct FRaceEvent
{
	ERaceEventType Type = ERaceEventType::LapCompleted;
	/** Driver id (overtaker for overtakes). */
	int32 Racer = INDEX_NONE;
	/** Overtaken driver id for overtakes. */
	int32 Other = INDEX_NONE;
	/** Race time when it happened. */
	double Time = 0.0;
	int32 Lap = 0;
	float LapTime = 0.f;
	int32 Place = 0;
	FOvertakeCloseness Closeness;
};

struct FRaceTrackerState
{
	TArray<FRacerSimState> Racers;
	/** Pair (A, B), A < B, at index A * Racers.Num() + B. Resized when a driver joins. */
	TArray<FRacePairState> Pairs;
	/** Racer ids in rank order (leader first). */
	TArray<int32> RankOrder;

	int64 StepCount = 0;
	/** Race time of the positions read in the current step: StepCount x step length. */
	double RaceTime = 0.0;
	int32 FinishedCount = 0;
	bool bRaceFinished = false;
	FRaceGap LeaderToSecondGap;
};

struct COSMICHOTSHOTSGAME_API FRaceTrackerSim
{
	/** Adds a driver; it is placed on the track at its first step with a location. Returns its id. */
	static int32 AddRacer(FRaceTrackerState& State, bool bHuman);

	/**
	 * One fixed step. Locations[i] is driver i's position at the start of the step,
	 * unset when it has no body. Appends what happened to OutEvents.
	 */
	static void Step(FRaceTrackerState& State, const FTrackSplineTable& Track, const FRaceTrackerParams& Params,
		TConstArrayView<TOptional<FVector>> Locations, double StepSeconds, TArray<FRaceEvent>& OutEvents);

	/** Gap from Ahead to Behind along the track (distance and time). */
	static FRaceGap ComputeGap(const FRaceTrackerState& State, const FRaceTrackerParams& Params, float LapLength, int32 Ahead, int32 Behind);

	/** Lap currently being driven, 1..NumLaps. */
	static int32 GetCurrentLap(const FRacerSimState& Racer, int32 NumLaps);

	/** Index of the next checkpoint to pass (0 = start/finish line). */
	static int32 GetNextCheckpoint(const FRacerSimState& Racer, const FRaceTrackerParams& Params);
};
