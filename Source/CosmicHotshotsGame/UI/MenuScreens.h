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
	/** Placeholder for the parts of the menu that later steps of Phase 2b add. */
	void ShowNotAvailable(const FText& What);

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
