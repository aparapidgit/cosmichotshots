// Race player state: a player's own copy of their race standing (rank, lap, finish).
// ARaceGameState is the source of truth for every driver; this mirror exists so UI and,
// later, network clients can read a player's race state from their player state.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Race/RaceTypes.h"
#include "RacePlayerState.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API ARacePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called by ARaceGameState every frame the race state changes. */
	void SetRaceStanding(const FRacerStanding& Standing);

	int32 GetRaceRank() const { return RaceRank; }
	int32 GetCurrentLap() const { return CurrentLap; }
	bool HasFinishedRace() const { return bFinishedRace; }
	float GetFinishTime() const { return FinishTime; }

protected:
	/** 1 = leader, 0 = not racing. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Race")
	int32 RaceRank = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Race")
	int32 CurrentLap = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Race")
	bool bFinishedRace = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Race")
	float FinishTime = 0.f;
};
