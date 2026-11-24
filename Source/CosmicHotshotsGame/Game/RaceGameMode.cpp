#include "Game/RaceGameMode.h"

#include "CosmicHotshotsGame.h"
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
#include "Vehicle/HoverMovementComponent.h"
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

		// Test helper (ch.Test.PlayerCharacter): here, before the first physics step, when the console
		// variable was set at startup (-DPCVars). Set later, the vehicle's own test helper applies it.
		const int32 PlayerCharacterIndex = CosmicHotshots::VehicleCVars::PlayerCharacter.GetValueOnGameThread();
		const APlayerController* FirstPlayer = GetWorld()->GetFirstPlayerController();
		if (ARaceVehicle* PlayerVehicle = FirstPlayer ? Cast<ARaceVehicle>(FirstPlayer->GetPawn()) : nullptr; PlayerVehicle && PlayerCharacterIndex >= 0)
		{
			PlayerVehicle->SetCharacter(GetDefault<UCosmicHotshotsSettings>()->FindCharacter(FString::FromInt(PlayerCharacterIndex)));
		}

		// Test helper (ch.Test.PlayerGridSlot): the console variable is not loaded yet when the player's
		// car is spawned, so move the car to its slot now, still before the first physics step.
		const TArray<APlayerStart*> Starts = GetSortedPlayerStarts();
		APawn* PlayerPawn = FirstPlayer ? FirstPlayer->GetPawn() : nullptr;
		if (PlayerPawn && !Starts.IsEmpty() && CosmicHotshots::VehicleCVars::PlayerGridSlot.GetValueOnGameThread() >= 0)
		{
			const APlayerStart* Slot = Starts[GetPlayerGridSlot(0, Starts.Num())];
			PlayerPawn->SetActorLocationAndRotation(Slot->GetActorLocation(), Slot->GetActorRotation(), false, nullptr, ETeleportType::ResetPhysics);
		}

		SpawnTestBots();
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
	ARaceVehicle* Dummy = GetWorld()->SpawnActor<ARaceVehicle>(PlayerPawn->GetClass(), Location,
		PlayerPawn->GetActorRotation() + FRotator(0.0, VehicleCVars::DummyYawOffset.GetValueOnGameThread(), 0.0), SpawnParameters);

	const int32 DummyCharacterIndex = VehicleCVars::DummyCharacter.GetValueOnGameThread();
	if (Dummy && DummyCharacterIndex >= 0)
	{
		Dummy->SetCharacter(GetDefault<UCosmicHotshotsSettings>()->FindCharacter(FString::FromInt(DummyCharacterIndex)));
	}
#endif
}

void ARaceGameMode::SpawnTestBots()
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	// Each bot drives its own line beside the centre and a slightly different throttle, so the
	// field spreads out and the faster ones catch and overtake the slower ones.
	static constexpr float BotLateralOffsets[] = { 400.f, -400.f, 150.f, -150.f, 650.f, -650.f, 0.f };
	static constexpr float BotThrottleScales[] = { 1.f, 0.97f, 0.94f, 0.99f, 0.96f, 0.93f, 0.98f };
	static_assert(UE_ARRAY_COUNT(BotLateralOffsets) == UE_ARRAY_COUNT(BotThrottleScales));

	const int32 RequestedBots = FMath::Clamp(VehicleCVars::TestBots.GetValueOnGameThread(), 0, static_cast<int32>(UE_ARRAY_COUNT(BotLateralOffsets)));
	if (RequestedBots == 0)
	{
		return;
	}

	const TArray<APlayerStart*> Starts = GetSortedPlayerStarts();
	const UClass* VehicleClass = DefaultPawnClass ? DefaultPawnClass.Get() : ARaceVehicle::StaticClass();
	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	const int32 NumCharacters = Settings->Characters.Num();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Slots the human players did not take (ChoosePlayerStart), front to back.
	TArray<int32> FreeSlots;
	for (int32 Slot = 0; Slot < Starts.Num(); ++Slot)
	{
		bool bTaken = false;
		for (int32 PlayerIndex = 0; PlayerIndex < GetNumPlayers(); ++PlayerIndex)
		{
			bTaken |= GetPlayerGridSlot(PlayerIndex, Starts.Num()) == Slot;
		}
		if (!bTaken)
		{
			FreeSlots.Add(Slot);
		}
	}

	int32 Spawned = 0;
	for (int32 Bot = 0; Bot < RequestedBots; ++Bot)
	{
		if (!FreeSlots.IsValidIndex(Bot))
		{
			UE_LOG(LogCosmicVehicle, Warning, TEXT("ch.Test.Bots: only %d free grid slots, %d bots requested."), Spawned, RequestedBots);
			break;
		}
		const int32 Slot = FreeSlots[Bot];

		ARaceVehicle* BotVehicle = GetWorld()->SpawnActor<ARaceVehicle>(const_cast<UClass*>(VehicleClass),
			Starts[Slot]->GetActorLocation(), Starts[Slot]->GetActorRotation(), SpawnParameters);
		if (!BotVehicle)
		{
			continue;
		}
		if (NumCharacters > 0)
		{
			// ch.Test.BotCharacter: every bot the same roster character; negative = one after the other through the roster.
			const int32 BotCharacter = VehicleCVars::BotCharacter.GetValueOnGameThread();
			BotVehicle->SetCharacter(Settings->FindCharacter(FString::FromInt(BotCharacter >= 0 ? BotCharacter : (Bot + 1) % NumCharacters)));
		}
		BotVehicle->GetHoverMovement()->SetTestBot(BotLateralOffsets[Bot], BotThrottleScales[Bot]);
		++Spawned;
	}
#endif
}

TArray<APlayerStart*> ARaceGameMode::GetSortedPlayerStarts() const
{
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}
	Starts.Sort([](const APlayerStart& A, const APlayerStart& B)
	{
		return A.GetName() < B.GetName();
	});
	return Starts;
}

AActor* ARaceGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const TArray<APlayerStart*> Starts = GetSortedPlayerStarts();
	if (Starts.IsEmpty())
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// One slot per player, in join order.
	return Starts[GetPlayerGridSlot(FMath::Max(GetNumPlayers() - 1, 0), Starts.Num())];
}

int32 ARaceGameMode::GetPlayerGridSlot(int32 PlayerIndex, int32 NumSlots) const
{
#if !UE_BUILD_SHIPPING
	// Test helper: the first player can start from any slot (e.g. the last one, to test comeback passives).
	const int32 TestSlot = CosmicHotshots::VehicleCVars::PlayerGridSlot.GetValueOnGameThread();
	if (PlayerIndex == 0 && TestSlot >= 0)
	{
		return TestSlot % NumSlots;
	}
#endif
	return PlayerIndex % NumSlots;
}
