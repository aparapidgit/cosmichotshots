#include "Adventure/HubCharacter.h"

#include "CosmicHotshotsGame.h"
#include "Adventure/AdventureDefinitions.h"
#include "Adventure/AdventureHubActors.h"
#include "Adventure/AdventureHubTuningData.h"
#include "Adventure/AdventureSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Character/CharacterDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/OnFootInputConfig.h"
#include "Input/SystemInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Race/RaceDebug.h"
#include "UI/HubScreens.h"

#define LOCTEXT_NAMESPACE "AdventureHubPlayer"

namespace
{
	/** The placeholder cylinder is 100 cm across and high: scale = wanted size in cm times this. */
	constexpr float HubBodyUnitScale = 0.01f;

	/** The hub HUD is drawn under every menu screen. */
	constexpr int32 HubHudZOrder = -1;
}

// --- AHubCharacter

AHubCharacter::AHubCharacter()
{
	// The character turns to where it walks; the camera turns with the player's look input.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(RootComponent);
	CameraArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

void AHubCharacter::BeginPlay()
{
	Super::BeginPlay();

	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	GetCharacterMovement()->MaxWalkSpeed = Tuning.WalkSpeed;
	GetCharacterMovement()->RotationRate = FRotator(0.f, Tuning.TurnRate, 0.f);
	CameraArm->TargetArmLength = Tuning.CameraDistance;
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, Tuning.CameraPivotHeight));

	// Placeholder body: a cylinder the size of the capsule, in the colour of the campaign's player character.
	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	Body->SetStaticMesh(Tuning.CylinderMesh.LoadSynchronous());
	Body->SetRelativeScale3D(FVector(Radius * 2.f, Radius * 2.f, HalfHeight * 2.f) * HubBodyUnitScale);

	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	if (UMaterialInterface* Material = Tuning.PlaceholderMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Body);
		Dynamic->SetVectorParameterValue(Tuning.ColourParameter, Campaign && Campaign->PlayerCharacter ? Campaign->PlayerCharacter->Colour : FLinearColor::White);
		Body->SetMaterial(0, Dynamic);
	}
}

void AHubCharacter::SetRunning(bool bRunning)
{
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	GetCharacterMovement()->MaxWalkSpeed = bRunning ? Tuning.RunSpeed : Tuning.WalkSpeed;
}

// --- AHubPlayerController

void AHubPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	OnFootInput = Settings->OnFootInput.LoadSynchronous();
	SystemInput = Settings->SystemInput.LoadSynchronous();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}
	if (!OnFootInput)
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("HUB: no on-foot input config. Set Project Settings > Game > Cosmic Hotshots > On Foot Input (Scripts/create_phase2b_assets.py creates the assets)."));
	}
	else
	{
		// Axes and the run button are polled every frame in PlayerTick; Interact is a press.
		for (const UInputAction* Action : { OnFootInput->Move.Get(), OnFootInput->Look.Get(), OnFootInput->LookMouse.Get(), OnFootInput->Run.Get() })
		{
			if (Action)
			{
				EnhancedInput->BindActionValue(Action);
			}
		}
		if (OnFootInput->Interact)
		{
			EnhancedInput->BindAction(OnFootInput->Interact, ETriggerEvent::Started, this, &AHubPlayerController::HandleInteractPressed);
		}
	}
	if (SystemInput && SystemInput->Pause)
	{
		EnhancedInput->BindAction(SystemInput->Pause, ETriggerEvent::Started, this, &AHubPlayerController::HandlePausePressed);
	}
}

void AHubPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	// The camera limits and where it looks at the start.
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	FRotator StartRotation = GetControlRotation();
	StartRotation.Pitch = Tuning.CameraStartPitch;
	SetControlRotation(StartRotation);

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputSubsystem)
	{
		return;
	}
	if (OnFootInput && OnFootInput->MappingContext)
	{
		InputSubsystem->AddMappingContext(OnFootInput->MappingContext, OnFootInput->MappingPriority);
	}
	if (SystemInput && SystemInput->MappingContext)
	{
		InputSubsystem->AddMappingContext(SystemInput->MappingContext, SystemInput->MappingPriority);
	}
}

