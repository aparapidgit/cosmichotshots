// Race AI controller: the computer driver of one ARaceVehicle.
//
// It registers itself as the vehicle's step input source, so its decisions are made inside
// the fixed physics step (FRaceAIDriverSim) and an AI race is the same at every frame rate.
// The vehicle only sees an input source; it does not know a computer is driving. The AI uses
// the same physics, character stats and passives as a human.
//
// Also here: mild rubber banding. A computer driver far behind the best human gets a small
// max-speed bonus, one far ahead a small penalty, through the shared modifier stack. It can
// be switched off with ch.AI.RubberBanding 0 and does nothing in a race without humans.

#pragma once

#include "CoreMinimal.h"
#include "AI/RaceAIDriverSim.h"
#include "GameFramework/Controller.h"
#include "Vehicle/VehicleStepInput.h"
#include "RaceAIController.generated.h"

class ARaceVehicle;
class FTrackSplineTable;
class UAIDifficultyData;
class URaceTrackerComponent;

UCLASS()
class COSMICHOTSHOTSGAME_API ARaceAIController : public AController, public IVehicleStepInputSource
{
	GENERATED_BODY()

public:
	ARaceAIController();

	/**
	 * Difficulty of this driver and a seed for its personality (line, courage, wall hugging, boost use,
	 * mistakes). The same seed always gives the same driver. Call before or after possessing the vehicle.
	 */
	void Configure(const UAIDifficultyData* InDifficulty, int32 InSeed);

	const UAIDifficultyData* GetDifficulty() const;

	/**
	 * Stand by on a human's car without possessing it: the human keeps driving, and this driver takes the
	 * car over the moment it has finished the race, so it does not sit on the track in everybody's way.
	 */
	void DriveAfterFinish(ARaceVehicle* Vehicle);

	// IVehicleStepInputSource
	virtual bool WantsControl(const FVehicleStepInputContext& Context) override;
	virtual FVehicleInputState GetStepInput(const FVehicleStepInputContext& Context) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitializeComponents() override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Starts driving a vehicle: at once (possessed car), or only once it has finished the race (a human's car). */
	void TakeVehicle(ARaceVehicle* Vehicle, bool bInOnlyAfterFinish);
	void ReleaseVehicle();

	/** Race state after the previous physics step; also finds this driver's id. Null when there is no race. */
	const struct FRaceStepView* FindRaceView(float SimTime);

	FRaceAIDriverParams BuildParams(const FVehicleStepInputContext& Context) const;

	/** Adds, updates or removes this driver's rubber-band modifier. Called from the physics step. */
	void UpdateRubberBand(const FVehicleStepInputContext& Context);

	/** Counts what the car did in its previous step and writes one AI_LAP log line per finished lap. */
	void UpdateLapStats(const FVehicleStepInputContext& Context);

	void DrawDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<const UAIDifficultyData> Difficulty;

	/** What makes this driver differ from the others; drawn from its seed. */
	FRaceAIPersonality Personality;

	TSharedPtr<const FTrackSplineTable> Track;
	TWeakObjectPtr<URaceTrackerComponent> Tracker;
	TWeakObjectPtr<class UItemSystemComponent> ItemSystem;
	int32 RacerId = INDEX_NONE;

	TWeakObjectPtr<ARaceVehicle> DrivenVehicle;
	/** Standing by on a human's car until it has finished. */
	bool bOnlyAfterFinish = false;
	bool bTookOver = false;

	/** Owned by the physics step. */
	FRaceAIDriverState DriverState;
	TArray<FRaceAIOtherCar> OtherCars;
	/** Item boxes along the track, found once when the driver takes the car. */
	TArray<FRaceAIBoxSpot> BoxSpots;
	/** The car's steering-by-speed curve, sampled once (the physics step must not touch the asset's curve). */
	TArray<float> SteeringBySpeedSamples;

	/** Rubber-band max-speed multiplier applied in the last step (1 = none); for the debug display. */
	float RubberBandMultiplier = 1.f;

	/** What the car did during the current lap (dev log, for tuning the drivers). Owned by the physics step. */
	struct FLapStats
	{
		float BoostSeconds = 0.f;
		float DriftSeconds = 0.f;
		float ClosePassSeconds = 0.f;
		int32 Boosts = 0;
		/** Touches of a wall or an object (each starts the close-pass lockout). */
		int32 Touches = 0;
		int32 Mistakes = 0;
		/** Drifts that went past the critical slide angle (loss of control). */
		int32 Overslides = 0;
		/** Largest distance (cm) between the car and the line it was steering for. */
		float MaxLineError = 0.f;
		float LineErrorSum = 0.f;
		float Seconds = 0.f;
	};
	FLapStats LapStats;
	int32 StatsLap = INDEX_NONE;
	int32 StatsBoostCount = 0;
	float StatsLockout = 0.f;
	float StatsOverslide = 0.f;
	int32 StatsMistakeCount = 0;
	FString VehicleName;
	/** Next time (vehicle sim time) an AI_STEP line is written (ch.Debug.AILog). */
	double NextStepLogTime = 0.0;
};
