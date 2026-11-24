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
	 * Difficulty of this driver and a seed for its small personal differences (line offset).
	 * The same seed always gives the same driver. Call before or after possessing the vehicle.
	 */
	void Configure(const UAIDifficultyData* InDifficulty, int32 InSeed);

	const UAIDifficultyData* GetDifficulty() const;

	// IVehicleStepInputSource
	virtual FVehicleInputState GetStepInput(const FVehicleStepInputContext& Context) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitializeComponents() override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FRaceAIDriverParams BuildParams() const;

	/** Adds, updates or removes this driver's rubber-band modifier. Called from the physics step. */
	void UpdateRubberBand(const FVehicleStepInputContext& Context);

	void DrawDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<const UAIDifficultyData> Difficulty;

	/** -1..+1: where in the personal line spread this driver sits. */
	float PersonalLine = 0.f;

	TSharedPtr<const FTrackSplineTable> Track;
	TWeakObjectPtr<URaceTrackerComponent> Tracker;
	TWeakObjectPtr<class UItemSystemComponent> ItemSystem;
	int32 RacerId = INDEX_NONE;

	/** Owned by the physics step. */
	FRaceAIDriverState DriverState;
	TArray<FRaceAIOtherCar> OtherCars;
	/** Item boxes along the track, found once when the driver takes the car. */
	TArray<FRaceAIBoxSpot> BoxSpots;
	/** Rubber-band max-speed multiplier applied in the last step (1 = none); for the debug display. */
	float RubberBandMultiplier = 1.f;
};
