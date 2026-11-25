#include "UI/MenuScreens.h"

#include "CosmicHotshotsGame.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "MenuScreens"

namespace
{
	// Buttons of the main menu, top to bottom.
	constexpr int32 MainMenuSingleRaceButton = 0;
	constexpr int32 MainMenuAdventureButton = 1;
	constexpr int32 MainMenuOptionsButton = 2;
	constexpr int32 MainMenuExitButton = 3;

	// Answers of the exit confirmation. "Cancel" comes first and has the focus.
	constexpr int32 ExitDialogCancelAnswer = 0;
	constexpr int32 ExitDialogExitAnswer = 1;
}

// --- UMenuDialogWidget

void UMenuDialogWidget::Setup(const FText& InTitle, const FText& InMessage, const TArray<FText>& InAnswers, int32 InInitialFocus)
{
	Title = InTitle;
	Message = InMessage;
	Answers = InAnswers;
	InitialFocusButton = InInitialFocus;
}

void UMenuDialogWidget::BuildScreen()
{
	AddTitle(Title);
	if (!Message.IsEmpty())
	{
		AddLine(Message);
	}
	for (const FText& Answer : Answers)
	{
		AddButton(Answer);
	}
}

void UMenuDialogWidget::HandleButton(int32 ButtonIndex)
{
	// Back to the screen below first, so the answer can open the next screen from there (or quit).
	GoBack();
	OnChoice.Broadcast(ButtonIndex);
}

// --- UMainMenuWidget

void UMainMenuWidget::BuildScreen()
{
	AddTitle(LOCTEXT("MainMenuTitle", "COSMIC HOTSHOTS"));
	AddButton(LOCTEXT("SingleRace", "Single Race"));
	AddButton(LOCTEXT("AdventureMode", "Adventure Mode"));
	AddButton(LOCTEXT("Options", "Options"));
	AddButton(LOCTEXT("ExitGame", "Exit Game"));
}

void UMainMenuWidget::HandleButton(int32 ButtonIndex)
{
	switch (ButtonIndex)
	{
	case MainMenuSingleRaceButton:
		// Until the race setup screen exists: straight to the character select, default race.
		OpenChildScreen(CreateScreen<UCharacterSelectWidget>());
		break;

	case MainMenuAdventureButton:
		ShowNotAvailable(LOCTEXT("AdventureMode", "Adventure Mode"));
		break;

	case MainMenuOptionsButton:
		ShowNotAvailable(LOCTEXT("Options", "Options"));
		break;

	case MainMenuExitButton:
	{
		UMenuDialogWidget* Dialog = CreateScreen<UMenuDialogWidget>();
		Dialog->Setup(LOCTEXT("ExitTitle", "EXIT GAME"), LOCTEXT("ExitMessage", "Exit the game?"),
			{ LOCTEXT("ExitCancel", "Cancel"), LOCTEXT("ExitConfirm", "Exit game") }, ExitDialogCancelAnswer);
		Dialog->OnChoice.AddUObject(this, &UMainMenuWidget::HandleExitChoice);
		OpenChildScreen(Dialog);
		break;
	}

	default:
		break;
	}
}

void UMainMenuWidget::ShowNotAvailable(const FText& What)
{
	UMenuDialogWidget* Dialog = CreateScreen<UMenuDialogWidget>();
	Dialog->Setup(What.ToUpper(), LOCTEXT("NotAvailable", "Not available yet."), { LOCTEXT("DialogBack", "Back") });
	OpenChildScreen(Dialog);
}

void UMainMenuWidget::HandleExitChoice(int32 AnswerIndex)
{
	if (AnswerIndex == ExitDialogExitAnswer)
	{
		UE_LOG(LogCosmicRace, Log, TEXT("FLOW main menu: exit game"));
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
	}
}

#undef LOCTEXT_NAMESPACE
