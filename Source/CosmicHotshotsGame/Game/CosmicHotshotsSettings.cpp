#include "Game/CosmicHotshotsSettings.h"

#include "AI/AIDifficultyData.h"
#include "Character/CharacterDefinition.h"

const UAIDifficultyData* UCosmicHotshotsSettings::GetAIDifficulty(int32 Index) const
{
	if (AIDifficulties.IsEmpty())
	{
		return GetDefault<UAIDifficultyData>();
	}
	const UAIDifficultyData* Difficulty = AIDifficulties[FMath::Clamp(Index, 0, AIDifficulties.Num() - 1)].LoadSynchronous();
	return Difficulty ? Difficulty : GetDefault<UAIDifficultyData>();
}

const UCharacterDefinition* UCosmicHotshotsSettings::FindCharacter(const FString& IndexOrName) const
{
	if (IndexOrName.IsNumeric())
	{
		const int32 Index = FCString::Atoi(*IndexOrName);
		return Characters.IsValidIndex(Index) ? Characters[Index].LoadSynchronous() : nullptr;
	}

	for (const TSoftObjectPtr<UCharacterDefinition>& Entry : Characters)
	{
		const FString AssetName = Entry.GetAssetName();

		// "DA_Character_TestHeavy" matches both the full name and "TestHeavy".
		FString ShortName = AssetName;
		int32 LastUnderscore = INDEX_NONE;
		if (AssetName.FindLastChar(TEXT('_'), LastUnderscore))
		{
			ShortName = AssetName.Mid(LastUnderscore + 1);
		}

		if (AssetName.Equals(IndexOrName, ESearchCase::IgnoreCase) || ShortName.Equals(IndexOrName, ESearchCase::IgnoreCase))
		{
			return Entry.LoadSynchronous();
		}
	}
	return nullptr;
}
