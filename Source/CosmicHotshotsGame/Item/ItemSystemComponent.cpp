#include "Item/ItemSystemComponent.h"

#include "CosmicHotshotsGame.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "Item/ItemBox.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemLootTable.h"
#include "Race/RaceGameState.h"
#include "Race/RaceTrackerComponent.h"
#include "Race/RaceTuningData.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"

namespace
{
	TAutoConsoleVariable<FString> CVarForceItem(
		TEXT("ch.Item.Force"), TEXT(""),
		TEXT("Dev: every box gives the item with this id (e.g. Empty). Empty text = normal loot table."), ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarNoPositionBias(
		TEXT("ch.Item.NoPositionBias"), 0,
		TEXT("Dev: 1 = the race place does not influence which item a box gives."), ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarItemSeed(
		TEXT("ch.Item.Seed"), 1234,
		TEXT("Seed of the item roulette. The same seed and the same race give the same items."), ECVF_Cheat);

	/** Dev: rolls the loot table many times for one race place and prints what came out. */
	FAutoConsoleCommandWithWorldAndArgs PrintLootCommand(
		TEXT("ch.Item.PrintLoot"),
		TEXT("Dev: ch.Item.PrintLoot <place> rolls the loot table 1000 times for that race place and prints the result."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			constexpr int32 NumRolls = 1000;
			const ARaceGameState* RaceState = World ? World->GetGameState<ARaceGameState>() : nullptr;
			const UItemLootTable* Table = RaceState ? RaceState->GetItemSystem()->GetLootTable() : nullptr;
			if (!Table)
			{
				UE_LOG(LogCosmicRace, Warning, TEXT("ch.Item.PrintLoot: no loot table (Project Settings > Cosmic Hotshots > Items)."));
				return;
			}

			const int32 Rank = Args.Num() > 0 ? FMath::Max(FCString::Atoi(*Args[0]), 1) : 1;
			FRandomStream Stream(CVarItemSeed.GetValueOnGameThread());
			TMap<FName, int32> Counts;
			for (int32 Roll = 0; Roll < NumRolls; ++Roll)
			{
				const UItemDefinition* Item = Table->Pick(Rank, Stream, CVarNoPositionBias.GetValueOnGameThread() == 0);
				Counts.FindOrAdd(Item ? Item->ItemId : FName(TEXT("(nothing)")))++;
			}
			for (const TPair<FName, int32>& Count : Counts)
			{
				UE_LOG(LogCosmicRace, Log, TEXT("LOOT place=%d item=%s %d of %d"), Rank, *Count.Key.ToString(), Count.Value, NumRolls);
			}
		}));
}

UItemSystemComponent::UItemSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Before physics: boxes placed this frame are in play from the frame's first physics step.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// The engine asserts that every component registered for the async physics tick is active.
	bAutoActivate = true;
}

void UItemSystemComponent::SetTracker(URaceTrackerComponent* InTracker)
{
	Tracker = InTracker;
}

void UItemSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	LootTable = Settings->ItemLootTable.LoadSynchronous();

	const URaceTuningData* Tuning = Settings->RaceTuning.LoadSynchronous();
	if (!Tuning)
	{
		Tuning = GetDefault<URaceTuningData>();
	}
	RespawnTime = Tuning->ItemBoxRespawnTime;
	RollDuration = Tuning->ItemRollDuration;
	PickupRadius = Tuning->ItemPickupRadius;
	AIUseDelay = Tuning->AIItemUseDelay;

	Random.Initialize(CVarItemSeed.GetValueOnGameThread());

	// The game mode may place boxes in its pre-physics tick; gather them in the same frame.
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		PrimaryComponentTick.AddPrerequisite(GameMode, GameMode->PrimaryActorTick);
	}

	// Enabled here, not in the constructor, so the physics step never sees a component that is not active yet.
	SetAsyncPhysicsTickEnabled(true);
}

void UItemSystemComponent::GatherBoxes()
{
	// Every box in the level, in a fixed order (by name), so box indices and the roulette are repeatable.
	BoxActors.Reset();
	for (TActorIterator<AItemBox> It(GetWorld()); It; ++It)
	{
		BoxActors.Add(*It);
	}
	BoxActors.Sort([](const AItemBox& A, const AItemBox& B) { return A.GetName() < B.GetName(); });

	FScopeLock ScopeLock(&Lock);
	NewBoxes.Reset(BoxActors.Num());
	for (const AItemBox* Box : BoxActors)
	{
		NewBoxes.AddDefaulted_GetRef().Location = Box->GetActorLocation();
	}
	bNewBoxes = true;
}

void UItemSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bBoxesGathered)
	{
		bBoxesGathered = true;
		GatherBoxes();
	}

	TArray<FItemEvent> Events;
	{
		FScopeLock ScopeLock(&Lock);
		Events = MoveTemp(PublishedEvents);
		PublishedEvents.Reset();
	}

	for (const FItemEvent& Event : Events)
	{
		if (BoxActors.IsValidIndex(Event.BoxIndex) && BoxActors[Event.BoxIndex])
		{
			BoxActors[Event.BoxIndex]->SetBoxActive(Event.Type == EItemEventType::BoxRespawn);
		}
		OnItemEvent.Broadcast(Event);
	}
}

