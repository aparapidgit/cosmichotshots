#include "Item/ItemLootTable.h"

#include "Item/ItemDefinition.h"
#include "Math/RandomStream.h"

const UItemDefinition* UItemLootTable::Pick(int32 Rank, FRandomStream& Random, bool bUsePositionBias) const
{
	// One roll decides whether the place counts this time, so the random stream always advances the same way.
	const bool bByPlace = Random.FRand() < PositionBias && bUsePositionBias;

	struct FCandidate
	{
		const UItemDefinition* Item = nullptr;
		float Weight = 0.f;
	};
	TArray<FCandidate, TInlineAllocator<16>> Candidates;
	float TotalWeight = 0.f;
	for (const FItemLootBand& Band : Bands)
	{
		if (bByPlace && (Rank < Band.MinRank || Rank > Band.MaxRank))
		{
			continue;
		}
		for (const FItemLootEntry& Entry : Band.Entries)
		{
			if (!Entry.Item || Rank < Entry.Item->MinRank || Rank > Entry.Item->MaxRank)
			{
				continue;
			}
			const float Weight = Entry.Weight * Entry.Item->RarityWeight;
			if (Weight > 0.f)
			{
				Candidates.Add({ Entry.Item, Weight });
				TotalWeight += Weight;
			}
		}
	}

	const float Roll = Random.FRand() * TotalWeight;
	float Accumulated = 0.f;
	for (const FCandidate& Candidate : Candidates)
	{
		Accumulated += Candidate.Weight;
		if (Roll < Accumulated)
		{
			return Candidate.Item;
		}
	}
	return Candidates.IsEmpty() ? nullptr : Candidates.Last().Item;
}

const UItemDefinition* UItemLootTable::FindItem(FName ItemId) const
{
	for (const FItemLootBand& Band : Bands)
	{
		for (const FItemLootEntry& Entry : Band.Entries)
		{
			if (Entry.Item && Entry.Item->ItemId == ItemId)
			{
				return Entry.Item;
			}
		}
	}
	return nullptr;
}
