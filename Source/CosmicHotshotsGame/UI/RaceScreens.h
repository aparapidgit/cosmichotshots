// Race screens: character select, results and the start / finish overlay.
//
// Plain UMG built in C++ (no Blueprint assets): grey and functional, polish comes later.
// Every screen is a column of text lines and buttons that works with a gamepad alone: the
// focused button is highlighted, up / down move the focus, the accept button clicks.
// The screens contain no game logic: they read data assets and the race game state, and
// hand choices to UGameFlowSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Race/RaceTypes.h"
#include "RaceScreens.generated.h"

class UCharacterDefinition;
class UTextBlock;
class UVerticalBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRaceMenuButtonClicked, int32 /*ButtonIndex*/);

/** A menu button that reports which one it is. */
UCLASS()
class COSMICHOTSHOTSGAME_API URaceMenuButton : public UButton
{
	GENERATED_BODY()

public:
	void Setup(int32 InIndex);

	FOnRaceMenuButtonClicked OnMenuClicked;

private:
	UFUNCTION()
	void HandleClicked();

	int32 Index = 0;
};

/** Base of the menu screens: a centred column of texts and buttons with gamepad focus. */
UCLASS(Abstract)
class COSMICHOTSHOTSGAME_API URaceScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Adds the screen to the viewport, gives the first button the focus and switches the player to menu input. */
	void ShowScreen();

	/** Removes the screen and gives the input back to the game. */
	void HideScreen();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Fills the screen: call AddTitle / AddLine / AddButton. */
	virtual void BuildScreen() PURE_VIRTUAL(URaceScreenWidget::BuildScreen, );

	/** A button was clicked (mouse, keyboard or the gamepad's accept button). */
	virtual void HandleButton(int32 ButtonIndex) PURE_VIRTUAL(URaceScreenWidget::HandleButton, );

	/** The focus moved to another button (INDEX_NONE = none). */
	virtual void HandleFocusChanged(int32 ButtonIndex) {}

	UTextBlock* AddTitle(const FText& Text);
	UTextBlock* AddLine(const FText& Text, const FLinearColor& Colour = FLinearColor::White);
	URaceMenuButton* AddButton(const FText& Text, const FLinearColor& TextColour = FLinearColor::White);

private:
	UTextBlock* AddText(const FText& Text, int32 FontSize, const FLinearColor& Colour);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Column;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URaceMenuButton>> Buttons;

	int32 FocusedButton = INDEX_NONE;
};

/** Character select: one button per race character; picking one sets up the race and starts it. */
UCLASS()
class COSMICHOTSHOTSGAME_API UCharacterSelectWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
	virtual void HandleFocusChanged(int32 ButtonIndex) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UCharacterDefinition>> Characters;

	/** Shows the passive of the character under the focus. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;
};

/** Results: every driver's place and times, then Restart / Change character. */
UCLASS()
class COSMICHOTSHOTSGAME_API URaceResultsWidget : public URaceScreenWidget
{
	GENERATED_BODY()

public:
	/** Call before ShowScreen. */
	void SetResults(const FRaceResultData& InResults);

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	UPROPERTY(Transient)
	FRaceResultData Results;
};

/** Big text in the middle of the screen: the start countdown, GO, and FINISH with the place. */
UCLASS()
class COSMICHOTSHOTSGAME_API URaceOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BigText;

	/** Seconds "GO!" has been on screen; negative = not shown yet. */
	float GoTime = -1.f;
	bool bCountdownSeen = false;
};

namespace CosmicHotshots::RaceUI
{
	/** 83.456 -> "1:23.456". */
	COSMICHOTSHOTSGAME_API FString FormatRaceTime(float Seconds);

	/** Starts a race with this character for player 1 and the default setup (full grid, default difficulty). */
	COSMICHOTSHOTSGAME_API void StartRaceWithCharacter(const UObject* WorldContext, const UCharacterDefinition* Character);
}
