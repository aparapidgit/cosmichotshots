#include "UI/MenuScreens.h"

#include "CosmicHotshotsGame.h"
#include "AI/AIDifficultyData.h"
#include "Camera/CameraTuningData.h"
#include "Input/DrivingInputConfig.h"
#include "Input/OnFootInputConfig.h"
#include "Input/SystemInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Player/PlayerSettingsSubsystem.h"
#include "UI/AdventureScreens.h"
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

	// Buttons of the Options screen.
	constexpr int32 OptionsCameraButton = 0;
	constexpr int32 OptionsHudButton = 1;
	constexpr int32 OptionsControlsButton = 2;

	/** Input action assets are named "IA_<Action>": the overview shows the part after this prefix. */
	const TCHAR* const InputActionNamePrefix = TEXT("IA_");

	const FLinearColor ControlsGroupColour(1.f, 0.85f, 0.2f, 1.f);
	const FLinearColor ControlsNoteColour(0.7f, 0.7f, 0.7f, 1.f);

	// Buttons of the pause menu, top to bottom.
	constexpr int32 PauseResumeButton = 0;
	constexpr int32 PauseOptionsButton = 1;
	constexpr int32 PauseRestartButton = 2;
	constexpr int32 PauseMainMenuButton = 3;

	// Answers of the "leave the race" confirmation. "Cancel" comes first and has the focus.
	constexpr int32 LeaveDialogCancelAnswer = 0;
	constexpr int32 LeaveDialogLeaveAnswer = 1;

	/** Two FOV values closer than this (degrees) are the same position of the FOV slider. */
	constexpr float FovMatchTolerance = 0.5f;

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
		OpenChildScreen(CreateScreen<UAdventureMenuWidget>());
		break;

	case MainMenuOptionsButton:
		OpenChildScreen(CreateScreen<UOptionsWidget>());
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

// --- UOptionsWidget

void UOptionsWidget::BuildScreen()
{
	AddTitle(LOCTEXT("OptionsTitle", "OPTIONS"));
	AddButton(LOCTEXT("OptionsCamera", "Camera"));
	AddButton(LOCTEXT("OptionsHud", "HUD"));
	AddButton(LOCTEXT("OptionsControls", "Controls"));
	AddButton(LOCTEXT("OptionsBack", "Back"));
}

void UOptionsWidget::HandleButton(int32 ButtonIndex)
{
	if (ButtonIndex == OptionsCameraButton)
	{
		OpenChildScreen(CreateScreen<UCameraOptionsWidget>());
	}
	else if (ButtonIndex == OptionsHudButton)
	{
		OpenChildScreen(CreateScreen<UHudOptionsWidget>());
	}
	else if (ButtonIndex == OptionsControlsButton)
	{
		OpenChildScreen(CreateScreen<UControlsOptionsWidget>());
	}
	else
	{
		GoBack();
	}
}

// --- UCameraOptionsWidget

void UCameraOptionsWidget::BuildScreen()
{
	AddTitle(LOCTEXT("CameraTitle", "CAMERA"));

	UPlayerSettingsSubsystem* Settings = UPlayerSettingsSubsystem::Get(GetOwningPlayer());
	const UCameraTuningData* Tuning = UPlayerSettingsSubsystem::GetCameraTuning();
	const FPlayerSettingsData Current = Settings ? Settings->GetSettings() : FPlayerSettingsData();
	const float CurrentFov = Settings ? Settings->GetBaseFov() : Tuning->FovNormal;

	// The slider: every step between the limits, plus the three presets wherever they fall.
	FovValues.Reset();
	for (float Fov = Tuning->MinBaseFov; Fov <= Tuning->MaxBaseFov + UE_KINDA_SMALL_NUMBER; Fov += FMath::Max(Tuning->FovOptionStep, 1.f))
	{
		FovValues.Add(Fov);
	}
	for (const float Preset : { Tuning->FovClose, Tuning->FovNormal, Tuning->FovWide })
	{
		if (Preset >= Tuning->MinBaseFov && Preset <= Tuning->MaxBaseFov && !FovValues.ContainsByPredicate([Preset](float Fov) { return FMath::IsNearlyEqual(Fov, Preset, FovMatchTolerance); }))
		{
			FovValues.Add(Preset);
		}
	}
	FovValues.Sort();

	TArray<FText> FovTexts;
	int32 CurrentFovIndex = 0;
	for (int32 Index = 0; Index < FovValues.Num(); ++Index)
	{
		const float Fov = FovValues[Index];
		const FText Degrees = FText::AsNumber(FMath::RoundToInt32(Fov));
		const FText PresetName = FMath::IsNearlyEqual(Fov, Tuning->FovClose, FovMatchTolerance) ? LOCTEXT("FovClose", "Close")
			: FMath::IsNearlyEqual(Fov, Tuning->FovNormal, FovMatchTolerance) ? LOCTEXT("FovNormal", "Normal")
			: FMath::IsNearlyEqual(Fov, Tuning->FovWide, FovMatchTolerance) ? LOCTEXT("FovWide", "Wide")
			: FText::GetEmpty();
		FovTexts.Add(PresetName.IsEmpty() ? Degrees : FText::Format(LOCTEXT("FovPreset", "{0}  ({1})"), Degrees, PresetName));

		if (FMath::Abs(Fov - CurrentFov) < FMath::Abs(FovValues[CurrentFovIndex] - CurrentFov))
		{
			CurrentFovIndex = Index;
		}
	}
	FovRow = AddOptionRow(LOCTEXT("CameraFov", "Field of view"), FovTexts, CurrentFovIndex);

	// Row values are the EEffectIntensity values in order.
	const TArray<FText> IntensityTexts = { LOCTEXT("IntensityOff", "Off"), LOCTEXT("IntensityLow", "Low"), LOCTEXT("IntensityNormal", "Normal") };
	ShakeRow = AddOptionRow(LOCTEXT("CameraShake", "Camera shake"), IntensityTexts, static_cast<int32>(Current.CameraShake));
	SpeedLinesRow = AddOptionRow(LOCTEXT("CameraSpeedLines", "Speed lines"), IntensityTexts, static_cast<int32>(Current.SpeedLines));

	AddButton(LOCTEXT("CameraBack", "Back"));
}

