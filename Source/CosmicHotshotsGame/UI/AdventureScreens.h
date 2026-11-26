// Adventure Mode menu screens: Start new / Load, the save slots, and a plain list of the levels.
//
// The level list is a TEMPORARY stand-in for the hub map (it starts the same races the portals will);
// it stays useful afterwards as a quick way into a level. The screens hold no progress: they ask
// UAdventureSubsystem what is won and open, and tell it which slot and which level the player picked.

#pragma once

#include "CoreMinimal.h"
#include "UI/RaceScreens.h"
#include "AdventureScreens.generated.h"

/** Adventure Mode: Start new, Load, Back. */
UCLASS()
class COSMICHOTSHOTSGAME_API UAdventureMenuWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
};

/** The save slots, for starting a new adventure (an occupied slot asks before it is replaced) or loading one. */
UCLASS()
class COSMICHOTSHOTSGAME_API UAdventureSlotSelectWidget : public URaceScreenWidget
{
	GENERATED_BODY()

public:
	/** Call before the screen is shown. True = pick a slot for a new adventure, false = pick a slot to load. */
	void SetStartNew(bool bInStartNew) { bStartNew = bInStartNew; }

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	void HandleOverwriteChoice(int32 AnswerIndex);

	/** The slot is the active game now: on to the levels. */
	void OpenLevels();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	bool bStartNew = false;

	/** Slot waiting for the "replace this adventure?" answer. */
	int32 PendingSlot = INDEX_NONE;
};

/** Every level of the campaign by section, with its state (won, open, locked). Picking an open one starts its race. */
UCLASS()
class COSMICHOTSHOTSGAME_API UAdventureLevelListWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	/** Buttons 0 .. NumLevels - 1 are the levels, in campaign order; the next one is Back. */
	int32 NumLevels = 0;
};
