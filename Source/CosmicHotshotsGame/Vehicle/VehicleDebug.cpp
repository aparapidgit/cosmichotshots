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

	TAutoConsoleVariable<int32> DebugDraw(
		TEXT("ch.Debug.Vehicle"), 0,
		TEXT("1 = draw hover traces and print vehicle state on screen."), ECVF_Cheat);

	TAutoConsoleVariable<float> LogInterval(
		TEXT("ch.Debug.VehicleLog"), 0.f,
		TEXT("Seconds of simulation time between vehicle state lines in the log. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<float> AutoQuitTime(
		TEXT("ch.Test.AutoQuitTime"), 0.f,
		TEXT("Test helper: quit after this many seconds of simulation time. 0 = off."), ECVF_Cheat);

	TAutoConsoleVariable<int32> ScriptedInput(
		TEXT("ch.Test.ScriptedInput"), 0,
		TEXT("Test helper: 1 = ignore the driver and feed a fixed input sequence."), ECVF_Cheat);
}
