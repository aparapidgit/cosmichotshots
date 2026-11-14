// Live tuning and debug switches for vehicles (console variables, dev only use).

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace CosmicHotshots::VehicleCVars
{
	/** Tuning overrides. A negative value means "use the data asset". */
	extern TAutoConsoleVariable<float> RideHeight;
	extern TAutoConsoleVariable<float> HoverFrequency;
	extern TAutoConsoleVariable<float> HoverDamping;
	extern TAutoConsoleVariable<float> Gravity;
	extern TAutoConsoleVariable<float> MaxSpeedKmh;
	extern TAutoConsoleVariable<float> Acceleration;
	extern TAutoConsoleVariable<float> MaxYawRate;
	extern TAutoConsoleVariable<float> YawResponse;
	extern TAutoConsoleVariable<float> LateralGrip;
	extern TAutoConsoleVariable<float> DriftGrip;

	/** 1 = the boost meter is always full (tuning boosts without having to drift first). */
	extern TAutoConsoleVariable<int32> InfiniteBoost;

	/** 1 = draw hover traces and print vehicle state on screen. */
	extern TAutoConsoleVariable<int32> DebugDraw;

	/** Seconds of simulation time between state lines in the log. 0 = off. */
	extern TAutoConsoleVariable<float> LogInterval;

	/** Test helper: quit the game after this many seconds of simulation time. 0 = off. */
	extern TAutoConsoleVariable<float> AutoQuitTime;

	/** Test helper: ignore the driver and feed a fixed input sequence. 1 = drive test, 2 = drift and boost test. */
	extern TAutoConsoleVariable<int32> ScriptedInput;

	/** Returns the console override when it is set (>= 0), otherwise the data asset value. */
	inline float Resolve(const TAutoConsoleVariable<float>& CVar, float AssetValue)
	{
		const float Override = CVar.GetValueOnGameThread();
		return Override >= 0.f ? Override : AssetValue;
	}
}
