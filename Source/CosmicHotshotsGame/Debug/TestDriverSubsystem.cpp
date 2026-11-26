#include "Debug/TestDriverSubsystem.h"

#include "CosmicHotshotsGame.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Race/RaceDebug.h"
#include "UnrealClient.h"

namespace
{
	/** Longest frame the test clock counts (s): a loading hitch must not jump over a scripted moment. */
	constexpr float TestDriverMaxFrameTime = 0.1f;

	/** A frame this many times longer than the median frame counts as a hitch in the frame statistics. */
	constexpr float TestDriverHitchFactor = 1.5f;
	constexpr float TestDriverMsPerSecond = 1000.f;

	/** Time of a ch.Test.MenuKeys batch written without "time@" (s). */
	constexpr float TestDriverDefaultKeyTime = 1.5f;
}

TStatId UTestDriverSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTestDriverSubsystem, STATGROUP_Tickables);
}

UWorld* UTestDriverSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void UTestDriverSubsystem::Tick(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	WallTime += FMath::Min(static_cast<float>(FApp::GetDeltaTime()), TestDriverMaxFrameTime);

	// Keys are pressed one at a time and held for one frame (down in one frame, up in the next): the game's
	// input is sampled once per frame, so a key pressed and released within a frame would never be seen.
	if (FSlateApplication::IsInitialized() && (!HeldKey.IsEmpty() || !PendingKeys.IsEmpty()))
	{
		if (!HeldKey.IsEmpty())
		{
			if (--HeldFramesRemaining <= 0)
			{
				FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(FKey(*HeldKey), FModifierKeysState(), /*InUserIndex=*/0, /*bInIsRepeat=*/false, 0, 0));
				HeldKey.Reset();
			}
		}
		else
		{
			// "Key*Frames" holds the key that many frames; a plain key is down for one frame.
			FString FramesText;
			HeldKey = PendingKeys[0];
			HeldFramesRemaining = 1;
			if (PendingKeys[0].Split(TEXT("*"), &HeldKey, &FramesText))
			{
				HeldFramesRemaining = FMath::Max(FCString::Atoi(*FramesText), 1);
			}
			PendingKeys.RemoveAt(0);
			FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(FKey(*HeldKey), FModifierKeysState(), /*InUserIndex=*/0, /*bInIsRepeat=*/false, 0, 0));
		}
	}

	// ch.Test.MenuKeys: batches "time@Key:Key" separated by '/', in order.
	const FString MenuKeys = RaceCVars::MenuKeys.GetValueOnGameThread();
	if (!MenuKeys.IsEmpty() && FSlateApplication::IsInitialized())
	{
		TArray<FString> Batches;
		MenuKeys.ParseIntoArray(Batches, TEXT("/"));
		if (Batches.IsValidIndex(NumKeyBatchesSent))
		{
			FString TimeText;
			FString KeysText = Batches[NumKeyBatchesSent];
			float BatchTime = TestDriverDefaultKeyTime;
			if (Batches[NumKeyBatchesSent].Split(TEXT("@"), &TimeText, &KeysText))
			{
				BatchTime = FCString::Atof(*TimeText);
			}
			if (WallTime >= BatchTime)
			{
				++NumKeyBatchesSent;
				TArray<FString> KeyNames;
				KeysText.ParseIntoArray(KeyNames, TEXT(":"));

				// A test window started in the background has no keyboard focus at all; a player's window has
				// it on the game. Menus set their own focus, so only fill in the missing case.
				const TSharedPtr<SWidget> FocusedBefore = FSlateApplication::Get().GetUserFocusedWidget(0);
				if (!FocusedBefore.IsValid())
				{
					FSlateApplication::Get().SetAllUserFocusToGameViewport();
				}
				const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
				UE_LOG(LogCosmicRace, Log, TEXT("TEST keys focus: %s%s"), Focused.IsValid() ? *Focused->GetTypeAsString() : TEXT("none"),
					FocusedBefore.IsValid() ? TEXT("") : TEXT(" (was none)"));

				PendingKeys.Append(KeyNames);
				UE_LOG(LogCosmicRace, Log, TEXT("TEST keys at %.1f s: %s"), BatchTime, *KeysText);
			}
		}
	}

	// ch.Test.ExecAt "time@command": what a developer would type into the console at that moment.
	FString ExecTimeText;
	FString ExecCommand;
	if (!bExecDone && RaceCVars::ExecAt.GetValueOnGameThread().Split(TEXT("@"), &ExecTimeText, &ExecCommand) && WallTime >= FCString::Atof(*ExecTimeText))
	{
		bExecDone = true;
		UE_LOG(LogCosmicRace, Log, TEXT("TEST exec at %s s: %s"), *ExecTimeText, *ExecCommand);
		if (GEngine)
		{
			GEngine->Exec(GetTickableGameObjectWorld(), *ExecCommand);
		}
	}

	// ch.Test.FrameStats N: collect every frame time from wall second N on (after loading and the start).
	const float FrameStatsStart = RaceCVars::FrameStats.GetValueOnGameThread();
	if (FrameStatsStart > 0.f && WallTime >= FrameStatsStart)
	{
		FrameTimes.Add(static_cast<float>(FApp::GetDeltaTime()));
	}

	const float ScreenshotTime = RaceCVars::WallScreenshotTime.GetValueOnGameThread();
	if (ScreenshotTime > 0.f && !bScreenshotTaken && WallTime >= ScreenshotTime)
	{
		bScreenshotTaken = true;
		FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/true);
	}

	const float QuitTime = RaceCVars::WallQuitTime.GetValueOnGameThread();
	if (QuitTime > 0.f && WallTime >= QuitTime)
	{
		LogFrameStats();
		FPlatformMisc::RequestExit(false);
	}
#endif
}

void UTestDriverSubsystem::Deinitialize()
{
	LogFrameStats();
	Super::Deinitialize();
}

void UTestDriverSubsystem::LogFrameStats()
{
	if (FrameTimes.IsEmpty())
	{
		return;
	}

	// How even the frame rate was: the average hides hitches, the slowest frames show them.
	TArray<float> Sorted = FrameTimes;
	Sorted.Sort();
	float Total = 0.f;
	int32 NumHitches = 0;
	const float Median = Sorted[Sorted.Num() / 2];
	for (const float FrameTime : Sorted)
	{
		Total += FrameTime;
		NumHitches += FrameTime > Median * TestDriverHitchFactor ? 1 : 0;
	}
	UE_LOG(LogCosmicRace, Log, TEXT("TEST frames=%d avgMs=%.1f medianMs=%.1f p95Ms=%.1f p99Ms=%.1f maxMs=%.1f hitches=%d (frames over %.1fx the median)"),
		Sorted.Num(), Total / Sorted.Num() * TestDriverMsPerSecond, Median * TestDriverMsPerSecond,
		Sorted[FMath::Min(Sorted.Num() - 1, Sorted.Num() * 95 / 100)] * TestDriverMsPerSecond,
		Sorted[FMath::Min(Sorted.Num() - 1, Sorted.Num() * 99 / 100)] * TestDriverMsPerSecond,
		Sorted.Last() * TestDriverMsPerSecond, NumHitches, TestDriverHitchFactor);
	FrameTimes.Reset();
}
