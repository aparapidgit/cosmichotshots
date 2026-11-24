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
