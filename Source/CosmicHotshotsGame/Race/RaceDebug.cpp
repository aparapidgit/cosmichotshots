#include "Race/RaceDebug.h"

namespace CosmicHotshots::RaceCVars
{
	TAutoConsoleVariable<int32> Laps(
		TEXT("ch.Race.Laps"), -1,
		TEXT("Laps per race. Negative = use the race tuning data asset. Change it before anyone completes a lap."), ECVF_Cheat);

	TAutoConsoleVariable<int32> DebugStandings(
		TEXT("ch.Debug.Race"), 0,
		TEXT("1 = draw the race standings table (rank, lap, progress, gaps) on the dev HUD."), ECVF_Cheat);

	TAutoConsoleVariable<int32> DebugTrack(
		TEXT("ch.Debug.TrackSpline"), 0,
		TEXT("1 = draw the track line, the checkpoints and each car's projection onto the line."), ECVF_Cheat);
}

namespace CosmicHotshots::RaceCVars
{
	TAutoConsoleVariable<int32> DebugPassives(
		TEXT("ch.Debug.Passives"), 0,
		TEXT("1 = draw the own car's passive state and active modifiers (strength, time left) on the dev HUD."), ECVF_Cheat);
}

namespace CosmicHotshots::RaceCVars
{
	TAutoConsoleVariable<int32> AIOpponents(
		TEXT("ch.Race.AIOpponents"), 0,
		TEXT("Computer drivers put on the free grid slots (0..7). Read when the race starts; stands in for the race setup menu."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AIDifficulty(
		TEXT("ch.Race.AIDifficulty"), -1,
		TEXT("Difficulty index of the computer drivers (Project Settings > Cosmic Hotshots > AI). Negative = the project default."), ECVF_Cheat);
}

namespace CosmicHotshots::RaceCVars
{
	TAutoConsoleVariable<int32> ItemBoxes(
		TEXT("ch.Race.ItemBoxes"), 1,
		TEXT("1 = item boxes are in the race, 0 = no boxes. Read when the race starts; stands in for the race setup option."), ECVF_Cheat);
}

namespace CosmicHotshots::RaceCVars
{
	TAutoConsoleVariable<float> Countdown(
		TEXT("ch.Race.Countdown"), -1.f,
		TEXT("Seconds of start countdown. Negative = use the race tuning data asset. 0 = the race starts at once."), ECVF_Cheat);

	TAutoConsoleVariable<int32> SkipMenus(
		TEXT("ch.Flow.SkipMenus"), 0,
		TEXT("1 = no character select: the race starts straight away, set up by the ch.Race.* console variables."), ECVF_Cheat);
}

namespace CosmicHotshots::RaceCVars
{
	TAutoConsoleVariable<int32> AutoSelectCharacter(
		TEXT("ch.Test.AutoSelectCharacter"), -1,
		TEXT("Test helper: on the character select, pick this roster index at once (no screen). Negative = off."), ECVF_Cheat);

	TAutoConsoleVariable<FString> AutoMenu(
		TEXT("ch.Test.AutoMenu"), TEXT(""),
		TEXT("Test helper: what the main menu map does by itself, without a screen. \"SingleRace\" = start a single race with the character of ch.Test.AutoSelectCharacter. Empty = nothing."), ECVF_Cheat);
}