const TArray<bool>& UItemSystemComponent::GetBoxesActiveForStep(float SimTime) const
{
	return (bHasBoxView && NewestBoxesSimTime == SimTime) ? OlderBoxesActive : NewestBoxesActive;
}

FItemSlot UItemSystemComponent::GetSlot(const ARaceVehicle* Vehicle) const
{
	const int32 RacerId = Tracker ? Tracker->FindRacerId(Vehicle) : INDEX_NONE;
	FScopeLock ScopeLock(&Lock);
	return PublishedSlots.IsValidIndex(RacerId) ? PublishedSlots[RacerId] : FItemSlot();
}

void UItemSystemComponent::GiveItem(ARaceVehicle* Vehicle, const UItemDefinition* Item)
{
	FScopeLock ScopeLock(&Lock);
	FPendingGive& Give = PendingGives.AddDefaulted_GetRef();
	Give.Vehicle = Vehicle;
	Give.Item = Item;
}

FString UItemSystemComponent::GetItemName(const UItemDefinition* Item) const
{
	return Item ? Item->ItemId.ToString() : FString(TEXT("None"));
}

const UItemDefinition* UItemSystemComponent::RollItem(int32 Rank)
{
	if (!LootTable)
	{
		return nullptr;
	}
#if !UE_BUILD_SHIPPING
	const FString Forced = CVarForceItem.GetValueOnAnyThread();
	if (!Forced.IsEmpty())
	{
		if (const UItemDefinition* ForcedItem = LootTable->FindItem(FName(*Forced)))
		{
			return ForcedItem;
		}
	}
	const bool bUseBias = CVarNoPositionBias.GetValueOnAnyThread() == 0;
#else
	const bool bUseBias = true;
#endif
	return LootTable->Pick(Rank, Random, bUseBias);
}

void UItemSystemComponent::AddEvent(EItemEventType Type, int32 RacerId, ARaceVehicle* Vehicle, const UItemDefinition* Item, int32 BoxIndex)
{
	FItemEvent& Event = StepEvents.AddDefaulted_GetRef();
	Event.Type = Type;
	Event.RacerId = RacerId;
	Event.Vehicle = Vehicle;
	Event.Item = Item;
	Event.BoxIndex = BoxIndex;

#if !UE_BUILD_SHIPPING
	const TCHAR* TypeText = TEXT("PICKUP");
	switch (Type)
	{
	case EItemEventType::Replace: TypeText = TEXT("REPLACE"); break;
	case EItemEventType::RollDone: TypeText = TEXT("ROLL_DONE"); break;
	case EItemEventType::Use: TypeText = TEXT("USE"); break;
	case EItemEventType::BoxRespawn: TypeText = TEXT("RESPAWN"); break;
	default: break;
	}
	UE_LOG(LogCosmicRace, Log, TEXT("ITEM %s t=%.3f car=%s item=%s box=%d"), TypeText, StepTime, *GetNameSafe(Vehicle), *GetItemName(Item), BoxIndex);
#endif
}

void UItemSystemComponent::UseItem(int32 RacerId, ARaceVehicle* Vehicle, float SimTime)
{
	FItemSlot& Slot = Slots[RacerId];
	const UItemDefinition* Item = Slot.Item;
	Slot.State = EItemSlotState::Empty;
	Slot.Item = nullptr;
	Slot.ReadyTime = 0.f;

	if (Item && Item->EffectClass)
	{
		FItemUseContext Context;
		Context.User = Vehicle;
		Context.SimTime = SimTime;
		Item->EffectClass.GetDefaultObject()->Use(Context);
	}
#if !UE_BUILD_SHIPPING
	else
	{
		// Placeholder item: nothing happens. The log line shows the loop is complete.
		UE_LOG(LogCosmicRace, Log, TEXT("Used %s item (no effect yet): %s"), *GetItemName(Item), *GetNameSafe(Vehicle));
	}
#endif
	AddEvent(EItemEventType::Use, RacerId, Vehicle, Item, INDEX_NONE);
}

void UItemSystemComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);

	if (!Tracker)
	{
		return;
	}

	StepTime = StepCount * static_cast<double>(DeltaTime);
	++StepCount;
	StepEvents.Reset();

	// Boxes found on the game thread, and items handed out by dev commands.
	TArray<FPendingGive> Gives;
	{
		FScopeLock ScopeLock(&Lock);
		if (bNewBoxes)
		{
			Boxes = NewBoxes;
			bNewBoxes = false;
		}
		Gives = MoveTemp(PendingGives);
		PendingGives.Reset();
	}

	Tracker->GetRegisteredVehicles(StepVehicles);
	if (Slots.Num() < StepVehicles.Num())
	{
		Slots.SetNum(StepVehicles.Num());
	}

	for (const FPendingGive& Give : Gives)
	{
		const int32 RacerId = StepVehicles.IndexOfByKey(Give.Vehicle.Get());
		if (RacerId != INDEX_NONE)
		{
			Slots[RacerId].State = EItemSlotState::Ready;
			Slots[RacerId].Item = Give.Item;
			Slots[RacerId].RollTimeRemaining = 0.f;
			Slots[RacerId].ReadyTime = 0.f;
		}
	}

	// Boxes come back.
	for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
	{
		FBoxState& Box = Boxes[BoxIndex];
		if (Box.RespawnRemaining > 0.f)
		{
			Box.RespawnRemaining -= DeltaTime;
			if (Box.RespawnRemaining <= 0.f)
			{
				Box.RespawnRemaining = 0.f;
				AddEvent(EItemEventType::BoxRespawn, INDEX_NONE, nullptr, nullptr, BoxIndex);
			}
		}
	}

	// Race places of the previous step: the same whichever component stepped first.
	const FRaceStepView& Race = Tracker->GetViewForStep(SimTime);
	const float PickupRadiusSquared = FMath::Square(PickupRadius);

	for (int32 RacerId = 0; RacerId < StepVehicles.Num(); ++RacerId)
	{
		ARaceVehicle* Vehicle = StepVehicles[RacerId];
		if (!Vehicle)
		{
			continue;
		}
		FItemSlot& Slot = Slots[RacerId];

		// The roulette.
		if (Slot.State == EItemSlotState::Rolling)
		{
			Slot.RollTimeRemaining -= DeltaTime;
			if (Slot.RollTimeRemaining <= 0.f)
			{
				Slot.RollTimeRemaining = 0.f;
				Slot.State = EItemSlotState::Ready;
				Slot.ReadyTime = 0.f;
				AddEvent(EItemEventType::RollDone, RacerId, Vehicle, Slot.Item, INDEX_NONE);
			}
		}
		else if (Slot.State == EItemSlotState::Ready)
		{
			Slot.ReadyTime += DeltaTime;
		}

		// Using the item. A press made in this same physics step waits for the next one, so the
		// result does not depend on whether the car or the item system stepped first.
		const UHoverMovementComponent* Movement = Vehicle->GetHoverMovement();
		const bool bHuman = Race.Racers.IsValidIndex(RacerId) ? Race.Racers[RacerId].bHuman : Vehicle->IsPlayerControlled();
		bool bUse = false;
		if (Movement->GetUseItemPressCount() != Slot.HandledUsePresses && Movement->GetUseItemPressSimTime() < SimTime)
		{
			Slot.HandledUsePresses = Movement->GetUseItemPressCount();
			bUse = true;
		}
		// Computer drivers and test cars do not press buttons for items yet: they use a ready item after a moment.
		if (!bHuman && Slot.State == EItemSlotState::Ready && Slot.ReadyTime >= AIUseDelay)
		{
			bUse = true;
		}
		if (bUse && Slot.State == EItemSlotState::Ready)
		{
			UseItem(RacerId, Vehicle, SimTime);
		}

		// Collecting a box: the car's body centre comes within the pickup radius.
		const TOptional<FVector> Location = URaceTrackerComponent::ReadBodyLocation(Vehicle);
		if (!Location.IsSet())
		{
			continue;
		}
		for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
		{
			FBoxState& Box = Boxes[BoxIndex];
			if (Box.RespawnRemaining > 0.f || FVector::DistSquared(Box.Location, Location.GetValue()) > PickupRadiusSquared)
			{
				continue;
			}

			Box.RespawnRemaining = RespawnTime;
			const int32 Rank = Race.Racers.IsValidIndex(RacerId) && Race.Racers[RacerId].Rank > 0 ? Race.Racers[RacerId].Rank : StepVehicles.Num();

			// A new item always replaces what is in the slot, used or not (developer decision).
			const bool bReplaced = Slot.State != EItemSlotState::Empty;
			Slot.Item = RollItem(Rank);
			Slot.State = EItemSlotState::Rolling;
			Slot.RollTimeRemaining = RollDuration;
			Slot.ReadyTime = 0.f;
			Slot.ReplaceCount += bReplaced ? 1 : 0;
			AddEvent(bReplaced ? EItemEventType::Replace : EItemEventType::Pickup, RacerId, Vehicle, Slot.Item, BoxIndex);
		}
	}

	// Keep the two newest box states for other components' physics steps.
	Swap(NewestBoxesActive, OlderBoxesActive);
	NewestBoxesActive.SetNum(Boxes.Num());
	for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
	{
		NewestBoxesActive[BoxIndex] = Boxes[BoxIndex].RespawnRemaining <= 0.f;
	}
	NewestBoxesSimTime = SimTime;
	bHasBoxView = true;

	FScopeLock ScopeLock(&Lock);
	PublishedSlots = Slots;
	PublishedEvents.Append(StepEvents);
}
