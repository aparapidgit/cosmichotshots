// Dev-only HUD: a plain boost meter bar so drift and boost can be judged while tuning,
// and the race standings table (ch.Debug.Race 1) so ranks, laps and gaps can be checked.
// It is NOT the game HUD (that is UMG, Phase 2); it only reads the vehicle's state
// and draws rectangles. Toggle with ch.Debug.BoostBar. Not drawn in shipping builds.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaceDebugHUD.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API ARaceDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** Standings table (ch.Debug.Race): rank, lap, checkpoint, progress, gaps. */
	void DrawRaceStandings();
};
