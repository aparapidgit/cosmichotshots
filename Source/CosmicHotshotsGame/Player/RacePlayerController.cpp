#include "Player/RacePlayerController.h"

#include "CosmicHotshotsGame.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Game/GameFlowSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Adventure/AdventureSubsystem.h"
#include "Input/SystemInputConfig.h"
#include "Player/PlayerSettingsSubsystem.h"
#include "Race/RaceDebug.h"
#include "UI/MenuScreens.h"
#include "UI/RaceHudWidget.h"

namespace
{
	/** The HUD is drawn under the start / finish overlay and under every menu screen. */
	constexpr int32 HudZOrder = -1;
}
#include "UI/RaceScreens.h"

#include "Item/ItemLootTable.h"
#include "Item/ItemSystemComponent.h"
#include "Race/RaceGameState.h"

#include "CosmicHotshotsGame.h"
#include "Camera/RaceCameraComponent.h"
#include "Character/CharacterDefinition.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Input/DrivingInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Vehicle/RaceVehicle.h"

void ARacePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// System actions (pause) do not depend on the driving config.
	SystemInput = GetDefault<UCosmicHotshotsSettings>()->SystemInput.LoadSynchronous();
	if (UEnhancedInputComponent* SystemEnhancedInput = Cast<UEnhancedInputComponent>(InputComponent); SystemEnhancedInput && SystemInput && SystemInput->Pause)
	{
		SystemEnhancedInput->BindAction(SystemInput->Pause, ETriggerEvent::Started, this, &ARacePlayerController::HandlePausePressed);
	}
	if (UEnhancedInputComponent* HudEnhancedInput = Cast<UEnhancedInputComponent>(InputComponent); HudEnhancedInput && SystemInput && SystemInput->ToggleHud)
	{
		HudEnhancedInput->BindAction(SystemInput->ToggleHud, ETriggerEvent::Started, this, &ARacePlayerController::HandleToggleHudPressed);
	}

	DrivingInput = GetDefault<UCosmicHotshotsSettings>()->DrivingInput.LoadSynchronous();
	if (!DrivingInput)
	{
		UE_LOG(LogCosmicVehicle, Warning,
			TEXT("No driving input config. Set Project Settings > Game > Cosmic Hotshots > Driving Input (run Scripts/create_phase1_assets.py to create the assets)."));
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogCosmicVehicle, Error, TEXT("Input component is not an EnhancedInputComponent. Check Config/DefaultInput.ini."));
		return;
	}

	// Values are polled every frame in PlayerTick, so plain value bindings are enough.
	const UInputAction* const Actions[] = {
		DrivingInput->Throttle, DrivingInput->Brake, DrivingInput->Steer,
		DrivingInput->DriftLeft, DrivingInput->DriftRight, DrivingInput->Boost, DrivingInput->Ability, DrivingInput->UseItem, DrivingInput->LookBack,
	};
	for (const UInputAction* Action : Actions)
	{
		if (Action)
		{
			EnhancedInput->BindActionValue(Action);
		}
	}
}

void ARacePlayerController::BeginPlay()
{
	Super::BeginPlay();

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputSubsystem)
	{
		return;
	}
	if (DrivingInput && DrivingInput->MappingContext)
	{
		InputSubsystem->AddMappingContext(DrivingInput->MappingContext, DrivingInput->MappingPriority);
	}
	if (SystemInput && SystemInput->MappingContext)
	{
		InputSubsystem->AddMappingContext(SystemInput->MappingContext, SystemInput->MappingPriority);
	}
}

void ARacePlayerController::HandleToggleHudPressed()
{
	if (UPlayerSettingsSubsystem* PlayerSettings = UPlayerSettingsSubsystem::Get(this))
	{
		PlayerSettings->SetHudVisible(!PlayerSettings->IsHudVisible());
		UE_LOG(LogCosmicRace, Log, TEXT("HUD visible=%d"), PlayerSettings->IsHudVisible() ? 1 : 0);
	}
}

void ARacePlayerController::HandlePausePressed()
{
	// Nothing to pause on: no window, already paused, or a screen is up (character select, results).
	if (!FApp::CanEverRender() || IsPaused() || (ScreenWidget && ScreenWidget->IsInViewport()))
	{
		return;
	}
	const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
	if (!RaceState || RaceState->GetRacePhase() != ERacePhase::Racing)
	{
		return;
	}

	if (!SetPause(true))
	{
		return;
	}
	UE_LOG(LogCosmicRace, Log, TEXT("FLOW pause: paused"));
	URaceScreenWidget* PauseMenu = CreateWidget<UPauseMenuWidget>(this, UPauseMenuWidget::StaticClass());
	PauseMenu->ShowScreen();
}

