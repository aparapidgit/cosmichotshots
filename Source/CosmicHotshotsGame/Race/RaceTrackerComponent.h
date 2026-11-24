// Race tracker component: runs FRaceTrackerSim in the fixed physics step.
//
// Lives on ARaceGameState. On the game thread it finds the track spline, turns the
// checkpoint markers (TriggerBoxes tagged "Checkpoint" + "CP_<index>") into distances along
// the line once (no overlap events: those arrive once per frame and would make laps depend
// on the frame rate), and registers every ARaceVehicle it sees. In every physics step it
// reads each car's body position, advances the race state and logs what happened with the
// step's race time. After each frame the game state takes the newest snapshot and the
// events and broadcasts them.
//
// Threading: the async physics tick may run off the game thread. Registration, the
// published snapshot and the event list are handed over under a lock. The step state
// itself (GetStepState) belongs to the physics step.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Race/RaceTrackerSim.h"
#include "RaceTrackerComponent.generated.h"

class ARaceVehicle;
class FTrackSplineTable;

/** One driver in a FRaceStepView. */
struct FRaceStepRacer
{
	/** On the track and ranked (has a body and was placed on the track line). */
	bool bInRace = false;
	bool bStarted = false;
	bool bFinished = false;
	bool bHuman = false;
	int32 Rank = 0;
	int32 LapsCompleted = 0;
	double Progress = 0.0;
	/** Position in the lap along the track line (cm) and sideways from it (cm, positive = right). */
	float TrackDistance = 0.f;
	float LateralOffset = 0.f;
	/** Speed along the track (cm/s). */
	float ProgressSpeed = 0.f;
	FRaceGap GapToCarAhead;
};

/**
 * Race state after one physics step, for code that runs inside other components' physics steps
 * (passives). Small on purpose: it is copied every step.
 */
struct FRaceStepView
{
	/** By driver id. */
	TArray<FRaceStepRacer> Racers;
	/** What happened in that step (overtakes, laps, finishes). */
	TArray<FRaceEvent> Events;
	/** Drivers with bInRace. */
	int32 NumInRace = 0;
	FRaceGap LeaderToSecondGap;
	double RaceTime = 0.0;
	bool bRaceFinished = false;
};

/** Snapshot of the race state for the game thread. */
struct FRaceTrackerSnapshot
{
	TArray<FRacerSimState> Racers;
	TArray<int32> RankOrder;
	FRaceGap LeaderToSecondGap;
	double RaceTime = 0.0;
	int32 NumLaps = 0;
	bool bRaceFinished = false;
};

UCLASS(ClassGroup = (Race))
class COSMICHOTSHOTSGAME_API URaceTrackerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URaceTrackerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	/** True when the level has a track spline, so the race is tracked. */
	bool IsTracking() const { return Track.IsValid(); }

	const TSharedPtr<const FTrackSplineTable>& GetTrack() const { return Track; }
	const FRaceTrackerParams& GetParams() const { return Params; }

	/** Driver id of a vehicle, INDEX_NONE when not registered. Game thread. */
	int32 FindRacerId(const ARaceVehicle* Vehicle) const;
	ARaceVehicle* GetRacerVehicle(int32 RacerId) const;

	/** Every registered vehicle by driver id (null for a car that is gone). Safe from a physics step. */
	void GetRegisteredVehicles(TArray<ARaceVehicle*>& OutVehicles) const;

	/** Position of a vehicle's physics body as the current physics step sees it; unset when it has none. */
	static TOptional<FVector> ReadBodyLocation(const ARaceVehicle* Vehicle);

	/**
	 * Takes the newest snapshot and the events since the last call. Game thread.
	 * Returns false when no physics step ran since the last call (snapshot unchanged).
	 */
	bool TakeUpdate(FRaceTrackerSnapshot& OutSnapshot, TArray<FRaceEvent>& OutEvents);

	/**
	 * Race state of the PREVIOUS physics step, for code inside another component's physics step
	 * (passives). SimTime is the value the engine passed to that step. The engine does not fix the
	 * order in which components step, so this returns the same view whether the tracker has
	 * already run its own part of the current step or not. Every step's events are seen exactly once.
	 */
	const FRaceStepView& GetViewForStep(float SimTime) const;

	/** Full step state (passing-time history, pair states). Only for code that knows the tracker has finished its step. */
	const FRaceTrackerState& GetStepState() const { return StepState; }

private:
	void BuildCheckpoints();
	void RegisterNewVehicles();
	void LogStep(const TArray<FRaceEvent>& Events);
	void DrawDebug() const;
	FString GetRacerName(int32 RacerId) const;

	TSharedPtr<const FTrackSplineTable> Track;
	FRaceTrackerParams Params;

	/** Vehicles by driver id. Written on the game thread under the lock. */
	TArray<TWeakObjectPtr<ARaceVehicle>> Vehicles;
	/** Drivers registered on the game thread, added to the step state by the next step. */
	TArray<bool> PendingHumanFlags;

	/** Owned by the physics step. */
	FRaceTrackerState StepState;
	TArray<TOptional<FVector>> StepLocations;
	TArray<FRaceEvent> StepEvents;
	double NextLogTime = 0.0;

	/** Views of the two newest steps (see GetViewForStep). */
	FRaceStepView NewestView;
	FRaceStepView OlderView;
	/** SimTime of the step that produced NewestView. */
	float NewestViewSimTime = 0.f;
	bool bHasView = false;

	/** Handed to the game thread. */
	FRaceTrackerSnapshot PublishedSnapshot;
	TArray<FRaceEvent> PublishedEvents;
	bool bSnapshotChanged = false;

	mutable FCriticalSection Lock;
};
