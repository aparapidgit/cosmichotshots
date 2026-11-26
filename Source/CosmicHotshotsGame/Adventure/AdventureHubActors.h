// Adventure hub actors: the portal of one level, the gate of one section, and the placeholder layout.
//
// Portals and gates hold no progress. They show what UAdventureSubsystem says (locked / open / won,
// gate closed / open) and update when that changes, so a win, ch.Adventure.UnlockAll or ch.Adventure.Reset
// is visible at once. A designed hub map can place these actors by hand; the placeholder hub spawns them.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AdventureHubActors.generated.h"

class UAdventureCampaignDefinition;
class UAdventureHubTuningData;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Where things stand in the placeholder hub: sections in a row along +X, the first one starting at the origin. */
namespace CosmicHotshots::HubLayout
{
	/** Where the portal of a level stands and which way it faces (into its section). */
	COSMICHOTSHOTSGAME_API FTransform GetPortalTransform(const UAdventureCampaignDefinition& Campaign, const UAdventureHubTuningData& Tuning, int32 LevelIndex);

	/** Centre of the opening that leads INTO a section (section 1 and later), at floor level. */
	COSMICHOTSHOTSGAME_API FVector GetGateLocation(const UAdventureHubTuningData& Tuning, int32 SectionIndex);

	/** Where the player stands when the hub is entered from the menu, facing along the row of sections. */
	COSMICHOTSHOTSGAME_API FTransform GetPlayerStart(const UAdventureHubTuningData& Tuning);
}

/** How a portal looks and what happens when the player tries to enter it. */
UENUM()
enum class EAdventurePortalState : uint8
{
	Locked,
	Open,
	Won,
};

/** The portal of one Adventure level: a pad, a door in the level's state colour and a label. */
UCLASS()
class COSMICHOTSHOTSGAME_API AAdventurePortal : public AActor
{
	GENERATED_BODY()

public:
	AAdventurePortal();

	/** Which level of the campaign this portal leads to (campaign order, from 0). */
	void SetLevelIndex(int32 InLevelIndex);
	int32 GetLevelIndex() const { return LevelIndex; }

	EAdventurePortalState GetState() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Level index for portals placed in a map by hand. */
	UPROPERTY(EditAnywhere, Category = "Adventure", meta = (ClampMin = "0"))
	int32 LevelIndex = 0;

private:
	void Refresh();

	UPROPERTY(VisibleAnywhere, Category = "Adventure")
	TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(VisibleAnywhere, Category = "Adventure")
	TObjectPtr<UStaticMeshComponent> Door;

	UPROPERTY(VisibleAnywhere, Category = "Adventure")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColourMaterial;

	/** State the look was last built for; INDEX_NONE = never. */
	int32 ShownState = INDEX_NONE;
};

/** The gate into a section: a block in the opening that goes away when every level of the section before is won. */
UCLASS()
class COSMICHOTSHOTSGAME_API AAdventureGate : public AActor
{
	GENERATED_BODY()

public:
	AAdventureGate();

	/** The section this gate leads into. */
	void SetSectionIndex(int32 InSectionIndex);
	int32 GetSectionIndex() const { return SectionIndex; }

	bool IsOpen() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Adventure", meta = (ClampMin = "1"))
	int32 SectionIndex = 1;

private:
	void Refresh();

	UPROPERTY(VisibleAnywhere, Category = "Adventure")
	TObjectPtr<UStaticMeshComponent> Block;

	UPROPERTY(VisibleAnywhere, Category = "Adventure")
	TObjectPtr<UTextRenderComponent> Label;

	/** 1 = open, 0 = closed, INDEX_NONE = never shown. */
	int32 ShownOpen = INDEX_NONE;
};