void UCameraOptionsWidget::HandleOptionChanged(int32 ButtonIndex, int32 Value)
{
	UPlayerSettingsSubsystem* Settings = UPlayerSettingsSubsystem::Get(GetOwningPlayer());
	if (!Settings)
	{
		return;
	}

	if (ButtonIndex == FovRow && FovValues.IsValidIndex(Value))
	{
		Settings->SetBaseFov(FovValues[Value]);
	}
	else if (ButtonIndex == ShakeRow)
	{
		Settings->SetCameraShake(static_cast<EEffectIntensity>(Value));
	}
	else if (ButtonIndex == SpeedLinesRow)
	{
		Settings->SetSpeedLines(static_cast<EEffectIntensity>(Value));
	}
}

void UCameraOptionsWidget::HandleButton(int32 ButtonIndex)
{
	// The only plain button is Back.
	GoBack();
}

// --- UHudOptionsWidget

void UHudOptionsWidget::BuildScreen()
{
	AddTitle(LOCTEXT("HudTitle", "HUD"));

	// One row per HUD element, in enum order: the row's button index IS the EHudElement value. Value 0 = off, 1 = on.
	UPlayerSettingsSubsystem* Settings = UPlayerSettingsSubsystem::Get(GetOwningPlayer());
	const TArray<FText> OnOffTexts = { LOCTEXT("HudOff", "Off"), LOCTEXT("HudOn", "On") };
	for (int32 Index = 0; Index < static_cast<int32>(EHudElement::Count); ++Index)
	{
		const EHudElement Element = static_cast<EHudElement>(Index);
		AddOptionRow(UPlayerSettingsSubsystem::GetHudElementName(Element), OnOffTexts, !Settings || Settings->IsHudElementEnabled(Element) ? 1 : 0);
	}

	AddLine(LOCTEXT("HudHint", "The show / hide HUD button (D-pad Up, H) hides the whole HUD and does not change these."), FLinearColor(0.7f, 0.7f, 0.7f, 1.f));
	AddButton(LOCTEXT("HudBack", "Back"));
}

void UHudOptionsWidget::HandleOptionChanged(int32 ButtonIndex, int32 Value)
{
	UPlayerSettingsSubsystem* Settings = UPlayerSettingsSubsystem::Get(GetOwningPlayer());
	if (Settings && ButtonIndex >= 0 && ButtonIndex < static_cast<int32>(EHudElement::Count))
	{
		Settings->SetHudElementEnabled(static_cast<EHudElement>(ButtonIndex), Value != 0);
	}
}

void UHudOptionsWidget::HandleButton(int32 ButtonIndex)
{
	// The only plain button is Back.
	GoBack();
}

// --- UControlsOptionsWidget

