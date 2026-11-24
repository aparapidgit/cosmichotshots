#include "Game/RaceGameMode.h"

#include "Debug/RaceDebugHUD.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/PlayerController.h"
#include "Vehicle/VehicleDebug.h"
#include "GameFramework/PlayerStart.h"
#include "Player/RacePlayerController.h"
#include "Race/RaceGameState.h"
#include "Race/RacePlayerState.h"
#include "Vehicle/RaceVehicle.h"

ARaceGameMode::ARaceGameMode()
{
	DefaultPawnClass = ARaceVehicle::StaticClass();
	PlayerControllerClass = ARacePlayerController::StaticClass();
	GameStateClass = ARaceGameState::StaticClass();
	PlayerStateClass = ARacePlayerState::StaticClass();

	// Dev-only boost meter until the real UMG HUD exists (Phase 2).
	HUDClass = ARaceDebugHUD::StaticClass();

	// Test helpers only; the tick does nothing else.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void ARaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (!bTestDummyChecked)
	{
		bTestDummyChecked = true;
		SpawnTestDummy();
	}
#endif
}

void ARaceGameMode::SpawnTestDummy()
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	const float DummyDistance = VehicleCVars::SpawnDummyAhead.GetValueOnGameThread();
	const APlayerController* FirstPlayer = GetWorld()->GetFirstPlayerController();
	const APawn* PlayerPawn = FirstPlayer ? FirstPlayer->GetPawn() : nullptr;
	if (DummyDistance <= 0.f || !PlayerPawn)
	{
		return;
	}

	const FVector Location = PlayerPawn->GetActorLocation()
		+ PlayerPawn->GetActorForwardVector() * DummyDistance
		+ PlayerPawn->GetActorRightVector() * VehicleCVars::DummyLateralOffset.GetValueOnGameThread();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARaceVehicle* Dummy = GetWorld()->SpawnActor<ARaceVehicle>(PlayerPawn->GetClass(), Location, PlayerPawn->GetActorRotation(), SpawnParameters);

	const int32 DummyCharacterIndex = VehicleCVars::DummyCharacter.GetValueOnGameThread();
	if (Dummy && DummyCharacterIndex >= 0)
	{
		Dummy->SetCharacter(GetDefault<UCosmicHotshotsSettings>()->FindCharacter(FString::FromInt(DummyCharacterIndex)));
	}
#endif
}

AActor* ARaceGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}

	if (Starts.IsEmpty())
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	Starts.Sort([](const APlayerStart& A, const APlayerStart& B)
	{
		return A.GetName() < B.GetName();
	});

	// One slot per player, in join order.
	const int32 SlotIndex = FMath::Max(GetNumPlayers() - 1, 0) % Starts.Num();
	return Starts[SlotIndex];
}
