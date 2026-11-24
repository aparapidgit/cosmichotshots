// Item system: item boxes, the roulette, each car's single item slot and using items.
//
// Lives on ARaceGameState and runs in the fixed physics step, so collecting and using items
// is identical at every frame rate. Per step it:
//  - brings back boxes whose respawn time is over;
//  - checks every car (in grid order) against the active boxes by distance: no overlap
//    events, those arrive once per frame;
//  - rolls an item from the loot table for the car's race place, with one seeded random
//    stream; a new item REPLACES an unused held one (developer decision);
//  - runs the roulette timers and uses an item when its car pressed the use button.
// Computer drivers use a ready item after a short delay. What happened is handed to the game
// thread as events: boxes hide and reappear, and the HUD shows the slot. This class contains
// no item behaviour: that is UItemEffect.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "ItemSystemComponent.generated.h"

class AItemBox;
class ARaceVehicle;
class UItemDefinition;
class UItemLootTable;
class URaceTrackerComponent;

UENUM(BlueprintType)
enum class EItemSlotState : uint8
{
	Empty,
	/** A box was just collected: the roulette is running, the item cannot be used yet. */
	Rolling,
	Ready
};

/** One car's item slot. */
USTRUCT(BlueprintType)
struct FItemSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EItemSlotState State = EItemSlotState::Empty;

	/** The item in the slot (also during the roulette, which only delays showing it). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<const UItemDefinition> Item;

	/** Seconds of roulette left. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	float RollTimeRemaining = 0.f;

	/** How many times an unused item was replaced by a new box (the HUD animates a change). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 ReplaceCount = 0;

	/** Seconds the item has been ready (computer drivers use it after a delay). */
	float ReadyTime = 0.f;
	/** Use-button presses of the car already handled. */
	int32 HandledUsePresses = 0;
};

UENUM(BlueprintType)
enum class EItemEventType : uint8
{
	Pickup,
	/** Pickup that threw away an unused held item. */
	Replace,
	RollDone,
	Use,
	BoxRespawn
};

/** Something the item system did in a physics step, for HUD, VFX and audio. */
USTRUCT(BlueprintType)
struct FItemEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EItemEventType Type = EItemEventType::Pickup;

	/** Car the event is about; null for BoxRespawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<ARaceVehicle> Vehicle;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<const UItemDefinition> Item;

	/** Index of the box (Pickup, Replace, BoxRespawn), else INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 BoxIndex = INDEX_NONE;

	int32 RacerId = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemEvent, const FItemEvent&, Event);

UCLASS(ClassGroup = (Race))
class COSMICHOTSHOTSGAME_API UItemSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemSystemComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	/** The race tracker that knows the cars and their places. */
	void SetTracker(URaceTrackerComponent* InTracker);

	/** The car's item slot as of the last frame. Game thread. Empty slot when the car is not in the race. */
	FItemSlot GetSlot(const ARaceVehicle* Vehicle) const;

	/** Dev: puts an item straight into a car's slot, ready to use (replaces what was there). Game thread. */
	void GiveItem(ARaceVehicle* Vehicle, const UItemDefinition* Item);

	const UItemLootTable* GetLootTable() const { return LootTable; }

	/**
	 * Which boxes were active (collectable) after the PREVIOUS physics step, by box index (boxes sorted by
	 * name). For code inside another component's physics step (the AI): the same answer whether the item
	 * system has already run its part of the current step or not.
	 */
	const TArray<bool>& GetBoxesActiveForStep(float SimTime) const;

	/** Item events, one frame after the physics step that produced them. */
	UPROPERTY(BlueprintAssignable, Category = "Item")
	FOnItemEvent OnItemEvent;

private:
	struct FBoxState
	{
		FVector Location = FVector::ZeroVector;
		/** Seconds until it comes back; 0 = active. */
		float RespawnRemaining = 0.f;
	};

	struct FPendingGive
	{
		TWeakObjectPtr<ARaceVehicle> Vehicle;
		const UItemDefinition* Item = nullptr;
	};

	void GatherBoxes();
	const UItemDefinition* RollItem(int32 Rank);
	void UseItem(int32 RacerId, ARaceVehicle* Vehicle, float SimTime);
	void AddEvent(EItemEventType Type, int32 RacerId, ARaceVehicle* Vehicle, const UItemDefinition* Item, int32 BoxIndex);
	FString GetItemName(const UItemDefinition* Item) const;

	UPROPERTY(Transient)
	TObjectPtr<URaceTrackerComponent> Tracker;

	UPROPERTY(Transient)
	TObjectPtr<const UItemLootTable> LootTable;

	/** Box actors by box index. Game thread. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AItemBox>> BoxActors;
	bool bBoxesGathered = false;

	// Tuning, copied once.
	float RespawnTime = 5.f;
	float RollDuration = 1.5f;
	float PickupRadius = 250.f;
	float AIUseDelay = 1.f;

	// Owned by the physics step.
	TArray<FBoxState> Boxes;
	TArray<FItemSlot> Slots;
	TArray<ARaceVehicle*> StepVehicles;
	TArray<FItemEvent> StepEvents;
	FRandomStream Random;
	int64 StepCount = 0;
	double StepTime = 0.0;
	/** Box states after the two newest steps (see GetBoxesActiveForStep). */
	TArray<bool> NewestBoxesActive;
	TArray<bool> OlderBoxesActive;
	float NewestBoxesSimTime = 0.f;
	bool bHasBoxView = false;

	// Handed between the threads under the lock.
	TArray<FBoxState> NewBoxes;
	bool bNewBoxes = false;
	TArray<FPendingGive> PendingGives;
	TArray<FItemSlot> PublishedSlots;
	TArray<FItemEvent> PublishedEvents;
	mutable FCriticalSection Lock;
};
