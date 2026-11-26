#include "UI/AdventureScreens.h"

#include "Adventure/AdventureDefinitions.h"
#include "Adventure/AdventureSubsystem.h"
#include "Components/TextBlock.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Game/GameFlowSubsystem.h"
#include "UI/MenuScreens.h"

#define LOCTEXT_NAMESPACE "AdventureScreens"

namespace
{
	// Buttons of the Adventure menu.
	constexpr int32 AdventureStartNewButton = 0;
	constexpr int32 AdventureLoadButton = 1;

	// Answers of the "replace this adventure?" confirmation. "Cancel" comes first and has the focus.
	constexpr int32 OverwriteCancelAnswer = 0;
	constexpr int32 OverwriteConfirmAnswer = 1;

	const FLinearColor AdventureDimColour(0.7f, 0.7f, 0.7f, 1.f);
	const FLinearColor AdventureWonColour(0.35f, 0.9f, 0.4f, 1.f);
	const FLinearColor AdventureLockedColour(0.5f, 0.5f, 0.5f, 1.f);
	const FLinearColor AdventureWarningColour(1.f, 0.6f, 0.2f, 1.f);
}

// --- UAdventureMenuWidget

void UAdventureMenuWidget::BuildScreen()
{
	AddTitle(LOCTEXT("AdventureTitle", "ADVENTURE MODE"));
	AddButton(LOCTEXT("AdventureStartNew", "Start new"));
	AddButton(LOCTEXT("AdventureLoad", "Load"));
	AddButton(LOCTEXT("AdventureBack", "Back"));
}

void UAdventureMenuWidget::HandleButton(int32 ButtonIndex)
{
	if (ButtonIndex == AdventureStartNewButton || ButtonIndex == AdventureLoadButton)
	{
		UAdventureSlotSelectWidget* Slots = CreateScreen<UAdventureSlotSelectWidget>();
		Slots->SetStartNew(ButtonIndex == AdventureStartNewButton);
		OpenChildScreen(Slots);
	}
	else
	{
		GoBack();
	}
}

// --- UAdventureSlotSelectWidget

void UAdventureSlotSelectWidget::BuildScreen()
{
	AddTitle(bStartNew ? LOCTEXT("SlotsNewTitle", "START NEW ADVENTURE") : LOCTEXT("SlotsLoadTitle", "LOAD ADVENTURE"));

	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;

	// Buttons 0 .. NumSaveSlots - 1 are the slots (RefreshSlots writes their labels); the next one is Back.
	for (int32 SlotIndex = 0; SlotIndex < UAdventureSubsystem::NumSaveSlots; ++SlotIndex)
	{
		AddButton(FText::GetEmpty());
	}
	AddButton(LOCTEXT("SlotsBack", "Back"));
	StatusText = AddLine(Campaign ? FText::GetEmpty() : LOCTEXT("NoCampaign", "No Adventure campaign in Project Settings > Game > Cosmic Hotshots."), AdventureWarningColour);
}

void UAdventureSlotSelectWidget::HandleScreenShown()
{
	RefreshSlots();
}

void UAdventureSlotSelectWidget::RefreshSlots()
{
	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	const int32 TotalLevels = Campaign ? Campaign->GetNumLevels() : 0;
	for (int32 SlotIndex = 0; SlotIndex < UAdventureSubsystem::NumSaveSlots; ++SlotIndex)
	{
		const bool bExists = Adventure && Adventure->DoesSlotExist(SlotIndex);
		SetButtonText(SlotIndex, bExists
			? FText::Format(LOCTEXT("SlotUsed", "Slot {0}:   {1} / {2} levels won"), FText::AsNumber(SlotIndex + 1), FText::AsNumber(Adventure->GetSlotLevelsWon(SlotIndex)), FText::AsNumber(TotalLevels))
			: FText::Format(LOCTEXT("SlotEmpty", "Slot {0}:   empty"), FText::AsNumber(SlotIndex + 1)),
			bExists ? FLinearColor::White : AdventureDimColour);
	}
}

void UAdventureSlotSelectWidget::HandleButton(int32 ButtonIndex)
{
	UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	if (ButtonIndex >= UAdventureSubsystem::NumSaveSlots || !Adventure)
	{
		GoBack();
		return;
	}
	if (!Adventure->GetCampaign())
	{
		return;
	}

	const bool bExists = Adventure->DoesSlotExist(ButtonIndex);
	if (!bStartNew)
	{
		if (bExists && Adventure->LoadGame(ButtonIndex))
		{
			OpenLevels();
		}
		else if (StatusText)
		{
			StatusText->SetText(LOCTEXT("SlotIsEmpty", "This slot is empty."));
		}
		return;
	}

	if (!bExists)
	{
		Adventure->StartNewGame(ButtonIndex);
		OpenLevels();
		return;
	}

	// Starting over throws the adventure in this slot away: ask first.
	PendingSlot = ButtonIndex;
	UMenuDialogWidget* Dialog = CreateScreen<UMenuDialogWidget>();
	Dialog->Setup(LOCTEXT("OverwriteTitle", "START NEW ADVENTURE"),
		FText::Format(LOCTEXT("OverwriteMessage", "Slot {0} already holds an adventure. Replace it?"), FText::AsNumber(ButtonIndex + 1)),
		{ LOCTEXT("OverwriteCancel", "Cancel"), LOCTEXT("OverwriteConfirm", "Replace and start new") }, OverwriteCancelAnswer);
	Dialog->OnChoice.AddUObject(this, &UAdventureSlotSelectWidget::HandleOverwriteChoice);
	OpenChildScreen(Dialog);
}