FVector2D AHubPlayerController::GetAxis2D(const UInputAction* Action) const
{
	const UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	return (Action && EnhancedInput) ? EnhancedInput->GetBoundActionValue(Action).Get<FVector2D>() : FVector2D::ZeroVector;
}

bool AHubPlayerController::GetButton(const UInputAction* Action) const
{
	const UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	return (Action && EnhancedInput) ? EnhancedInput->GetBoundActionValue(Action).Get<bool>() : false;
}

void AHubPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!bHubSetUp)
	{
		bHubSetUp = true;
		SetUpHub();
	}

	ApplyMovementInput(DeltaTime);
	UpdateNearby();

	if (MessageTimeRemaining > 0.f)
	{
		MessageTimeRemaining -= DeltaTime;
		if (MessageTimeRemaining <= 0.f && HudWidget)
		{
			HudWidget->SetMessage(FText::GetEmpty());
		}
	}
}

void AHubPlayerController::SetUpHub()
{
	if (!IsLocalController())
	{
		return;
	}
	UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;

	if (FApp::CanEverRender())
	{
		HudWidget = CreateWidget<UHubHudWidget>(this, UHubHudWidget::StaticClass());
		HudWidget->AddToPlayerScreen(HubHudZOrder);
	}

	// What the last race changed: said once, when the player is back in the hub.
	if (Adventure && Campaign)
	{
		const FAdventureRaceOutcome Outcome = Adventure->ConsumeLastOutcome();
		const UAdventureLevelDefinition* Unlocked = Campaign->GetLevel(Outcome.UnlockedLevelIndex);
		if (Campaign->Sections.IsValidIndex(Outcome.OpenedSectionIndex))
		{
			ShowMessage(FText::Format(LOCTEXT("GateOpened", "The gate to {0} is open!"), Campaign->Sections[Outcome.OpenedSectionIndex].DisplayName));
		}
		else if (Unlocked)
		{
			ShowMessage(FText::Format(LOCTEXT("LevelUnlocked", "{0} is unlocked!"), Unlocked->DisplayName));
		}
		else if (Outcome.bWon && Outcome.LevelIndex == Campaign->GetNumLevels() - 1)
		{
			ShowMessage(LOCTEXT("CampaignDone", "Every level is won!"));
		}
		UE_LOG(LogCosmicRace, Log, TEXT("HUB entered: %d/%d levels won; last race level=%d won=%d unlocked=%d openedSection=%d"),
			Adventure->GetNumLevelsWon(), Campaign->GetNumLevels(), Outcome.LevelIndex, Outcome.bWon ? 1 : 0, Outcome.UnlockedLevelIndex, Outcome.OpenedSectionIndex);
	}

#if !UE_BUILD_SHIPPING
	// Test helper (ch.Test.HubEnterLevel): walk into this level's portal once, the way the Interact button does.
	const int32 TestLevel = CosmicHotshots::RaceCVars::HubEnterLevel.GetValueOnGameThread();
	if (TestLevel >= 0 && Adventure && !Adventure->HasTestEnteredPortal())
	{
		Adventure->SetTestEnteredPortal();
		for (TActorIterator<AAdventurePortal> It(GetWorld()); It; ++It)
		{
			if (It->GetLevelIndex() == TestLevel)
			{
				if (APawn* OwnPawn = GetPawn())
				{
					OwnPawn->SetActorLocation(It->GetActorLocation() + FVector(0.f, 0.f, OwnPawn->GetSimpleCollisionHalfHeight()));
				}
				UE_LOG(LogCosmicRace, Log, TEXT("HUB test: entering the portal of level %d"), TestLevel);
				TryEnterPortal(*It);
				break;
			}
		}
	}
#endif
}

