#include "Game/MenuGameMode.h"

#include "CosmicHotshotsGame.h"
#include "Character/CharacterDefinition.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/GameFlowSubsystem.h"
#include "Misc/App.h"
#include "Race/RaceDebug.h"
#include "UI/MenuScreens.h"
#include "UnrealClient.h"
#include "Vehicle/VehicleDebug.h"

namespace
{
	/** Value of ch.Test.AutoMenu that starts a single race. */
	const TCHAR* const AutoMenuSingleRace = TEXT("SingleRace");

	/** Value of ch.Test.AutoMenu that opens the race setup screen (for screenshots). */
	const TCHAR* const AutoMenuRaceSetup = TEXT("RaceSetup");

	/** Fields of ch.Test.AutoSetup: rivals, laps, difficulty, items. */
	constexpr int32 AutoSetupNumParts = 4;

	/** Seconds after the menu map started at which ch.Test.MenuKeys are pressed (the screen is up by then). */
	constexpr float MenuKeysTime = 1.5f;
}

AMenuGameMode::AMenuGameMode()
{
	// A menu only: nobody to drive and nothing to draw besides the menu screens.
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = AMenuPlayerController::StaticClass();
}

void AMenuPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!bMenuSetUp)
	{
		bMenuSetUp = true;
		SetUpMenu();
	}
	TickTestHelpers(DeltaTime);
}

void AMenuPlayerController::SetUpMenu()
{
	if (!IsLocalController())
	{
		return;
	}
	UE_LOG(LogCosmicRace, Log, TEXT("FLOW main menu: map started"));

#if !UE_BUILD_SHIPPING
	// Test helper (ch.Test.AutoMenu): do what the menu buttons would do, without a screen.
	const FString AutoMenu = CosmicHotshots::RaceCVars::AutoMenu.GetValueOnGameThread();
	if (AutoMenu.Equals(AutoMenuSingleRace, ESearchCase::IgnoreCase))
	{
		const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
		const int32 AutoSelect = CosmicHotshots::RaceCVars::AutoSelectCharacter.GetValueOnGameThread();
		const TArray<const UCharacterDefinition*> RaceCharacters = Settings->GetRaceCharacters();
		const UCharacterDefinition* PickedCharacter = AutoSelect >= 0 ? Settings->FindCharacter(FString::FromInt(AutoSelect))
			: (RaceCharacters.IsEmpty() ? nullptr : RaceCharacters[0]);
		UE_LOG(LogCosmicRace, Log, TEXT("FLOW main menu: auto Single Race with %s"), *GetNameSafe(PickedCharacter));

		// ch.Test.AutoSetup "rivals:laps:difficulty:items": what the setup screen would write. Empty = the default race.
		TArray<FString> SetupParts;
		CosmicHotshots::RaceCVars::AutoSetup.GetValueOnGameThread().ParseIntoArray(SetupParts, TEXT(":"));
		UGameFlowSubsystem* AutoFlow = UGameFlowSubsystem::Get(this);
		if (SetupParts.Num() == AutoSetupNumParts && AutoFlow)
		{
			FRaceSessionConfig& Setup = AutoFlow->EditSingleRaceSetup();
			Setup.HumanCharacters = { PickedCharacter };
			Setup.NumAIOpponents = FCString::Atoi(*SetupParts[0]);
			Setup.NumLaps = FCString::Atoi(*SetupParts[1]);
			Setup.AIDifficulty = FCString::Atoi(*SetupParts[2]);
			Setup.bItemBoxes = FCString::Atoi(*SetupParts[3]) != 0;
			AutoFlow->StartSingleRace(this);
			return;
		}
		CosmicHotshots::RaceUI::StartRaceWithCharacter(this, PickedCharacter);
		return;
	}
	const bool bAutoRaceSetup = AutoMenu.Equals(AutoMenuRaceSetup, ESearchCase::IgnoreCase);
	if (!AutoMenu.IsEmpty() && !bAutoRaceSetup)
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("ch.Test.AutoMenu: unknown value '%s'."), *AutoMenu);
	}
#else
	const bool bAutoRaceSetup = false;
#endif

	// "Change setup" after a race comes back here and wants the race setup on top of the menu.
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	const bool bOpenRaceSetup = (Flow && Flow->ConsumeRaceSetupRequest()) || bAutoRaceSetup;
	if (bOpenRaceSetup)
	{
		UE_LOG(LogCosmicRace, Log, TEXT("FLOW main menu: opening the race setup"));
	}

	// No window (headless test): nothing to draw on.
	if (!FApp::CanEverRender())
	{
		return;
	}

	UMainMenuWidget* MainMenu = CreateWidget<UMainMenuWidget>(this, UMainMenuWidget::StaticClass());
	MenuWidget = MainMenu;
	MainMenu->ShowScreen();
	if (bOpenRaceSetup)
	{
		MainMenu->OpenRaceSetup();
	}
}

void AMenuPlayerController::TickTestHelpers(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	// The race maps run these helpers from the vehicle; the menu map has none.
	MenuTime += DeltaTime;

	// ch.Test.MenuKeys: press keys the way a player would, through Slate, so focus and option rows react for real.
	const FString MenuKeys = RaceCVars::MenuKeys.GetValueOnGameThread();
	if (!MenuKeys.IsEmpty() && !bMenuKeysSent && MenuTime >= MenuKeysTime && FSlateApplication::IsInitialized())
	{
		bMenuKeysSent = true;
		TArray<FString> KeyNames;
		MenuKeys.ParseIntoArray(KeyNames, TEXT(":"));
		for (const FString& KeyName : KeyNames)
		{
			const FKeyEvent KeyEvent(FKey(*KeyName), FModifierKeysState(), /*InUserIndex=*/0, /*bInIsRepeat=*/false, 0, 0);
			FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
			FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);
		}
		UE_LOG(LogCosmicRace, Log, TEXT("FLOW main menu: sent test keys %s"), *MenuKeys);
	}

	const float ScreenshotTime = VehicleCVars::ScreenshotTime.GetValueOnGameThread();
	if (ScreenshotTime > 0.f && !bMenuScreenshotTaken && MenuTime >= ScreenshotTime)
	{
		bMenuScreenshotTaken = true;
		FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/true);
	}

	// With ch.Test.AutoMenu SingleRace the test goes on in the race map, which quits by its own clock.
	const float AutoQuitTime = VehicleCVars::AutoQuitTime.GetValueOnGameThread();
	if (AutoQuitTime > 0.f && MenuTime >= AutoQuitTime && !RaceCVars::AutoMenu.GetValueOnGameThread().Equals(AutoMenuSingleRace, ESearchCase::IgnoreCase))
	{
		FPlatformMisc::RequestExit(false);
	}
#endif
}
