#include "UI/RaceScreens.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterDefinition.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Game/GameFlowSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Passive/PassiveBase.h"
#include "Race/RaceGameState.h"
#include "Race/RaceTrackerComponent.h"
#include "Vehicle/RaceVehicle.h"

#define LOCTEXT_NAMESPACE "RaceScreens"

namespace
{
	// Placeholder look: grey and readable. Sizes in Slate units.
	constexpr int32 TitleFontSize = 34;
	constexpr int32 LineFontSize = 18;
	constexpr int32 ButtonFontSize = 22;
	constexpr int32 OverlayFontSize = 96;
	constexpr float ScreenLinePadding = 4.f;
	constexpr float ScreenButtonPadding = 6.f;
	constexpr float TitlePadding = 18.f;
	constexpr int32 ScreenZOrder = 10;
	constexpr int32 OverlayZOrder = 5;

	const FLinearColor ScreenBackColour(0.02f, 0.02f, 0.03f, 0.88f);
	const FLinearColor ButtonColour(0.25f, 0.25f, 0.28f, 1.f);
	const FLinearColor ButtonFocusColour(1.f, 0.8f, 0.15f, 1.f);
	const FLinearColor ScreenOwnRowColour(1.f, 0.85f, 0.2f, 1.f);
	const FLinearColor ScreenDimColour(0.7f, 0.7f, 0.7f, 1.f);

	/** How long "GO!" stays on screen after the start (s). */
	constexpr float GoDisplayTime = 1.f;

	// Default race until the race setup screen exists (Phase 2b).
	constexpr int32 DefaultOpponents = 7;

	// Buttons of the results screen.
	constexpr int32 RestartButton = 0;
	constexpr int32 ChangeCharacterButton = 1;

	UGameFlowSubsystem* GetFlow(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UGameFlowSubsystem>() : nullptr;
	}
}

FString CosmicHotshots::RaceUI::FormatRaceTime(float Seconds)
{
	const int32 TotalMilliseconds = FMath::Max(FMath::RoundToInt32(Seconds * 1000.f), 0);
	return FString::Printf(TEXT("%d:%02d.%03d"), TotalMilliseconds / 60000, (TotalMilliseconds / 1000) % 60, TotalMilliseconds % 1000);
}

void CosmicHotshots::RaceUI::StartRaceWithCharacter(const UObject* WorldContext, const UCharacterDefinition* Character)
{
	UGameFlowSubsystem* Flow = GetFlow(WorldContext);
	if (!Flow)
	{
		return;
	}

	FRaceSessionConfig Config;
	Config.HumanCharacters.Add(Character);
	Config.NumAIOpponents = DefaultOpponents;
	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();
	Config.Track = Settings->Tracks.IsEmpty() ? nullptr : Settings->Tracks[0].LoadSynchronous();
	Flow->SetRaceConfig(Config);
	Flow->StartRace(WorldContext);
}

// --- URaceMenuButton

void URaceMenuButton::Setup(int32 InIndex)
{
	Index = InIndex;
	OnClicked.AddUniqueDynamic(this, &URaceMenuButton::HandleClicked);
}

void URaceMenuButton::HandleClicked()
{
	OnMenuClicked.Broadcast(Index);
}

// --- URaceScreenWidget

TSharedRef<SWidget> URaceScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(ScreenBackColour);
		Background->SetHorizontalAlignment(HAlign_Center);
		Background->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Background;

		Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Background->SetContent(Column);

		BuildScreen();
	}
	return Super::RebuildWidget();
}

UTextBlock* URaceScreenWidget::AddText(const FText& Text, int32 FontSize, const FLinearColor& Colour)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = FontSize;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Colour));
	return Block;
}

UTextBlock* URaceScreenWidget::AddTitle(const FText& Text)
{
	UTextBlock* Block = AddText(Text, TitleFontSize, FLinearColor::White);
	UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Block);
	BoxSlot->SetHorizontalAlignment(HAlign_Center);
	BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, TitlePadding));
	return Block;
}

UTextBlock* URaceScreenWidget::AddLine(const FText& Text, const FLinearColor& Colour)
{
	UTextBlock* Block = AddText(Text, LineFontSize, Colour);
	UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Block);
	BoxSlot->SetHorizontalAlignment(HAlign_Center);
	BoxSlot->SetPadding(FMargin(0.f, ScreenLinePadding));
	return Block;
}

URaceMenuButton* URaceScreenWidget::AddButton(const FText& Text, const FLinearColor& TextColour)
{
	URaceMenuButton* Button = WidgetTree->ConstructWidget<URaceMenuButton>();
	Button->Setup(Buttons.Num());
	Button->SetBackgroundColor(ButtonColour);
	Button->AddChild(AddText(Text, ButtonFontSize, TextColour));
	Button->OnMenuClicked.AddUObject(this, &URaceScreenWidget::HandleButton);

	UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Button);
	BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	BoxSlot->SetPadding(FMargin(0.f, ScreenButtonPadding));
	Buttons.Add(Button);
	return Button;
}

void URaceScreenWidget::ShowScreen()
{
	AddToViewport(ScreenZOrder);

	APlayerController* Player = GetOwningPlayer();
	if (!Player)
	{
		return;
	}

	// Menu input: the focus sits on a button, so a gamepad works without a mouse.
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (!Buttons.IsEmpty())
	{
		InputMode.SetWidgetToFocus(Buttons[0]->TakeWidget());
	}
	Player->SetInputMode(InputMode);
	Player->SetShowMouseCursor(true);
	if (!Buttons.IsEmpty())
	{
		Buttons[0]->SetKeyboardFocus();
	}
}

