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
		TEXT("Test helper: what the main menu map does by itself, without a screen. \"SingleRace\" = start a single race with the character of ch.Test.AutoSelectCharacter (setup: ch.Test.AutoSetup), \"RaceSetup\" = open the race setup screen, \"Adventure\" = new game or load (ch.Test.AdventureNew, ch.Test.AdventureSlot) and start the level ch.Test.AdventureLevel. Empty = nothing."), ECVF_Cheat);

	TAutoConsoleVariable<FString> AutoSetup(
		TEXT("ch.Test.AutoSetup"), TEXT(""),
		TEXT("Test helper: the Single Race setup ch.Test.AutoMenu uses, as \"rivals:laps:difficulty:items\" (e.g. \"3:2:2:0\"; laps 0 = track default). Empty = the default race."), ECVF_Cheat);

	TAutoConsoleVariable<FString> MenuKeys(
		TEXT("ch.Test.MenuKeys"), TEXT(""),
		TEXT("Test helper: keys pressed through Slate like a player would, in batches \"time@Key:Key\" separated by '/', e.g. \"1.5@Left:Down/4@Enter\" (seconds since the game started ticking; FKey names such as Gamepad_DPad_Left). Needs a window."), ECVF_Cheat);

	TAutoConsoleVariable<float> WallScreenshotTime(
		TEXT("ch.Test.WallScreenshotTime"), 0.f,
		TEXT("Test helper: take one screenshot at this wall-clock time in seconds since the game started ticking (also while paused). 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<FString> ExecAt(
		TEXT("ch.Test.ExecAt"), TEXT(""),
		TEXT("Test helper: one console command run at a wall-clock time (s since the game started ticking), as \"6@ch.Adventure.UnlockAll 1\"."), ECVF_Cheat);

	TAutoConsoleVariable<float> FrameStats(
		TEXT("ch.Test.FrameStats"), 0.f,
		TEXT("Test helper: collect every frame time from this wall-clock time (s) on and log average, median, p95, p99, max and hitch count when the game quits. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<int32> RaceHudEnabled(
		TEXT("ch.Hud.Enabled"), 1,
		TEXT("Dev: 0 = the race HUD widget is not created at all (to measure what it costs). Read when the race starts."), ECVF_Cheat);

	TAutoConsoleVariable<float> WallQuitTime(
		TEXT("ch.Test.WallQuitTime"), 0.f,
		TEXT("Test helper: quit at this wall-clock time in seconds since the game started ticking (also while paused). 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AdventureSlot(
		TEXT("ch.Test.AdventureSlot"), 0,
		TEXT("Test helper for ch.Test.AutoMenu Adventure: the save slot (0..2)."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AdventureNew(
		TEXT("ch.Test.AdventureNew"), 0,
		TEXT("Test helper for ch.Test.AutoMenu Adventure: 1 = start a new game in the slot, 0 = load it."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AdventureLevel(
		TEXT("ch.Test.AdventureLevel"), 0,
		TEXT("Test helper for ch.Test.AutoMenu Adventure: index of the level to start (0 = first)."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AdventureHub(
		TEXT("ch.Test.AdventureHub"), 0,
		TEXT("Test helper for ch.Test.AutoMenu Adventure: 1 = open the hub instead of starting the level's race."), ECVF_Cheat);

	TAutoConsoleVariable<int32> HubEnterLevel(
		TEXT("ch.Test.HubEnterLevel"), -1,
		TEXT("Test helper: in the hub, walk into the portal of this level once per run (as the Interact button does). Negative = off."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AutoReturnToHub(
		TEXT("ch.Test.AutoReturnToHub"), 0,
		TEXT("Test helper: 1 = after an Adventure race go straight back to the hub (as the results screen's buttons do)."), ECVF_Cheat);

	TAutoConsoleVariable<int32> AICharacterSeed(
		TEXT("ch.Race.AICharacterSeed"), -1,
		TEXT("Seed of the draw that gives the rivals of a Single Race their characters. Negative = a new draw every time."), ECVF_Cheat);
}