void ARacePlayerController::GiveItem(const FString& ItemId)
{
	ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn());
	const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
	UItemSystemComponent* Items = RaceState ? RaceState->GetItemSystem() : nullptr;
	const UItemLootTable* Table = Items ? Items->GetLootTable() : nullptr;
	const UItemDefinition* Item = Table ? Table->FindItem(FName(*ItemId)) : nullptr;
	if (!Vehicle || !Item)
	{
		UE_LOG(LogCosmicVehicle, Warning, TEXT("GiveItem: no vehicle, or the loot table has no item with id '%s'."), *ItemId);
		return;
	}
	Items->GiveItem(Vehicle, Item);
}

void ARacePlayerController::SetCharacter(const FString& IndexOrName)
{
	ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn());
	const UCharacterDefinition* NewCharacter = GetDefault<UCosmicHotshotsSettings>()->FindCharacter(IndexOrName);
	if (!Vehicle || !NewCharacter)
	{
		UE_LOG(LogCosmicVehicle, Warning, TEXT("SetCharacter: no vehicle, or no roster character matches '%s'."), *IndexOrName);
		return;
	}

	Vehicle->SetCharacter(NewCharacter);
	UE_LOG(LogCosmicVehicle, Log, TEXT("SetCharacter: %s is now %s."), *GetNameSafe(Vehicle), *GetNameSafe(NewCharacter));
}

void ARacePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(InPawn))
	{
		Vehicle->GetImpact()->OnImpact.AddUniqueDynamic(this, &ARacePlayerController::HandleVehicleImpact);
	}
}

void ARacePlayerController::OnUnPossess()
{
	if (const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn()))
	{
		Vehicle->GetImpact()->OnImpact.RemoveDynamic(this, &ARacePlayerController::HandleVehicleImpact);
	}

	Super::OnUnPossess();
}

void ARacePlayerController::HandleVehicleImpact(const FVehicleImpactInfo& ImpactInfo)
{
	if (!DrivingInput || !IsLocalController())
	{
		return;
	}

	const float Intensity = DrivingInput->ImpactRumbleMaxIntensity * ImpactInfo.Strength;
	if (Intensity > 0.f && DrivingInput->ImpactRumbleDuration > 0.f)
	{
		PlayDynamicForceFeedback(Intensity, DrivingInput->ImpactRumbleDuration,
			/*bAffectsLeftLarge=*/true, /*bAffectsLeftSmall=*/true, /*bAffectsRightLarge=*/true, /*bAffectsRightSmall=*/true);
	}
}

void ARacePlayerController::SetUpRaceScreens()
{
	if (!IsLocalController())
	{
		return;
	}

	// The player's Options (FOV, shake, speed lines) go to the camera of the car they drive. Here, in the first
	// tick, because console variables that redirect or switch off the settings file are not there earlier.
	if (UPlayerSettingsSubsystem* PlayerSettings = UPlayerSettingsSubsystem::Get(this))
	{
		const ARaceVehicle* OwnVehicle = Cast<ARaceVehicle>(GetPawn());
		PlayerSettings->ApplyToCamera(OwnVehicle ? OwnVehicle->GetRaceCamera() : nullptr);
	}

	ARaceGameState* RaceState = GetWorld()->GetGameState<ARaceGameState>();
	UGameFlowSubsystem* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr;
	if (!RaceState || !Flow)
	{
		return;
	}
	RaceState->OnRaceFinished.AddUniqueDynamic(this, &ARacePlayerController::HandleRaceFinished);

	const bool bWaitingForSetup = Flow->IsWaitingForRaceSetup();

#if !UE_BUILD_SHIPPING
	// Test helper (ch.Test.AutoSelectCharacter): pick a character without the screen, the way the button does.
	const int32 AutoSelect = CosmicHotshots::RaceCVars::AutoSelectCharacter.GetValueOnGameThread();
	if (bWaitingForSetup && AutoSelect >= 0)
	{
		const UCharacterDefinition* PickedCharacter = GetDefault<UCosmicHotshotsSettings>()->FindCharacter(FString::FromInt(AutoSelect));
		UE_LOG(LogCosmicRace, Log, TEXT("FLOW character select: auto-selected %s"), *GetNameSafe(PickedCharacter));
		CosmicHotshots::RaceUI::StartRaceWithCharacter(this, PickedCharacter);
		return;
	}
#endif

	// No window (headless test): nothing to draw on.
	if (!FApp::CanEverRender())
	{
		return;
	}

	OverlayWidget = CreateWidget<URaceOverlayWidget>(this, URaceOverlayWidget::StaticClass());
	OverlayWidget->AddToViewport();

	// This player's HUD, on this player's part of the screen (split-screen ready), under the overlay and the menus.
	if (CosmicHotshots::RaceCVars::RaceHudEnabled.GetValueOnGameThread() != 0)
	{
		HudWidget = CreateWidget<URaceHudWidget>(this, URaceHudWidget::StaticClass());
		HudWidget->AddToPlayerScreen(HudZOrder);
	}

	if (bWaitingForSetup)
	{
		ScreenWidget = CreateWidget<UCharacterSelectWidget>(this, UCharacterSelectWidget::StaticClass());
		ScreenWidget->ShowScreen();
	}
}