void AHubPlayerController::ApplyMovementInput(float DeltaTime)
{
	AHubCharacter* HubCharacter = Cast<AHubCharacter>(GetPawn());
	if (!HubCharacter || !OnFootInput)
	{
		return;
	}
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();

	// Camera: the stick is a rate, the mouse a movement. Up on either looks up.
	const FVector2D StickLook = GetAxis2D(OnFootInput->Look) * Tuning.StickLookRate * DeltaTime;
	const FVector2D MouseLook = GetAxis2D(OnFootInput->LookMouse) * Tuning.MouseLookScale;
	FRotator Look = GetControlRotation();
	Look.Yaw += StickLook.X + MouseLook.X;
	Look.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch) + StickLook.Y + MouseLook.Y, Tuning.CameraMinPitch, Tuning.CameraMaxPitch);
	Look.Roll = 0.f;
	SetControlRotation(Look);

	// Walking is relative to where the camera looks, on the ground plane.
	const FVector2D Move = GetAxis2D(OnFootInput->Move);
	const FRotator YawOnly(0.f, Look.Yaw, 0.f);
	HubCharacter->AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Move.Y);
	HubCharacter->AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Move.X);
	HubCharacter->SetRunning(GetButton(OnFootInput->Run));
}

void AHubPlayerController::UpdateNearby()
{
	NearbyPortal = nullptr;
	const APawn* OwnPawn = GetPawn();
	if (!OwnPawn)
	{
		return;
	}
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;

	// The nearest portal within reach (distances on the ground plane).
	float BestDistance = Tuning.InteractRadius;
	for (TActorIterator<AAdventurePortal> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), OwnPawn->GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			NearbyPortal = *It;
		}
	}

	FText Prompt;
	if (NearbyPortal && Campaign)
	{
		const UAdventureLevelDefinition* Level = Campaign->GetLevel(NearbyPortal->GetLevelIndex());
		const UAdventureLevelDefinition* Before = Campaign->GetLevel(NearbyPortal->GetLevelIndex() - 1);
		const FText LevelName = Level ? Level->DisplayName : FText::GetEmpty();
		Prompt = NearbyPortal->GetState() != EAdventurePortalState::Locked
			? FText::Format(LOCTEXT("PromptEnter", "{0}:  press A / E to race"), LevelName)
			: FText::Format(LOCTEXT("PromptLocked", "{0} is locked:  win {1} first"), LevelName, Before ? Before->DisplayName : LOCTEXT("PromptPrevious", "the level before it"));
	}
	else if (Campaign)
	{
		// A closed gate nearby explains itself.
		for (TActorIterator<AAdventureGate> It(GetWorld()); It; ++It)
		{
			if (!It->IsOpen() && FVector::Dist2D(It->GetActorLocation(), OwnPawn->GetActorLocation()) <= Tuning.GateHintRadius
				&& Campaign->Sections.IsValidIndex(It->GetSectionIndex() - 1))
			{
				Prompt = FText::Format(LOCTEXT("PromptGate", "Gate closed:  win every level of {0}"), Campaign->Sections[It->GetSectionIndex() - 1].DisplayName);
				break;
			}
		}
	}

	if (HudWidget)
	{
		HudWidget->SetPrompt(Prompt);
		if (Adventure && Campaign)
		{
			HudWidget->SetProgress(FText::Format(LOCTEXT("Progress", "Adventure:  {0} / {1} levels won"),
				FText::AsNumber(Adventure->GetNumLevelsWon()), FText::AsNumber(Campaign->GetNumLevels())));
		}
	}
}

void AHubPlayerController::HandleInteractPressed()
{
	if (NearbyPortal && !IsPaused())
	{
		TryEnterPortal(NearbyPortal);
	}
}

void AHubPlayerController::TryEnterPortal(const AAdventurePortal* Portal)
{
	UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	if (!Portal || !Adventure)
	{
		return;
	}
	// A locked portal says so (the prompt already does) and nothing starts.
	if (!Adventure->StartLevel(this, Portal->GetLevelIndex()))
	{
		ShowMessage(LOCTEXT("PortalLockedMessage", "This portal is locked."));
	}
}

void AHubPlayerController::HandlePausePressed()
{
	if (!FApp::CanEverRender() || IsPaused() || !SetPause(true))
	{
		return;
	}
	URaceScreenWidget* PauseMenu = CreateWidget<UHubPauseMenuWidget>(this, UHubPauseMenuWidget::StaticClass());
	PauseMenu->ShowScreen();
}

void AHubPlayerController::ShowMessage(const FText& Message)
{
	MessageTimeRemaining = UAdventureHubTuningData::Get()->MessageDuration;
	if (HudWidget)
	{
		HudWidget->SetMessage(Message);
	}
}

#undef LOCTEXT_NAMESPACE
