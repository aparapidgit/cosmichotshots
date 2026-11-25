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

/** A button that holds one choice out of a list ("Rivals:  < 5 >"). Left / right change it, a click steps forward. */
USTRUCT()
struct FRaceMenuOptionRow
{
	GENERATED_BODY()

	/** Index of the row's button on its screen. */
	int32 ButtonIndex = INDEX_NONE;

	FText Label;
	TArray<FText> Values;
	int32 Value = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text;
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

	/**
	 * Screen stack: puts Child in place of this screen. The back button (gamepad B, Escape) or GoBack()
	 * on the child brings this screen back, with the focus where it was.
	 */
	void OpenChildScreen(URaceScreenWidget* Child);

	/** Closes this screen and shows the one it was opened from. Does nothing on a screen without a parent. */
	void GoBack();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Creates a screen for the same player (to hand to OpenChildScreen). */
	template <typename ScreenType>
	ScreenType* CreateScreen()
	{
		return CreateWidget<ScreenType>(GetOwningPlayer(), ScreenType::StaticClass());
	}

	/** Button that has the focus when the screen is shown for the first time. Set it in BuildScreen. */
	int32 InitialFocusButton = 0;

	/** Fills the screen: call AddTitle / AddLine / AddButton. */
	virtual void BuildScreen() PURE_VIRTUAL(URaceScreenWidget::BuildScreen, );

	/** A button was clicked (mouse, keyboard or the gamepad's accept button). */
	virtual void HandleButton(int32 ButtonIndex) PURE_VIRTUAL(URaceScreenWidget::HandleButton, );

	/** The focus moved to another button (INDEX_NONE = none). */
	virtual void HandleFocusChanged(int32 ButtonIndex) {}

	UTextBlock* AddTitle(const FText& Text);
	UTextBlock* AddLine(const FText& Text, const FLinearColor& Colour = FLinearColor::White);
	URaceMenuButton* AddButton(const FText& Text, const FLinearColor& TextColour = FLinearColor::White);

	/**
	 * Adds an option row: a button showing "Label:  < value >". Left / right pick the previous / next value,
	 * a click steps forward and wraps round. A row with one value is shown without arrows (fixed).
	 * Returns the row's button index.
	 */
	int32 AddOptionRow(const FText& Label, const TArray<FText>& Values, int32 InitialValue);

	/** Chosen value of the option row with this button index (0 when it is not an option row). */
	int32 GetOptionValue(int32 ButtonIndex) const;

	/** An option row changed its value. */
	virtual void HandleOptionChanged(int32 ButtonIndex, int32 Value) {}

private:
	UTextBlock* AddText(const FText& Text, int32 FontSize, const FLinearColor& Colour);

	/** Click on any button: option rows step their value, the others go to HandleButton. */
	void HandleButtonClicked(int32 ButtonIndex);

	FRaceMenuOptionRow* FindOptionRow(int32 ButtonIndex);

	/** Moves the row's value by Step; bWrap = past the last value comes the first. */
	void StepOptionRow(FRaceMenuOptionRow& Row, int32 Step, bool bWrap);
	void RefreshOptionRow(FRaceMenuOptionRow& Row);

	UPROPERTY(Transient)
	TArray<FRaceMenuOptionRow> OptionRows;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Column;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URaceMenuButton>> Buttons;

	/** The screen this one was opened from (screen stack). Kept alive while it is off the viewport. */
	UPROPERTY(Transient)
	TObjectPtr<URaceScreenWidget> ParentScreen;

	int32 FocusedButton = INDEX_NONE;

	/** Last button that had the focus: gets it back when the screen is shown again. */
	int32 LastFocusedButton = INDEX_NONE;
};

/**
 * Character select: one button per race character. As part of the Single Race setup the pick goes into the
 * setup and the track select follows; on its own (a track opened without a setup, dev) it starts a default race.
 */
UCLASS()
class COSMICHOTSHOTSGAME_API UCharacterSelectWidget : public URaceScreenWidget
{
	GENERATED_BODY()

public:
	/** Call before the screen is shown: the pick continues the Single Race setup instead of starting a race. */
	void SetPartOfRaceSetup(bool bInPartOfRaceSetup) { bPartOfRaceSetup = bInPartOfRaceSetup; }

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

	bool bPartOfRaceSetup = false;
};

/** Results: every driver's place and times, then Restart / Change setup / Main menu. */
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