void ARacePlayerController::HandleRaceFinished(const FRaceResultData& Results)
{
	if (UGameFlowSubsystem* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr)
	{
		Flow->SetLastResults(Results);
	}
	UE_LOG(LogCosmicRace, Log, TEXT("FLOW results: %d drivers, race time %.3f"), Results.Entries.Num(), Results.RaceTime);

	// Adventure progression: a win unlocks the next level. Before the "no window" return, so headless tests see it.
	if (UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this))
	{
		Adventure->HandleRaceFinished(Results);

#if !UE_BUILD_SHIPPING
		// Test helper (ch.Test.AutoReturnToHub): what "Continue" / "Return to hub" on the results screen does.
		UGameFlowSubsystem* HubFlow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr;
		if (CosmicHotshots::RaceCVars::AutoReturnToHub.GetValueOnGameThread() != 0 && Adventure->GetCurrentLevel() != INDEX_NONE && HubFlow)
		{
			HubFlow->OpenAdventureHub(this);
			return;
		}
#endif
	}

	if (!FApp::CanEverRender())
	{
		return;
	}
	URaceResultsWidget* ResultsWidget = CreateWidget<URaceResultsWidget>(this, URaceResultsWidget::StaticClass());
	ResultsWidget->SetResults(Results);
	ScreenWidget = ResultsWidget;
	ScreenWidget->ShowScreen();
}

void ARacePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!bRaceScreensSetUp)
	{
		bRaceScreensSetUp = true;
		SetUpRaceScreens();
	}

	ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetPawn());
	if (!Vehicle || !DrivingInput)
	{
		return;
	}

	FVehicleInputState Input;
	Input.Throttle = FMath::Clamp(GetAxisValue(DrivingInput->Throttle), 0.f, 1.f);
	Input.Brake = FMath::Clamp(GetAxisValue(DrivingInput->Brake), 0.f, 1.f);
	Input.Steer = FMath::Clamp(GetAxisValue(DrivingInput->Steer), -1.f, 1.f);
	Input.bDriftLeft = GetButtonValue(DrivingInput->DriftLeft);
	Input.bDriftRight = GetButtonValue(DrivingInput->DriftRight);
	Input.bBoost = GetButtonValue(DrivingInput->Boost);
	Input.bUseAbility = GetButtonValue(DrivingInput->Ability);
	Input.bUseItem = GetButtonValue(DrivingInput->UseItem);

	Vehicle->SetInput(Input);

	// Camera controls are not vehicle input: they go straight to this player's camera.
	if (URaceCameraComponent* Camera = Vehicle->GetRaceCamera())
	{
		Camera->SetLookBack(GetButtonValue(DrivingInput->LookBack));
	}
}

float ARacePlayerController::GetAxisValue(const UInputAction* Action) const
{
	const UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	return (Action && EnhancedInput) ? EnhancedInput->GetBoundActionValue(Action).Get<float>() : 0.f;
}

bool ARacePlayerController::GetButtonValue(const UInputAction* Action) const
{
	const UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	return (Action && EnhancedInput) ? EnhancedInput->GetBoundActionValue(Action).Get<bool>() : false;
}
