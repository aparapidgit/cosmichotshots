#include "Vehicle/VehicleDebug.h"

namespace CosmicHotshots::VehicleCVars
{
	static constexpr float UseAssetValue = -1.f;

	TAutoConsoleVariable<float> RideHeight(
		TEXT("ch.Vehicle.RideHeight"), UseAssetValue,
		TEXT("Override hover ride height in cm. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> HoverFrequency(
		TEXT("ch.Vehicle.HoverFrequency"), UseAssetValue,
		TEXT("Override hover spring frequency in Hz. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> HoverDamping(
		TEXT("ch.Vehicle.HoverDamping"), UseAssetValue,
		TEXT("Override hover damping ratio. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> Gravity(
		TEXT("ch.Vehicle.Gravity"), UseAssetValue,
		TEXT("Override vehicle gravity in cm/s^2. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> MaxSpeedKmh(
		TEXT("ch.Vehicle.MaxSpeed"), UseAssetValue,
		TEXT("Override top speed in km/h. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> Acceleration(
		TEXT("ch.Vehicle.Acceleration"), UseAssetValue,
		TEXT("Override forward acceleration in cm/s^2. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> MaxYawRate(
		TEXT("ch.Vehicle.MaxYawRate"), UseAssetValue,
		TEXT("Override turn rate in degrees per second. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> YawResponse(
		TEXT("ch.Vehicle.YawResponse"), UseAssetValue,
		TEXT("Override steering response in 1/s. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> LateralGrip(
		TEXT("ch.Vehicle.LateralGrip"), UseAssetValue,
		TEXT("Override sideways grip in 1/s. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> DriftCriticalAngle(
		TEXT("ch.Vehicle.DriftCriticalAngle"), UseAssetValue,
		TEXT("Override the critical drift slip angle in degrees. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<float> DriftGrip(
		TEXT("ch.Vehicle.DriftGrip"), UseAssetValue,
		TEXT("Override sideways grip while drifting in 1/s. Negative = use the tuning data asset."), ECVF_Cheat);

	TAutoConsoleVariable<int32> InfiniteBoost(
		TEXT("ch.Vehicle.InfiniteBoost"), 0,
		TEXT("1 = the boost meter is always full."), ECVF_Cheat);

	TAutoConsoleVariable<int32> DebugDraw(
		TEXT("ch.Debug.Vehicle"), 0,
		TEXT("1 = draw hover traces and print vehicle state on screen."), ECVF_Cheat);

	TAutoConsoleVariable<float> LogInterval(
		TEXT("ch.Debug.VehicleLog"), 0.f,
		TEXT("Seconds of simulation time between vehicle state lines in the log. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<float> AutoQuitTime(
		TEXT("ch.Test.AutoQuitTime"), 0.f,
		TEXT("Test helper: quit after this many seconds of simulation time. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<float> SpawnDummyAhead(
		TEXT("ch.Test.SpawnDummyAhead"), 0.f,
		TEXT("Test helper: spawn a stationary dummy car this far ahead of the player in cm. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<float> DummyLateralOffset(
		TEXT("ch.Test.DummyLateralOffset"), 0.f,
		TEXT("Test helper: sideways offset of the dummy car in cm (positive = to the right)."), ECVF_Cheat);

	TAutoConsoleVariable<int32> PlayerCharacter(
		TEXT("ch.Test.PlayerCharacter"), -1,
		TEXT("Test helper: roster index of the character given to the player's car at start. Negative = leave as is."), ECVF_Cheat);

	TAutoConsoleVariable<int32> DummyCharacter(
		TEXT("ch.Test.DummyCharacter"), -1,
		TEXT("Test helper: roster index of the character given to the dummy car. Negative = project default."), ECVF_Cheat);

	TAutoConsoleVariable<int32> TestBots(
		TEXT("ch.Test.Bots"), 0,
		TEXT("Test helper: number of test bots (0..7) on the free grid slots, driving the test autopilot. Set at startup (-DPCVars) or before the level loads."), ECVF_Cheat);

	TAutoConsoleVariable<float> DummyYawOffset(
		TEXT("ch.Test.DummyYawOffset"), 0.f,
		TEXT("Test helper: the dummy car is turned by this many degrees (90 = side on to the player)."), ECVF_Cheat);

	TAutoConsoleVariable<int32> BotCharacter(
		TEXT("ch.Test.BotCharacter"), -1,
		TEXT("Test helper: roster index of the character every test bot gets. Negative = one after the other through the roster."), ECVF_Cheat);

	TAutoConsoleVariable<int32> PlayerGridSlot(
		TEXT("ch.Test.PlayerGridSlot"), -1,
		TEXT("Test helper: grid slot (0 = pole) the first player starts from. Negative = normal order. Set at startup (-DPCVars)."), ECVF_Cheat);

	TAutoConsoleVariable<float> TestSpeedModifier(
		TEXT("ch.Test.SpeedModifier"), 0.f,
		TEXT("Test helper: max-speed multiplier added to the player's modifier stack at 2 s (1 s fade in, 2 s hold, 1 s fade out). 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<float> StartBoostCharge(
		TEXT("ch.Test.StartBoostCharge"), -1.f,
		TEXT("Test helper: boost meter value (0..1) given to the player's car at start. Negative = leave as is."), ECVF_Cheat);

	TAutoConsoleVariable<float> ScreenshotTime(
		TEXT("ch.Test.ScreenshotTime"), 0.f,
		TEXT("Test helper: take one screenshot at this simulation time in seconds. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<int32> ScriptedInput(
		TEXT("ch.Test.ScriptedInput"), 0,
		TEXT("Test helper: ignore the driver and feed a fixed input sequence. 1 = drive test, 2 = drift and boost test, 3 = wall hit test, 4 = close pass along a wall, 5 = straight past a dummy (overtake), 6 = autopilot along the track line (laps), 7 = lean into a dummy beside (side contact), 8 = autopilot with two long drifts (drift chain)."), ECVF_Cheat);
}
