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
