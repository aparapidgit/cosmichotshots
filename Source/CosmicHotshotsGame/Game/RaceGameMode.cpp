#include "Game/RaceGameMode.h"

#include "CosmicHotshotsGame.h"
#include "AI/AIDifficultyData.h"
#include "AI/RaceAIController.h"
#include "Character/CharacterDefinition.h"
#include "Race/RaceDebug.h"
#include "Debug/RaceDebugHUD.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Game/GameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Item/ItemBox.h"
#include "GameFramework/PlayerController.h"
#include "Vehicle/VehicleDebug.h"
#include "GameFramework/PlayerStart.h"
#include "Player/RacePlayerController.h"
#include "Race/RaceGameState.h"
#include "Race/RacePlayerState.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"

namespace
{
	const FName ItemBoxPlaceholderTag(TEXT("ItemBox"));

	/** Seeds of the drivers that take over human cars after the finish (grid slots use 0..7). */
	constexpr int32 FinishDriverSeedBase = 100;
}

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

	if (bGridFilled)
	{
		return;
	}
	bGridFilled = true;

	// The race setup: from the menus (UGameFlowSubsystem) when there is one, else from the dev console variables.
	const UGameFlowSubsystem* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr;
	const bool bHasConfig = Flow && Flow->HasRaceConfig();
	const bool bWaitingForSetup = Flow && Flow->IsWaitingForRaceSetup();

	const bool bItemBoxes = !bWaitingForSetup
		&& (bHasConfig ? Flow->GetRaceConfig().bItemBoxes : CosmicHotshots::RaceCVars::ItemBoxes.GetValueOnGameThread() != 0);
	ReplaceItemBoxPlaceholders(bItemBoxes);
	if (bWaitingForSetup)
	{
		// The player is still choosing (character select): the grid stays empty and nothing starts.
		return;
	}

	// Characters the humans picked.
	if (bHasConfig)
	{
		int32 PlayerIndex = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++PlayerIndex)
		{
			ARaceVehicle* HumanVehicle = It->IsValid() ? Cast<ARaceVehicle>((*It)->GetPawn()) : nullptr;
			const TArray<TObjectPtr<const UCharacterDefinition>>& Picked = Flow->GetRaceConfig().HumanCharacters;
			if (HumanVehicle && Picked.IsValidIndex(PlayerIndex) && Picked[PlayerIndex])
			{
				HumanVehicle->SetCharacter(Picked[PlayerIndex]);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	{

		// Test helper (ch.Test.PlayerCharacter): here, before the first physics step, when the console
		// variable was set before the race started (-ExecCmds). Set later, the vehicle's own test helper applies it.
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

	}
#endif

	// The rest of the grid, in the first pre-physics tick: every car exists before the first physics step.
	FreeGridSlots = FindFreeGridSlots();
#if !UE_BUILD_SHIPPING
	SpawnTestBots();
#endif
	SpawnAIOpponents();
	SpawnFinishDrivers();
#if !UE_BUILD_SHIPPING
	SpawnTestDummy();
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

	int32 Spawned = 0;
	for (int32 Bot = 0; Bot < RequestedBots; ++Bot)
	{
		if (FreeGridSlots.IsEmpty())
		{
			UE_LOG(LogCosmicVehicle, Warning, TEXT("ch.Test.Bots: only %d free grid slots, %d bots requested."), Spawned, RequestedBots);
			break;
		}
		const int32 Slot = FreeGridSlots[0];
		FreeGridSlots.RemoveAt(0);

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

void ARaceGameMode::ReplaceItemBoxPlaceholders(bool bItemBoxes)
{
	// The test track marks its item box spots with plain cubes tagged "ItemBox" (and the map has them with
	// collision on). They never stay in a race: each one is hidden and, when item boxes are on, a real
	// AItemBox takes its place. Levels built with AItemBox / AItemBoxCluster actors need none of this.
	TArray<AActor*> Placeholders;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(ItemBoxPlaceholderTag) && !It->IsA<AItemBox>())
		{
			Placeholders.Add(*It);
		}
	}
	// Fixed order, so box indices are the same in every run.
	Placeholders.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });

	for (AActor* Placeholder : Placeholders)
	{
		Placeholder->SetActorEnableCollision(false);
		Placeholder->SetActorHiddenInGame(true);
		if (bItemBoxes)
		{
			GetWorld()->SpawnActor<AItemBox>(AItemBox::StaticClass(), Placeholder->GetActorLocation(), FRotator::ZeroRotator);
		}
	}

	if (!bItemBoxes)
	{
		// "Item boxes off": boxes placed in the level directly go too.
		for (TActorIterator<AItemBox> It(GetWorld()); It; ++It)
		{
			It->Destroy();
		}
	}
}

