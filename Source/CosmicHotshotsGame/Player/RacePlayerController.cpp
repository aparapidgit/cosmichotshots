#include "Player/RacePlayerController.h"

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

	if (!DrivingInput || !DrivingInput->MappingContext)
	{
		return;
	}

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(DrivingInput->MappingContext, DrivingInput->MappingPriority);
		}
	}
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

void ARacePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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
