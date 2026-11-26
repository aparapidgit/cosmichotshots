// Adventure Mode data: the campaign, its sections and its levels, and the save file.
//
// A campaign is a list of sections; a section is an open area of the hub closed by a gate and holds a few
// levels (portals); a level is one race. The order of sections and levels is the order in the data assets:
// adding, removing or reordering levels needs asset changes only. Progress is saved by level id, so it
// survives a reorder.
//
// Unlock rule (developer decision): levels open one after the other. A level opens when the level before
// it has been WON (1st place); the gate of a section opens when every level of the section before is won.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "AdventureDefinitions.generated.h"

class UCharacterDefinition;
class UTrackDefinition;

/** One Adventure level: the race behind one portal. */
UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UAdventureLevelDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Id of the level and of its portal. Progress is saved under this id: never change it once players have saves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName PortalId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	TObjectPtr<const UTrackDefinition> Track;

	/** 0 = the track's default lap count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0"))
	int32 NumLaps = 0;

	/** Computer drivers in the race (the player is extra). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0", ClampMax = "7"))
	int32 NumRivals = 3;

	/** Index into the project's AI difficulties. Negative = the project default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	int32 AIDifficulty = -1;

	/**
	 * The rivals' characters, in grid order. Empty (or shorter than NumRivals) = the missing ones are drawn at
	 * random from the other characters. The campaign's player character is never a rival.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	TArray<TObjectPtr<const UCharacterDefinition>> Rivals;

	/** Video played when the player enters the portal. Empty = the cinematic placeholder screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic", meta = (AllowedClasses = "/Script/MediaAssets.MediaSource"))
	FSoftObjectPath IntroVideo;
};

/** One section of the hub: an open area behind a gate, with its levels. */
USTRUCT(BlueprintType)
struct FAdventureSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Section")
	FText DisplayName;

	/** Levels of the section, in the order they open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Section")
	TArray<TObjectPtr<const UAdventureLevelDefinition>> Levels;

	/** Video played the first time the player enters the section. Empty = the cinematic placeholder screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic", meta = (AllowedClasses = "/Script/MediaAssets.MediaSource"))
	FSoftObjectPath IntroVideo;
};

/** The whole Adventure campaign. */
UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UAdventureCampaignDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Whose adventure this is: the character the player drives in every Adventure race (Spark). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign")
	TObjectPtr<const UCharacterDefinition> PlayerCharacter;

	/** Video played once, when a new adventure starts. Empty = the cinematic placeholder screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic", meta = (AllowedClasses = "/Script/MediaAssets.MediaSource"))
	FSoftObjectPath IntroVideo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign")
	TArray<FAdventureSection> Sections;

	/** Levels of all sections together. A "level index" counts through the sections in order, from 0. */
	int32 GetNumLevels() const;

	/** Null when the index is out of range (or the data has an empty entry). */
	const UAdventureLevelDefinition* GetLevel(int32 LevelIndex) const;

	/** Section a level belongs to, INDEX_NONE when the level index is out of range. */
	int32 GetSectionOfLevel(int32 LevelIndex) const;

	/** Level index of the first level of a section. */
	int32 GetFirstLevelOfSection(int32 SectionIndex) const;
};

/** One Adventure save slot. */
UCLASS()
class COSMICHOTSHOTSGAME_API UAdventureSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** PortalId of every level the player has won. */
	UPROPERTY()
	TArray<FName> WonLevels;

	/** The campaign intro has been shown. */
	UPROPERTY()
	bool bIntroSeen = false;

	/** Sections whose intro has been shown (section index). */
	UPROPERTY()
	TArray<int32> SeenSectionIntros;
};
