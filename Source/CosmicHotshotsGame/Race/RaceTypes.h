// Race state types shared by the tracker, the game state, the HUD and (later) passives.

#pragma once

#include "CoreMinimal.h"
#include "RaceTypes.generated.h"

class APlayerState;
class ARaceVehicle;
class UCharacterDefinition;

UENUM(BlueprintType)
enum class ERacePhase : uint8
{
	/** No track spline in the level: nothing is tracked. */
	Inactive,
	Racing,
	/** Every human driver has finished (or every driver, when no human races). */
	Finished
};

/** Gap between two cars, measured along the track. */
USTRUCT(BlueprintType)
struct FRaceGap
{
	GENERATED_BODY()

	/** False when there is no such car (e.g. no car ahead of the leader). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bValid = false;

	/** Distance along the track in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float DistanceCm = 0.f;

	/** How long ago the car ahead was where the car behind is now, in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float TimeSeconds = 0.f;
};

/** How close an overtake was. */
USTRUCT(BlueprintType)
struct FOvertakeCloseness
{
	GENERATED_BODY()

	/** Sideways distance between the two cars (cm) at the moment they were level. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float LateralDistance = 0.f;

	/** Gap along the track (cm) between the two cars when the overtake was confirmed. Information only. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float GapAtConfirm = 0.f;

	/** LateralDistance within CloseOvertakeLateral of the race tuning (speed difference does not matter). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bClose = false;

	/** 1 = side by side, 0 = at (or beyond) the close lateral limit. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float Closeness = 0.f;
};

/** Live race state of one driver. */
USTRUCT(BlueprintType)
struct FRacerStanding
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TObjectPtr<ARaceVehicle> Vehicle;

	/** Order in which the tracker registered the driver (grid order). Breaks ties. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 GridIndex = INDEX_NONE;

	/** 1 = leader. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 Rank = 0;

	/** Lap being driven, 1..NumLaps (NumLaps after finishing). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 CurrentLap = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 LapsCompleted = 0;

	/** Index of the next checkpoint to pass (0 = start/finish line). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 NextCheckpoint = 0;

	/** Total distance along the track since the start line in cm (negative on the grid behind the line). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float Progress = 0.f;

	/** Position within the lap along the track, 0..lap length, in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float TrackDistance = 0.f;

	/** Sideways offset from the track line in cm (positive = right). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float LateralOffset = 0.f;

	/** Gap to the next car ahead in the ranking. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	FRaceGap GapToCarAhead;

	/** Has crossed the start line (or joined the race past it). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bStarted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bFinished = false;

	/** Race time at the finish line in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float FinishTime = 0.f;

	/** Fastest completed lap in seconds, 0 when none. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float BestLapTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TArray<float> LapTimes;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bHuman = false;
};

/** One line of the race results. */
USTRUCT(BlueprintType)
struct FRaceResultEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TObjectPtr<ARaceVehicle> Vehicle;

	/** Null for drivers without a player state (test dummies). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TObjectPtr<APlayerState> PlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TObjectPtr<const UCharacterDefinition> Character;

	/** 1 = winner. Drivers that did not finish are placed by their progress, after all finishers. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 Place = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bFinished = false;

	/** Race time at the finish line in seconds (0 when not finished). */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float TotalTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float BestLapTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TArray<float> LapTimes;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bHuman = false;
};

/** Everything a results screen or the Adventure unlock logic needs. */
USTRUCT(BlueprintType)
struct FRaceResultData
{
	GENERATED_BODY()

	/** Ordered by place. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TArray<FRaceResultEntry> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 NumLaps = 0;

	/** Race time when the race finished, in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float RaceTime = 0.f;
};
