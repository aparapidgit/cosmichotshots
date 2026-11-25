// Menu screens outside a race: the main menu and a small dialog (message or confirmation).
//
// Built on URaceScreenWidget (plain UMG in C++, gamepad focus, screen stack with a back button).
// The screens contain no game logic: they open other screens or call UGameFlowSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "UI/RaceScreens.h"
#include "MenuScreens.generated.h"

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

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;

private:
	/** Placeholder for the parts of the menu that later steps of Phase 2b add. */
	void ShowNotAvailable(const FText& What);

	void HandleExitChoice(int32 AnswerIndex);
};
