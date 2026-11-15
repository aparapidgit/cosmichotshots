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

	/** Test helper: spawn a stationary dummy car this far ahead of the player (cm). 0 = off. */
	extern TAutoConsoleVariable<float> SpawnDummyAhead;

	/** Test helper: sideways offset of the dummy car (cm, positive = to the right). */
	extern TAutoConsoleVariable<float> DummyLateralOffset;

	/** Test helper: roster index of the character given to the player's car at start. Negative = leave as is. */
	extern TAutoConsoleVariable<int32> PlayerCharacter;

	/** Test helper: roster index of the character given to the dummy car. Negative = project default. */
	extern TAutoConsoleVariable<int32> DummyCharacter;

	/** Test helper: take one screenshot at this simulation time (seconds). 0 = off. */
	extern TAutoConsoleVariable<float> ScreenshotTime;

	/** Test helper: ignore the driver and feed a fixed input sequence. 1 = drive test, 2 = drift and boost test, 3 = wall hit test. */
	extern TAutoConsoleVariable<int32> ScriptedInput;

	/** Returns the console override when it is set (>= 0), otherwise the data asset value. */
	inline float Resolve(const TAutoConsoleVariable<float>& CVar, float AssetValue)
	{
		const float Override = CVar.GetValueOnGameThread();
		return Override >= 0.f ? Override : AssetValue;
	}
}
