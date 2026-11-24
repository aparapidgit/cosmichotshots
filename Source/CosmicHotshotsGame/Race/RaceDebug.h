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

namespace CosmicHotshots::RaceCVars
{
	/** 1 = draw the own car's passive state and its active modifiers (strength, time left) on the dev HUD. */
	extern TAutoConsoleVariable<int32> DebugPassives;
}

namespace CosmicHotshots::RaceCVars
{
	/** Computer drivers put on the free grid slots (0..7) until the race setup menu exists. Read when the race starts. */
	extern TAutoConsoleVariable<int32> AIOpponents;

	/** Difficulty index of those computer drivers. Negative = the project default. */
	extern TAutoConsoleVariable<int32> AIDifficulty;
}

namespace CosmicHotshots::RaceCVars
{
	/** 1 = item boxes are in the race, 0 = no boxes (stands in for the race setup option). Read when the race starts. */
	extern TAutoConsoleVariable<int32> ItemBoxes;
}

namespace CosmicHotshots::RaceCVars
{
	/** Seconds of start countdown. Negative = use the race tuning asset. 0 = start at once (headless tests). */
	extern TAutoConsoleVariable<float> Countdown;

	/** 1 = no character select: the race starts straight away with the dev console variables (headless tests). */
	extern TAutoConsoleVariable<int32> SkipMenus;
}