void UControlsOptionsWidget::BuildScreen()
{
	AddTitle(LOCTEXT("ControlsTitle", "CONTROLS"));

	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	const UDrivingInputConfig* Driving = Settings->DrivingInput.LoadSynchronous();
	const USystemInputConfig* System = Settings->SystemInput.LoadSynchronous();
	const UOnFootInputConfig* OnFoot = Settings->OnFootInput.LoadSynchronous();
	AddBindingGroup(LOCTEXT("ControlsDriving", "Driving"), Driving ? Driving->MappingContext.Get() : nullptr);
	AddBindingGroup(LOCTEXT("ControlsSystem", "System"), System ? System->MappingContext.Get() : nullptr);
	AddBindingGroup(LOCTEXT("ControlsOnFoot", "On foot (Adventure hub)"), OnFoot ? OnFoot->MappingContext.Get() : nullptr);

	AddLine(LOCTEXT("ControlsMenus", "Menus:   D-pad or stick to move,  A / Enter to accept,  B / Escape to go back"), ControlsNoteColour);
	AddLine(LOCTEXT("ControlsNote", "Changing the bindings comes later."), ControlsNoteColour);
	AddButton(LOCTEXT("ControlsBack", "Back"));
}

void UControlsOptionsWidget::AddBindingGroup(const FText& GroupName, const UInputMappingContext* MappingContext)
{
	if (!MappingContext)
	{
		return;
	}
	AddLine(GroupName, ControlsGroupColour);

	// The mapping context lists one entry per binding: gather them per action, in the order the actions first appear.
	TArray<const UInputAction*> Actions;
	TMap<const UInputAction*, FString> GamepadKeys;
	TMap<const UInputAction*, FString> KeyboardKeys;
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		const UInputAction* Action = Mapping.Action;
		if (!Action)
		{
			continue;
		}
		Actions.AddUnique(Action);
		FString& Keys = (Mapping.Key.IsGamepadKey() ? GamepadKeys : KeyboardKeys).FindOrAdd(Action);
		Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(", ")) + Mapping.Key.GetDisplayName().ToString();
	}

	for (const UInputAction* Action : Actions)
	{
		FString ActionName = Action->GetName();
		ActionName.RemoveFromStart(InputActionNamePrefix);
		const FString* Gamepad = GamepadKeys.Find(Action);
		const FString* Keyboard = KeyboardKeys.Find(Action);
		AddLine(FText::Format(LOCTEXT("ControlsLine", "{0}:   {1}   |   {2}"),
			FText::FromString(FName::NameToDisplayString(ActionName, /*bIsBool=*/false)),
			Gamepad ? FText::FromString(*Gamepad) : LOCTEXT("ControlsNone", "-"),
			Keyboard ? FText::FromString(*Keyboard) : LOCTEXT("ControlsNone", "-")));
	}
}

void UControlsOptionsWidget::HandleButton(int32 ButtonIndex)
{
	// The only button is Back.
	GoBack();
}

// --- UPauseMenuWidget

void UPauseMenuWidget::BuildScreen()
{
	AddTitle(LOCTEXT("PauseTitle", "PAUSED"));
	AddButton(LOCTEXT("PauseResume", "Resume"));
	AddButton(LOCTEXT("PauseOptions", "Options"));
	AddButton(LOCTEXT("PauseRestart", "Restart"));
	AddButton(LOCTEXT("PauseMainMenu", "Main menu"));
}

void UPauseMenuWidget::Resume()
{
	APlayerController* Player = GetOwningPlayer();
	HideScreen();
	if (Player)
	{
		Player->SetPause(false);
	}
	UE_LOG(LogCosmicRace, Log, TEXT("FLOW pause: resumed"));
}

bool UPauseMenuWidget::HandleBackOnRoot()
{
	Resume();
	return true;
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The pause button itself closes the menu again (the game does not see input while the menu is up).
	// Not on auto-repeat: the press that opened the menu is still held and would close it at once.
	if (InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right && !InKeyEvent.IsRepeat())
	{
		Resume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::HandleButton(int32 ButtonIndex)
{
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	switch (ButtonIndex)
	{
	case PauseResumeButton:
		Resume();
		break;

	case PauseOptionsButton:
		OpenChildScreen(CreateScreen<UOptionsWidget>());
		break;

	case PauseRestartButton:
		if (Flow)
		{
			Resume();
			Flow->StartRace(this);
		}
		break;

	case PauseMainMenuButton:
	{
		UMenuDialogWidget* Dialog = CreateScreen<UMenuDialogWidget>();
		Dialog->Setup(LOCTEXT("LeaveTitle", "MAIN MENU"), LOCTEXT("LeaveMessage", "Leave the race?"),
			{ LOCTEXT("LeaveCancel", "Cancel"), LOCTEXT("LeaveConfirm", "Leave race") }, LeaveDialogCancelAnswer);
		Dialog->OnChoice.AddUObject(this, &UPauseMenuWidget::HandleMainMenuChoice);
		OpenChildScreen(Dialog);
		break;
	}

	default:
		break;
	}
}

void UPauseMenuWidget::HandleMainMenuChoice(int32 AnswerIndex)
{
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	if (AnswerIndex == LeaveDialogLeaveAnswer && Flow)
	{
		Resume();
		Flow->OpenMainMenu(this);
	}
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
