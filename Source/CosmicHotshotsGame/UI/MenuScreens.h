// Menu screens outside a race: the main menu and a small dialog (message or confirmation).
//
// Built on URaceScreenWidget (plain UMG in C++, gamepad focus, screen stack with a back button).
// The screens contain no game logic: they open other screens or call UGameFlowSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "UI/RaceScreens.h"
#include "MenuScreens.generated.h"

class UTrackDefinition;

/**
 * A title, one line of text and a row of answers. Used for confirmations ("Exit the game?") and for
 * plain messages. Any answer closes the dialog; OnChoice then says which one it was. The back button
 * closes it without an answer.
 */
UCLASS()
class COSMICHOTSHOTSGAME_API UMenuDialogWidget : public URaceScreenWidget
{
	GENERATED_BODY()

public:
	/** Call before the dialog is shown. InInitialFocus = the answer under the focus (the safe one). */
	void Setup(const FText& InTitle, const FText& InMessage, const TArray<FText>& InAnswers, int32 InInitialFocus = 0);

	FOnRaceMenuButtonClicked OnChoice;

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	FText Title;
	FText Message;
	TArray<FText> Answers;
};

/** Main menu: Single Race, Adventure Mode, Options, Exit Game. */
UCLASS()
class COSMICHOTSHOTSGAME_API UMainMenuWidget : public URaceScreenWidget
{
	GENERATED_BODY()

public:
	/** Opens the Single Race setup on top of the menu (the Single Race button, and "Change setup" after a race). */
	void OpenRaceSetup();

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	void HandleExitChoice(int32 AnswerIndex);
};

/**
 * Single Race setup: players, rivals, laps, AI difficulty, item boxes. "Next" writes the choices into the
 * flow subsystem's Single Race setup and goes on to the character select, then the track select.
 * The screen starts from the last setup, so "Change setup" after a race shows the previous choices.
 */
UCLASS()
class COSMICHOTSHOTSGAME_API URaceSetupWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	// Button indices of the option rows.
	int32 RivalsRow = INDEX_NONE;
	int32 LapsRow = INDEX_NONE;
	int32 DifficultyRow = INDEX_NONE;
	int32 ItemBoxesRow = INDEX_NONE;
	int32 NextButton = INDEX_NONE;
};

/** Options: the list of option groups. Opened from the main menu and from the pause menu. */
UCLASS()
class COSMICHOTSHOTSGAME_API UOptionsWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
};

/**
 * Options > Camera: base FOV (a stepped slider with the presets Close / Normal / Wide on it), camera shake
 * and speed lines. Every change goes to the player's settings at once: saved, and shown on the running
 * race when the screen was opened from the pause menu.
 */
UCLASS()
class COSMICHOTSHOTSGAME_API UCameraOptionsWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
	virtual void HandleOptionChanged(int32 ButtonIndex, int32 Value) override;

private:
	/** FOV of every position of the slider, in degrees. */
	TArray<float> FovValues;

	int32 FovRow = INDEX_NONE;
	int32 ShakeRow = INDEX_NONE;
	int32 SpeedLinesRow = INDEX_NONE;
};

/**
 * Options > HUD: one On / Off row per HUD element (EHudElement), all on by default. These toggles are
 * independent of the show / hide HUD button: an element is drawn only when both allow it.
 */
UCLASS()
class COSMICHOTSHOTSGAME_API UHudOptionsWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
	virtual void HandleOptionChanged(int32 ButtonIndex, int32 Value) override;
};

/** Pause menu of a race: Resume, Options, Restart, Main menu. The game is paused while it is open. */
UCLASS()
class COSMICHOTSHOTSGAME_API UPauseMenuWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
	virtual bool HandleBackOnRoot() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Resume();
	void HandleMainMenuChoice(int32 AnswerIndex);
};

/** Track select: one button per track of the project; picking one starts the race. */
UCLASS()
class COSMICHOTSHOTSGAME_API UTrackSelectWidget : public URaceScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UTrackDefinition>> Tracks;
};