void UAdventureSlotSelectWidget::HandleOverwriteChoice(int32 AnswerIndex)
{
	UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	if (AnswerIndex == OverwriteConfirmAnswer && Adventure && PendingSlot != INDEX_NONE)
	{
		Adventure->StartNewGame(PendingSlot);
		OpenLevels();
	}
	PendingSlot = INDEX_NONE;
}

void UAdventureSlotSelectWidget::OpenLevels()
{
	// Into the hub. Without a hub map in the project settings, the level list stands in for it.
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	if (Flow && GetDefault<UCosmicHotshotsSettings>()->AdventureHubMap.IsValid())
	{
		HideScreen();
		Flow->OpenAdventureHub(this);
		return;
	}
	OpenChildScreen(CreateScreen<UAdventureLevelListWidget>());
}

// --- UAdventureLevelListWidget

void UAdventureLevelListWidget::BuildScreen()
{
	AddTitle(LOCTEXT("LevelsTitle", "ADVENTURE LEVELS"));
	AddLine(LOCTEXT("LevelsShortcut", "Shortcut: the same races as the portals in the hub."), AdventureDimColour);

	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	NumLevels = 0;
	SectionLines.Reset();
	int32 FirstOpenLevel = INDEX_NONE;
	for (int32 SectionIndex = 0; Campaign && SectionIndex < Campaign->Sections.Num(); ++SectionIndex)
	{
		// The texts come from RefreshLevels.
		SectionLines.Add(AddLine(FText::GetEmpty()));
		for (int32 Index = 0; Index < Campaign->Sections[SectionIndex].Levels.Num(); ++Index)
		{
			if (FirstOpenLevel == INDEX_NONE && Adventure->IsLevelUnlocked(NumLevels) && !Adventure->IsLevelWon(NumLevels))
			{
				FirstOpenLevel = NumLevels;
			}
			AddButton(FText::GetEmpty());
			++NumLevels;
		}
	}

	AddButton(LOCTEXT("LevelsBack", "Back"));
	StatusText = AddLine(FText::GetEmpty(), AdventureWarningColour);

	// The focus starts on the level to play next.
	InitialFocusButton = FirstOpenLevel != INDEX_NONE ? FirstOpenLevel : 0;
	RefreshLevels();
}

void UAdventureLevelListWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshLevels();
}

void UAdventureLevelListWidget::RefreshLevels()
{
	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	if (!Campaign)
	{
		return;
	}

	// Two bits per level (won, open) and one per section (gate open): rewrite the labels only when they change.
	uint64 State = 0;
	for (int32 LevelIndex = 0; LevelIndex < NumLevels; ++LevelIndex)
	{
		State = (State << 2) | (Adventure->IsLevelWon(LevelIndex) ? 2 : 0) | (Adventure->IsLevelUnlocked(LevelIndex) ? 1 : 0);
	}
	for (int32 SectionIndex = 0; SectionIndex < Campaign->Sections.Num(); ++SectionIndex)
	{
		State = (State << 1) | (Adventure->IsSectionOpen(SectionIndex) ? 1 : 0);
	}
	if (State == ShownState)
	{
		return;
	}
	ShownState = State;

	int32 LevelIndex = 0;
	for (int32 SectionIndex = 0; SectionIndex < Campaign->Sections.Num(); ++SectionIndex)
	{
		const FAdventureSection& Section = Campaign->Sections[SectionIndex];
		if (SectionLines.IsValidIndex(SectionIndex) && SectionLines[SectionIndex])
		{
			SectionLines[SectionIndex]->SetText(FText::Format(LOCTEXT("SectionLine", "{0}   ({1})"), Section.DisplayName,
				Adventure->IsSectionOpen(SectionIndex) ? LOCTEXT("GateOpen", "gate open") : LOCTEXT("GateClosed", "gate closed")));
		}
		for (const UAdventureLevelDefinition* Level : Section.Levels)
		{
			const bool bWon = Adventure->IsLevelWon(LevelIndex);
			const bool bUnlocked = Adventure->IsLevelUnlocked(LevelIndex);
			SetButtonText(LevelIndex, FText::Format(LOCTEXT("LevelButton", "{0}   {1}"), Level ? Level->DisplayName : LOCTEXT("MissingLevel", "(missing level)"),
				bWon ? LOCTEXT("LevelWon", "WON") : bUnlocked ? LOCTEXT("LevelOpen", "OPEN") : LOCTEXT("LevelLocked", "LOCKED")),
				bWon ? AdventureWonColour : bUnlocked ? FLinearColor::White : AdventureLockedColour);
			++LevelIndex;
		}
	}
	if (StatusText)
	{
		StatusText->SetText(FText::GetEmpty());
	}
}

void UAdventureLevelListWidget::HandleButton(int32 ButtonIndex)
{
	UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	if (ButtonIndex >= NumLevels || !Adventure)
	{
		GoBack();
		return;
	}

	if (!Adventure->IsLevelUnlocked(ButtonIndex))
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("LevelIsLocked", "Locked: win the level before it first."));
		}
		return;
	}

	HideScreen();
	Adventure->StartLevel(this, ButtonIndex);
}

#undef LOCTEXT_NAMESPACE
