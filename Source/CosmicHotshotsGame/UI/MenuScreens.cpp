#include "UI/MenuScreens.h"

#include "CosmicHotshotsGame.h"
#include "AI/AIDifficultyData.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Game/GameFlowSubsystem.h"
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

	/** Humans in a Single Race until split-screen exists. */
	constexpr int32 SetupHumanPlayers = 1;
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
		OpenRaceSetup();
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

void UMainMenuWidget::OpenRaceSetup()
{
	OpenChildScreen(CreateScreen<URaceSetupWidget>());
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

// --- URaceSetupWidget

void URaceSetupWidget::BuildScreen()
{
	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	const FRaceSessionConfig Setup = Flow ? Flow->EditSingleRaceSetup() : FRaceSessionConfig();

	AddTitle(LOCTEXT("SetupTitle", "SINGLE RACE"));

	// Local multiplayer (2-4 players, split-screen) comes later: one player for now, shown as a fixed row.
	AddOptionRow(LOCTEXT("SetupPlayers", "Players"), { FText::AsNumber(SetupHumanPlayers) }, 0);

	TArray<FText> RivalValues;
	for (int32 Rivals = 0; Rivals <= UCosmicHotshotsSettings::MaxGridSize - SetupHumanPlayers; ++Rivals)
	{
		RivalValues.Add(FText::AsNumber(Rivals));
	}
	RivalsRow = AddOptionRow(LOCTEXT("SetupRivals", "Rivals"), RivalValues, Setup.NumAIOpponents);

	// Value 0 = the track's own lap count, value N = N laps (the same meaning as FRaceSessionConfig::NumLaps).
	TArray<FText> LapValues;
	LapValues.Add(LOCTEXT("SetupLapsDefault", "Track default"));
	for (int32 Laps = 1; Laps <= UCosmicHotshotsSettings::MaxSetupLaps; ++Laps)
	{
		LapValues.Add(FText::AsNumber(Laps));
	}
	LapsRow = AddOptionRow(LOCTEXT("SetupLaps", "Laps"), LapValues, Setup.NumLaps);

	TArray<FText> DifficultyValues;
	for (int32 Index = 0; Index < Settings->AIDifficulties.Num(); ++Index)
	{
		const UAIDifficultyData* Difficulty = Settings->AIDifficulties[Index].LoadSynchronous();
		DifficultyValues.Add(Difficulty && !Difficulty->DisplayName.IsEmpty() ? Difficulty->DisplayName
			: FText::Format(LOCTEXT("SetupDifficultyFallback", "Level {0}"), FText::AsNumber(Index + 1)));
	}
	if (DifficultyValues.IsEmpty())
	{
		DifficultyValues.Add(LOCTEXT("SetupDifficultyDefault", "Default"));
	}
	DifficultyRow = AddOptionRow(LOCTEXT("SetupDifficulty", "AI difficulty"), DifficultyValues,
		Setup.AIDifficulty >= 0 ? Setup.AIDifficulty : Settings->DefaultAIDifficulty);

	// Value 0 = off, 1 = on.
	ItemBoxesRow = AddOptionRow(LOCTEXT("SetupItemBoxes", "Item boxes"), { LOCTEXT("SetupOff", "Off"), LOCTEXT("SetupOn", "On") }, Setup.bItemBoxes ? 1 : 0);

	AddButton(LOCTEXT("SetupNext", "Next"));
	NextButton = ItemBoxesRow + 1;
	InitialFocusButton = RivalsRow;
}

void URaceSetupWidget::HandleButton(int32 ButtonIndex)
{
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	if (ButtonIndex != NextButton || !Flow)
	{
		return;
	}

	FRaceSessionConfig& Setup = Flow->EditSingleRaceSetup();
	Setup.NumAIOpponents = GetOptionValue(RivalsRow);
	Setup.NumLaps = GetOptionValue(LapsRow);
	Setup.AIDifficulty = GetOptionValue(DifficultyRow);
	Setup.bItemBoxes = GetOptionValue(ItemBoxesRow) != 0;

	UCharacterSelectWidget* CharacterSelect = CreateScreen<UCharacterSelectWidget>();
	CharacterSelect->SetPartOfRaceSetup(true);
	OpenChildScreen(CharacterSelect);
}

// --- UTrackSelectWidget

void UTrackSelectWidget::BuildScreen()
{
	AddTitle(LOCTEXT("TrackTitle", "SELECT TRACK"));

	Tracks.Reset();
	for (const TSoftObjectPtr<UTrackDefinition>& Entry : GetDefault<UCosmicHotshotsSettings>()->Tracks)
	{
		if (const UTrackDefinition* Track = Entry.LoadSynchronous())
		{
			Tracks.Add(Track);
			AddButton(FText::Format(LOCTEXT("TrackButton", "{0}   ({1} laps)"), Track->DisplayName, FText::AsNumber(Track->DefaultLaps)));
		}
	}
	if (Tracks.IsEmpty())
	{
		AddLine(LOCTEXT("NoTracks", "No tracks in Project Settings > Game > Cosmic Hotshots > Tracks."));
	}
}

void UTrackSelectWidget::HandleButton(int32 ButtonIndex)
{
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	if (!Tracks.IsValidIndex(ButtonIndex) || !Flow)
	{
		return;
	}

	Flow->EditSingleRaceSetup().Track = Tracks[ButtonIndex];
	HideScreen();
	Flow->StartSingleRace(this);
}

#undef LOCTEXT_NAMESPACE
