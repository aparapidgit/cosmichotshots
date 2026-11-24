// Live tuning and debug switches for the race rules (console variables, dev use).

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace CosmicHotshots::RaceCVars
{
	/** Laps per race. Negative = use the race tuning asset. */
	extern TAutoConsoleVariable<int32> Laps;

	/** 1 = draw the standings table (ranks, laps, gaps) on the dev HUD. */
	extern TAutoConsoleVariable<int32> DebugStandings;

	/** 1 = draw the track line, the checkpoints and every car's projection onto the line. */
	extern TAutoConsoleVariable<int32> DebugTrack;
}
