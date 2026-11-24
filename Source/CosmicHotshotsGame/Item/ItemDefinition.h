// Item definition and item effect: what an item box can give and what using it does.
//
// The real power-ups do not exist yet. The whole item loop (box, roulette, slot, use) runs
// with one placeholder item, "Empty", that has no effect class. Adding a real power-up later
// needs only: a new UItemDefinition asset, a UItemEffect subclass, and an entry in the loot
// table. Boxes, HUD and vehicle code stay untouched.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDefinition.generated.h"

class ARaceVehicle;
class UTexture2D;

/** What an item effect is told when it is used. */
struct FItemUseContext
{
	/** The car that used the item. */
	ARaceVehicle* User = nullptr;
	/** The value the engine passed to the physics step the item was used in (stamps effects queued on other cars). */
	float SimTime = 0.f;
};

/**
 * What an item does when it is used. One subclass per power-up. Use() runs inside the fixed
 * physics step: change other cars only through their modifier stack's Queue(), like passives do.
 */
UCLASS(Abstract)
class COSMICHOTSHOTSGAME_API UItemEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual void Use(const FItemUseContext& Context) const PURE_VIRTUAL(UItemEffect::Use, );
};

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Short id for debug commands and logs, e.g. "Empty". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** Icon for the HUD item slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** How common the item is: multiplies its weight in every loot table entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0.0"))
	float RarityWeight = 1.f;

	/** What using the item does. Empty = placeholder item: nothing happens, only a log line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSubclassOf<UItemEffect> EffectClass;

	/** Best and worst race place that can get this item (1 = leader). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MinRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxRank = 8;
};
