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

namespace CosmicHotshots::RaceCVars
{
	/** Test helper: roster index picked at once on the character select (no screen). Negative = off. */
	extern TAutoConsoleVariable<int32> AutoSelectCharacter;

	/** Test helper: what the main menu map does by itself, without a screen ("SingleRace"). Empty = nothing. */
	extern TAutoConsoleVariable<FString> AutoMenu;

	/** Test helper: the Single Race setup ch.Test.AutoMenu uses, as "rivals:laps:difficulty:items". Empty = default race. */
	extern TAutoConsoleVariable<FString> AutoSetup;

	/** Test helper: keys pressed through Slate, as "1.5@Left:Down/4@Enter" (seconds since the game started ticking @ FKey names). See UTestDriverSubsystem. */
	extern TAutoConsoleVariable<FString> MenuKeys;

	/** Test helper: take one screenshot at this wall-clock time (s, also while paused). 0 = off. */
	extern TAutoConsoleVariable<float> WallScreenshotTime;

	/** Test helper: quit at this wall-clock time (s, also while paused). 0 = off. */
	extern TAutoConsoleVariable<float> WallQuitTime;

	/** Seed of the draw that gives the rivals of a Single Race their characters. Negative = a new draw every time. */
	extern TAutoConsoleVariable<int32> AICharacterSeed;
}
