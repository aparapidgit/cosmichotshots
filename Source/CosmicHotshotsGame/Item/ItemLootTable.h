// Item loot table: which item a box gives, depending on the race place of the car.
//
// Entries are grouped in rank bands (for example 1st, 2nd-3rd, 4th-6th, 7th-8th), so trailing
// cars can get more helpful items than the leaders (comeback pressure). PositionBias says how
// strongly the place counts: keep it mild, because the passives already help trailing cars.
// For now the table holds one placeholder item, "Empty".

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemLootTable.generated.h"

class UItemDefinition;
struct FRandomStream;

USTRUCT(BlueprintType)
struct FItemLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<const UItemDefinition> Item;

	/** Relative chance within the band (multiplied by the item's RarityWeight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct FItemLootBand
{
	GENERATED_BODY()

	/** Race places this band covers, both included (1 = leader). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxRank = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FItemLootEntry> Entries;
};

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UItemLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Rolls an item for a car in the given race place. With bUsePositionBias false (or by the
	 * chance 1 - PositionBias) the place is ignored and every band counts. Items whose own
	 * MinRank / MaxRank exclude the place are never given. Null when nothing fits.
	 */
	const UItemDefinition* Pick(int32 Rank, FRandomStream& Random, bool bUsePositionBias) const;

	/** Item with this id anywhere in the table, or null. */
	const UItemDefinition* FindItem(FName ItemId) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FItemLootBand> Bands;

	/** 1 = the place always decides the band, 0 = the place never matters. In between: the chance that it does. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PositionBias = 0.7f;
};
