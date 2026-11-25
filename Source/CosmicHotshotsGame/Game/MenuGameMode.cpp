#include "Game/MenuGameMode.h"

#include "CosmicHotshotsGame.h"
#include "Character/CharacterDefinition.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Misc/App.h"
#include "Race/RaceDebug.h"
#include "UI/MenuScreens.h"
#include "UnrealClient.h"
#include "Vehicle/VehicleDebug.h"

namespace
{
	/** Value of ch.Test.AutoMenu that starts a single race. */
	const TCHAR* const AutoMenuSingleRace = TEXT("SingleRace");
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
		CosmicHotshots::RaceUI::StartRaceWithCharacter(this, PickedCharacter);
		return;
	}
	if (!AutoMenu.IsEmpty())
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("ch.Test.AutoMenu: unknown value '%s'."), *AutoMenu);
	}
#endif

	// No window (headless test): nothing to draw on.
	if (!FApp::CanEverRender())
	{
		return;
	}

	MenuWidget = CreateWidget<UMainMenuWidget>(this, UMainMenuWidget::StaticClass());
	MenuWidget->ShowScreen();
}

void AMenuPlayerController::TickTestHelpers(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	// The race maps run these helpers from the vehicle; the menu map has none.
	MenuTime += DeltaTime;

	const float ScreenshotTime = VehicleCVars::ScreenshotTime.GetValueOnGameThread();
	if (ScreenshotTime > 0.f && !bMenuScreenshotTaken && MenuTime >= ScreenshotTime)
	{
		bMenuScreenshotTaken = true;
		FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/true);
	}

	// With ch.Test.AutoMenu the test goes on in the race map, which quits by its own clock.
	const float AutoQuitTime = VehicleCVars::AutoQuitTime.GetValueOnGameThread();
	if (AutoQuitTime > 0.f && MenuTime >= AutoQuitTime && RaceCVars::AutoMenu.GetValueOnGameThread().IsEmpty())
	{
		FPlatformMisc::RequestExit(false);
	}
#endif
}
