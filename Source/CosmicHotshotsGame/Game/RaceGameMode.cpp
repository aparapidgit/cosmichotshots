#include "Game/RaceGameMode.h"

#include "Debug/RaceDebugHUD.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Player/RacePlayerController.h"
#include "Vehicle/RaceVehicle.h"

ARaceGameMode::ARaceGameMode()
{
	DefaultPawnClass = ARaceVehicle::StaticClass();
	PlayerControllerClass = ARacePlayerController::StaticClass();

	// Dev-only boost meter until the real UMG HUD exists (Phase 2).
	HUDClass = ARaceDebugHUD::StaticClass();
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