TArray<int32> ARaceGameMode::FindFreeGridSlots()
{
	// Slots the human players did not take (ChoosePlayerStart), front to back, at most MaxGridSize slots in all.
	const int32 NumSlots = FMath::Min(GetSortedPlayerStarts().Num(), UCosmicHotshotsSettings::MaxGridSize);
	TArray<int32> FreeSlots;
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		bool bTaken = false;
		for (int32 PlayerIndex = 0; PlayerIndex < GetNumPlayers(); ++PlayerIndex)
		{
			bTaken |= GetPlayerGridSlot(PlayerIndex, NumSlots) == Slot;
		}
		if (!bTaken)
		{
			FreeSlots.Add(Slot);
		}
	}
	return FreeSlots;
}

void ARaceGameMode::SpawnAIOpponents()
{
	using namespace CosmicHotshots;

	const UGameFlowSubsystem* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr;
	const bool bHasConfig = Flow && Flow->HasRaceConfig();
	const int32 Requested = FMath::Clamp(bHasConfig ? Flow->GetRaceConfig().NumAIOpponents : RaceCVars::AIOpponents.GetValueOnGameThread(),
		0, UCosmicHotshotsSettings::MaxGridSize - 1);
	if (Requested == 0)
	{
		return;
	}

	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	const int32 DifficultyIndex = bHasConfig ? Flow->GetRaceConfig().AIDifficulty : RaceCVars::AIDifficulty.GetValueOnGameThread();
	const UAIDifficultyData* Difficulty = Settings->GetAIDifficulty(DifficultyIndex >= 0 ? DifficultyIndex : Settings->DefaultAIDifficulty);

	// Characters are unique per race: skip the ones the humans drive. Roster entries without a passive are
	// the dev test characters, not racers.
	TArray<const UCharacterDefinition*> Available;
	for (const TSoftObjectPtr<UCharacterDefinition>& Entry : Settings->Characters)
	{
		const UCharacterDefinition* Candidate = Entry.LoadSynchronous();
		if (!Candidate || !Candidate->PassiveDefinition)
		{
			continue;
		}
		bool bTaken = false;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const ARaceVehicle* HumanVehicle = It->IsValid() ? Cast<ARaceVehicle>((*It)->GetPawn()) : nullptr;
			bTaken |= HumanVehicle && HumanVehicle->GetCharacter() == Candidate;
		}
		if (!bTaken)
		{
			Available.Add(Candidate);
		}
	}

	const TArray<APlayerStart*> Starts = GetSortedPlayerStarts();
	UClass* VehicleClass = DefaultPawnClass ? DefaultPawnClass.Get() : ARaceVehicle::StaticClass();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < Requested; ++Index)
	{
		if (FreeGridSlots.IsEmpty())
		{
			UE_LOG(LogCosmicRace, Warning, TEXT("ch.Race.AIOpponents: no free grid slot for computer driver %d of %d."), Index + 1, Requested);
			break;
		}
		const int32 Slot = FreeGridSlots[0];
		FreeGridSlots.RemoveAt(0);

		ARaceVehicle* Vehicle = GetWorld()->SpawnActor<ARaceVehicle>(VehicleClass,
			Starts[Slot]->GetActorLocation(), Starts[Slot]->GetActorRotation(), SpawnParameters);
		ARaceAIController* Driver = Vehicle ? GetWorld()->SpawnActor<ARaceAIController>(SpawnParameters) : nullptr;
		if (!Driver)
		{
			continue;
		}
		// The setup names the rivals' characters (menus draw them); without that, the free roster characters in order.
		const UCharacterDefinition* SetupCharacter = bHasConfig && Flow->GetRaceConfig().AICharacters.IsValidIndex(Index)
			? Flow->GetRaceConfig().AICharacters[Index].Get() : nullptr;
		if (SetupCharacter)
		{
			Vehicle->SetCharacter(SetupCharacter);
		}
		else if (!Available.IsEmpty())
		{
			Vehicle->SetCharacter(Available[Index % Available.Num()]);
		}
		// The grid slot is the seed: the same slot always gets the same driver.
		Driver->Configure(Difficulty, Slot);
		Driver->Possess(Vehicle);
	}
}

void ARaceGameMode::SpawnFinishDrivers()
{
	// One computer driver stands by on every human's car and takes it over when that car has finished,
	// so it keeps driving instead of sitting on the track (developer decision).
	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	const UAIDifficultyData* Difficulty = Settings->GetAIDifficulty(Settings->DefaultAIDifficulty);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 PlayerIndex = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++PlayerIndex)
	{
		ARaceVehicle* HumanVehicle = It->IsValid() ? Cast<ARaceVehicle>((*It)->GetPawn()) : nullptr;
		ARaceAIController* Driver = HumanVehicle ? GetWorld()->SpawnActor<ARaceAIController>(SpawnParameters) : nullptr;
		if (Driver)
		{
			Driver->Configure(Difficulty, FinishDriverSeedBase + PlayerIndex);
			Driver->DriveAfterFinish(HumanVehicle);
		}
	}
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
