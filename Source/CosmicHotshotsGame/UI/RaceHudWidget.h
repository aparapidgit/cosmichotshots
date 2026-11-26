// Race HUD of one local player (TEMPORARY look: plain text and bars until the real visual HUD exists).
//
// Standings, own place and lap, Energy Core meter, item slot, passive indicator and active effects.
// Plain UMG built in C++. The HUD holds no game logic: every tick it reads the race game state, the item
// system, the player's vehicle (boost meter, modifier stack, passive) and shows what it finds.
// What is drawn follows the player's settings: the show / hide button AND the Options toggle of each
// element (UPlayerSettingsSubsystem::IsHudElementDrawn). Hiding never changes the mechanics underneath.
// One widget per local player, added to that player's part of the screen, so split-screen needs nothing new.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/PlayerSettingsSubsystem.h"
#include "RaceHudWidget.generated.h"

class UCanvasPanel;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class COSMICHOTSHOTSGAME_API URaceHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UTextBlock* MakeText(int32 FontSize);

	/** Remembers which widget shows a HUD element, so the visibility rule is applied in one place. */
	void RegisterElement(EHudElement Element, UWidget* Widget);

	void UpdateStandings(const class ARaceGameState& RaceState, const class ARaceVehicle* OwnVehicle, bool bShowItemMarker);
	void UpdatePlaceAndLap(const ARaceGameState& RaceState, const ARaceVehicle& OwnVehicle);
	void UpdateEnergyCore(const ARaceVehicle& OwnVehicle);
	void UpdateItemSlot(const ARaceGameState& RaceState, const ARaceVehicle& OwnVehicle);
	void UpdatePassiveAndEffects(const ARaceVehicle& OwnVehicle);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Root;

	/** Widget of each HUD element that has one, indexed by EHudElement (null = no own widget). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> ElementWidgets;

	/** One line per grid slot, leader first. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StandingLines;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlaceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LapText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> EnergyCoreBox;

	/** One bar per segment of the Energy Core reservoir. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> EnergyCoreSegments;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ItemSlotText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PassiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsText;
};
