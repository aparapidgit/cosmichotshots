// Race game state: the race state everybody can see (replication-ready, no networking yet).
//
// Holds the standings of every driver (humans, AI and test dummies alike, so it does
// not depend on player states), the race phase and time, the race events and the queries
// that passives, abilities and the HUD use. The numbers come from URaceTrackerComponent,
// which computes them in the fixed physics step; this class copies the newest snapshot
// once per frame and broadcasts the events. Queries here are for the game thread; code
// inside the physics step uses GetTracker()->GetStepState().

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Race/RaceTypes.h"
#include "RaceGameState.generated.h"

class ARaceVehicle;
class URaceTrackerComponent;
struct FTrackFrame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRaceOvertake, ARaceVehicle*, Overtaker, ARaceVehicle*, Overtaken, const FOvertakeCloseness&, Closeness);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRaceLapCompleted, ARaceVehicle*, Vehicle, int32, LapNumber, float, LapTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRacerFinished, ARaceVehicle*, Vehicle, int32, Place, float, FinishTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaceFinished, const FRaceResultData&, Results);

UCLASS()
class COSMICHOTSHOTSGAME_API ARaceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARaceGameState();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	URaceTrackerComponent* GetTracker() const { return Tracker; }

	// Queries (game thread)

	ERacePhase GetRacePhase() const { return RacePhase; }
	float GetRaceTime() const { return RaceTime; }
	int32 GetNumLaps() const { return NumLaps; }

	/** Every driver, leader first. */
	const TArray<FRacerStanding>& GetStandings() const { return Standings; }

	/** Null when the vehicle is not in the race. */
	const FRacerStanding* FindStanding(const ARaceVehicle* Vehicle) const;

	/** 1 = leader, 0 = not in the race. */
	UFUNCTION(BlueprintCallable, Category = "Race")
	int32 GetRank(const ARaceVehicle* Vehicle) const;

	UFUNCTION(BlueprintCallable, Category = "Race")
	ARaceVehicle* GetLeader() const;

	/** Next car ahead in the ranking, null for the leader. */
	UFUNCTION(BlueprintCallable, Category = "Race")
	ARaceVehicle* GetCarAhead(const ARaceVehicle* Vehicle) const;

	/** Gap to the next car ahead along the track (invalid for the leader). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	FRaceGap GetGapToCarAhead(const ARaceVehicle* Vehicle) const;

	/** Gap between 1st and 2nd place along the track (invalid with fewer than two drivers). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	FRaceGap GetLeaderToSecondGap() const { return LeaderToSecondGap; }

	/** Driving direction of the track nearest to a world location (unit vector; forward X when no track). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	FVector GetTrackDirectionAt(const FVector& Location) const;

	/** Track line point, driving direction and right vector at a distance along the track (wraps around the lap). */
	bool GetTrackFrameAtDistance(float Distance, FTrackFrame& OutFrame) const;

	/** Lap length along the track line in cm, 0 when no track. */
	float GetLapLength() const;

	/** Results of the finished race (empty before). */
	const FRaceResultData& GetResults() const { return Results; }

	// Events (game thread, one frame after the physics step that produced them)

	UPROPERTY(BlueprintAssignable, Category = "Race")
	FOnRaceOvertake OnOvertake;

	UPROPERTY(BlueprintAssignable, Category = "Race")
	FOnRaceLapCompleted OnLapCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Race")
	FOnRacerFinished OnRacerFinished;

	/** The single end-of-race event, for the results screen and Adventure progression. */
	UPROPERTY(BlueprintAssignable, Category = "Race")
	FOnRaceFinished OnRaceFinished;

protected:
	virtual void BeginPlay() override;

private:
	void ApplySnapshot();
	void BroadcastEvents();
	void BuildResults();
	void MirrorToPlayerStates() const;

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<URaceTrackerComponent> Tracker;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Race")
	ERacePhase RacePhase = ERacePhase::Inactive;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Race")
	int32 NumLaps = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Race")
	float RaceTime = 0.f;

	/** Leader first. */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Race")
	TArray<FRacerStanding> Standings;

	UPROPERTY(VisibleInstanceOnly, Category = "Race")
	FRaceGap LeaderToSecondGap;

	UPROPERTY(VisibleInstanceOnly, Category = "Race")
	FRaceResultData Results;

	bool bResultsBroadcast = false;
};