void URaceScreenWidget::HideScreen()
{
	RemoveFromParent();
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->SetInputMode(FInputModeGameOnly());
		Player->SetShowMouseCursor(false);
	}
}

void URaceScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Clear focus highlight: the focused (or hovered) button is drawn in the highlight colour.
	int32 NewFocus = INDEX_NONE;
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		const bool bFocused = Buttons[Index]->HasKeyboardFocus() || Buttons[Index]->IsHovered();
		Buttons[Index]->SetBackgroundColor(bFocused ? ButtonFocusColour : ButtonColour);
		if (bFocused && NewFocus == INDEX_NONE)
		{
			NewFocus = Index;
		}
	}
	if (NewFocus != FocusedButton)
	{
		FocusedButton = NewFocus;
		HandleFocusChanged(FocusedButton);
	}
}

// --- UCharacterSelectWidget

void UCharacterSelectWidget::BuildScreen()
{
	AddTitle(LOCTEXT("SelectTitle", "SELECT CHARACTER"));

	Characters.Reset();
	for (const UCharacterDefinition* Character : GetDefault<UCosmicHotshotsSettings>()->GetRaceCharacters())
	{
		Characters.Add(Character);
		AddButton(Character->DisplayName, Character->Colour);
	}

	DescriptionText = AddLine(FText::GetEmpty(), ScreenDimColour);
}

void UCharacterSelectWidget::HandleFocusChanged(int32 ButtonIndex)
{
	if (!DescriptionText)
	{
		return;
	}
	const UPassiveDefinition* Passive = Characters.IsValidIndex(ButtonIndex) ? Characters[ButtonIndex]->PassiveDefinition.Get() : nullptr;
	DescriptionText->SetText(Passive
		? FText::Format(LOCTEXT("PassiveLine", "{0}: {1}"), Passive->DisplayName, Passive->Description)
		: FText::GetEmpty());
}

void UCharacterSelectWidget::HandleButton(int32 ButtonIndex)
{
	if (Characters.IsValidIndex(ButtonIndex))
	{
		HideScreen();
		CosmicHotshots::RaceUI::StartRaceWithCharacter(this, Characters[ButtonIndex]);
	}
}

// --- URaceResultsWidget

void URaceResultsWidget::SetResults(const FRaceResultData& InResults)
{
	Results = InResults;
}

void URaceResultsWidget::BuildScreen()
{
	AddTitle(LOCTEXT("ResultsTitle", "RESULTS"));

	for (const FRaceResultEntry& Entry : Results.Entries)
	{
		const FText Name = Entry.Character ? Entry.Character->DisplayName : FText::FromString(GetNameSafe(Entry.Vehicle));
		const FString TimeText = Entry.bFinished
			? FString::Printf(TEXT("%s   best lap %s"), *CosmicHotshots::RaceUI::FormatRaceTime(Entry.TotalTime), *CosmicHotshots::RaceUI::FormatRaceTime(Entry.BestLapTime))
			: FString(TEXT("did not finish"));
		AddLine(FText::FromString(FString::Printf(TEXT("%d.  %s%s   %s"), Entry.Place, *Name.ToString(), Entry.bHuman ? TEXT("  (YOU)") : TEXT(""), *TimeText)),
			Entry.bHuman ? ScreenOwnRowColour : FLinearColor::White);
	}

	AddButton(LOCTEXT("Restart", "Restart"));
	AddButton(LOCTEXT("ChangeCharacter", "Change character"));
}

void URaceResultsWidget::HandleButton(int32 ButtonIndex)
{
	UGameFlowSubsystem* Flow = GetFlow(this);
	if (!Flow)
	{
		return;
	}

	HideScreen();
	if (ButtonIndex == ChangeCharacterButton)
	{
		// Without a race setup the track opens on the character select again.
		Flow->ClearRaceConfig();
	}
	Flow->StartRace(this);
}

// --- URaceOverlayWidget

TSharedRef<SWidget> URaceOverlayWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
		Root->SetBrushColor(FLinearColor::Transparent);
		Root->SetHorizontalAlignment(HAlign_Center);
		Root->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Root;

		BigText = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = BigText->GetFont();
		Font.Size = OverlayFontSize;
		BigText->SetFont(Font);
		BigText->SetJustification(ETextJustify::Center);
		Root->SetContent(BigText);

		// Never in the way of the game or of a menu.
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void URaceOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
	if (!BigText || !RaceState)
	{
		return;
	}

	FText Text;
	const float Countdown = RaceState->GetTracker()->GetCountdownRemaining();
	if (Countdown > 0.f)
	{
		bCountdownSeen = true;
		Text = FText::AsNumber(FMath::CeilToInt32(Countdown));
	}
	else if (Countdown == 0.f && bCountdownSeen && GoTime < GoDisplayTime)
	{
		GoTime = FMath::Max(GoTime, 0.f) + InDeltaTime;
		Text = LOCTEXT("Go", "GO!");
	}
	else if (RaceState->GetRacePhase() == ERacePhase::Racing)
	{
		// Finished, while the others are still driving.
		const FRacerStanding* Standing = RaceState->FindStanding(Cast<ARaceVehicle>(GetOwningPlayerPawn()));
		if (Standing && Standing->bFinished)
		{
			Text = FText::Format(LOCTEXT("Finish", "FINISH\nPlace {0}"), FText::AsNumber(Standing->Rank));
		}
	}
	BigText->SetText(Text);
}

#undef LOCTEXT_NAMESPACE
