#include "Race/RacePlayerState.h"

#include "Net/UnrealNetwork.h"

void ARacePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARacePlayerState, RaceRank);
	DOREPLIFETIME(ARacePlayerState, CurrentLap);
	DOREPLIFETIME(ARacePlayerState, bFinishedRace);
	DOREPLIFETIME(ARacePlayerState, FinishTime);
}

void ARacePlayerState::SetRaceStanding(const FRacerStanding& Standing)
{
	RaceRank = Standing.Rank;
	CurrentLap = Standing.CurrentLap;
	bFinishedRace = Standing.bFinished;
	FinishTime = Standing.FinishTime;
}
