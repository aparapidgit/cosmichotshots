// Test driver (dev builds only): lets a scripted run of the real game press keys, take a screenshot and quit
// on a wall clock that keeps running while the game is paused and across map changes.
//
// The vehicle's own test helpers (ch.Test.ScreenshotTime, ch.Test.AutoQuitTime) count simulation time,
// which stands still in the pause menu; menu and pause tests use these instead:
//   ch.Test.MenuKeys "1.5@Left:Down/4@Enter"   keys pressed through Slate, like a player would
//   ch.Test.ExecAt "6@ch.Adventure.UnlockAll 1" a console command typed at that moment
//   ch.Test.WallScreenshotTime 6               one screenshot (with UI)
//   ch.Test.WallQuitTime 8                     quit
// Times are seconds since the game started ticking.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TestDriverSubsystem.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API UTestDriverSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	/** Seconds since the game started ticking (wall clock, each frame capped so a loading hitch does not skip ahead). */
	float WallTime = 0.f;

	/** Batches of ch.Test.MenuKeys already pressed. */
	int32 NumKeyBatchesSent = 0;

	/** Keys still to press, in order, and the key that is down in this frame. */
	TArray<FString> PendingKeys;
	FString HeldKey;

	bool bScreenshotTaken = false;

	/** ch.Test.ExecAt has run its command. */
	bool bExecDone = false;

	/** ch.Test.FrameStats: frame times (s) collected since the start time, logged as one line when the game quits. */
	TArray<float> FrameTimes;

	void LogFrameStats();

public:
	virtual void Deinitialize() override;
};
