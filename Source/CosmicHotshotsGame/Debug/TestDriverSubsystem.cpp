#include "Debug/TestDriverSubsystem.h"

#include "CosmicHotshotsGame.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Race/RaceDebug.h"
#include "UnrealClient.h"

namespace
{
	/** Longest frame the test clock counts (s): a loading hitch must not jump over a scripted moment. */
	constexpr float TestDriverMaxFrameTime = 0.1f;

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
			FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(FKey(*HeldKey), FModifierKeysState(), /*InUserIndex=*/0, /*bInIsRepeat=*/false, 0, 0));
			HeldKey.Reset();
		}
		else
		{
			HeldKey = PendingKeys[0];
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

	const float ScreenshotTime = RaceCVars::WallScreenshotTime.GetValueOnGameThread();
	if (ScreenshotTime > 0.f && !bScreenshotTaken && WallTime >= ScreenshotTime)
	{
		bScreenshotTaken = true;
		FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/true);
	}

	const float QuitTime = RaceCVars::WallQuitTime.GetValueOnGameThread();
	if (QuitTime > 0.f && WallTime >= QuitTime)
	{
		FPlatformMisc::RequestExit(false);
	}
#endif
}
