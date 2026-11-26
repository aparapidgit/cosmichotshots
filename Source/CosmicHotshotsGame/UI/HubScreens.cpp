#include "UI/HubScreens.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Game/GameFlowSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "UI/AdventureScreens.h"
#include "UI/MenuScreens.h"

#define LOCTEXT_NAMESPACE "HubScreens"

namespace
{
	// Placeholder look. Sizes in Slate units.
	constexpr float HubHudMargin = 24.f;
	constexpr float HubPromptBottomMargin = 90.f;
	constexpr float HubMessageTopMargin = 110.f;
	constexpr int32 HubProgressFontSize = 18;
	constexpr int32 HubPromptFontSize = 26;
	constexpr int32 HubMessageFontSize = 34;

	const FLinearColor HubHudShadowColour(0.f, 0.f, 0.f, 0.85f);
	const FVector2D HubHudShadowOffset(1.5, 1.5);
	const FLinearColor HubMessageColour(1.f, 0.85f, 0.2f, 1.f);

	// Buttons of the hub pause menu, top to bottom.
	constexpr int32 HubPauseResumeButton = 0;
	constexpr int32 HubPauseOptionsButton = 1;
	constexpr int32 HubPauseLevelsButton = 2;
	constexpr int32 HubPauseMainMenuButton = 3;

	// Answers of the "leave the hub" confirmation. "Cancel" comes first and has the focus.
	constexpr int32 HubLeaveCancelAnswer = 0;
	constexpr int32 HubLeaveConfirmAnswer = 1;
}

// --- UHubHudWidget

UTextBlock* UHubHudWidget::MakeText(int32 FontSize, const FLinearColor& Colour)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = FontSize;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetShadowOffset(HubHudShadowOffset);
	Block->SetShadowColorAndOpacity(HubHudShadowColour);
	return Block;
}

TSharedRef<SWidget> UHubHudWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;

		ProgressText = MakeText(HubProgressFontSize, FLinearColor::White);
		UCanvasPanelSlot* ProgressSlot = Root->AddChildToCanvas(ProgressText);
		ProgressSlot->SetAnchors(FAnchors(0.f, 0.f));
		ProgressSlot->SetPosition(FVector2D(HubHudMargin, HubHudMargin));
		ProgressSlot->SetAutoSize(true);

		PromptText = MakeText(HubPromptFontSize, FLinearColor::White);
		UCanvasPanelSlot* PromptSlot = Root->AddChildToCanvas(PromptText);
		PromptSlot->SetAnchors(FAnchors(0.5f, 1.f));
		PromptSlot->SetAlignment(FVector2D(0.5, 1.0));
		PromptSlot->SetPosition(FVector2D(0.0, -HubPromptBottomMargin));
		PromptSlot->SetAutoSize(true);

		MessageText = MakeText(HubMessageFontSize, HubMessageColour);
		UCanvasPanelSlot* MessageSlot = Root->AddChildToCanvas(MessageText);
		MessageSlot->SetAnchors(FAnchors(0.5f, 0.f));
		MessageSlot->SetAlignment(FVector2D(0.5, 0.0));
		MessageSlot->SetPosition(FVector2D(0.0, HubMessageTopMargin));
		MessageSlot->SetAutoSize(true);

		// Display only: never in the way of the game or of a menu.
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UHubHudWidget::SetPrompt(const FText& Text)
{
	if (PromptText && !PromptText->GetText().EqualTo(Text))
	{
		PromptText->SetText(Text);
	}
}

void UHubHudWidget::SetProgress(const FText& Text)
{
	if (ProgressText && !ProgressText->GetText().EqualTo(Text))
	{
		ProgressText->SetText(Text);
	}
}

void UHubHudWidget::SetMessage(const FText& Text)
{
	if (MessageText)
	{
		MessageText->SetText(Text);
	}
}

// --- UHubPauseMenuWidget

void UHubPauseMenuWidget::BuildScreen()
{
	AddTitle(LOCTEXT("HubPauseTitle", "PAUSED"));
	AddButton(LOCTEXT("HubPauseResume", "Resume"));
	AddButton(LOCTEXT("HubPauseOptions", "Options"));
	AddButton(LOCTEXT("HubPauseLevels", "Level list"));
	AddButton(LOCTEXT("HubPauseMainMenu", "Main menu"));
}

void UHubPauseMenuWidget::Resume()
{
	APlayerController* Player = GetOwningPlayer();
	HideScreen();
	if (Player)
	{
		Player->SetPause(false);
	}
}

bool UHubPauseMenuWidget::HandleBackOnRoot()
{
	Resume();
	return true;
}

FReply UHubPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The pause button closes the menu again. Not on auto-repeat: the press that opened it is still held.
	if (InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right && !InKeyEvent.IsRepeat())
	{
		Resume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHubPauseMenuWidget::HandleButton(int32 ButtonIndex)
{
	switch (ButtonIndex)
	{
	case HubPauseResumeButton:
		Resume();
		break;

	case HubPauseOptionsButton:
		OpenChildScreen(CreateScreen<UOptionsWidget>());
		break;

	case HubPauseLevelsButton:
		OpenChildScreen(CreateScreen<UAdventureLevelListWidget>());
		break;

	case HubPauseMainMenuButton:
	{
		UMenuDialogWidget* Dialog = CreateScreen<UMenuDialogWidget>();
		Dialog->Setup(LOCTEXT("HubLeaveTitle", "MAIN MENU"), LOCTEXT("HubLeaveMessage", "Leave the adventure? Your progress is saved."),
			{ LOCTEXT("HubLeaveCancel", "Cancel"), LOCTEXT("HubLeaveConfirm", "Main menu") }, HubLeaveCancelAnswer);
		Dialog->OnChoice.AddUObject(this, &UHubPauseMenuWidget::HandleMainMenuChoice);
		OpenChildScreen(Dialog);
		break;
	}

	default:
		break;
	}
}

void UHubPauseMenuWidget::HandleMainMenuChoice(int32 AnswerIndex)
{
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	if (AnswerIndex == HubLeaveConfirmAnswer && Flow)
	{
		Resume();
		Flow->OpenMainMenu(this);
	}
}

#undef LOCTEXT_NAMESPACE
