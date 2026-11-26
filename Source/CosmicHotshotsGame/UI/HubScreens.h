// Adventure hub UI: the small on-foot HUD (progress, prompt, message) and the hub's pause menu.
// Plain UMG built in C++. The HUD only shows the texts the hub player controller hands it.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RaceScreens.h"
#include "HubScreens.generated.h"

class UTextBlock;

/** On-foot HUD: progress in a corner, the interact prompt at the bottom, a message near the top. */
UCLASS()
class COSMICHOTSHOTSGAME_API UHubHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** "Level 1-2: press A / E to race", or why the portal or gate is closed. Empty = nothing near. */
	void SetPrompt(const FText& Text);

	/** "Adventure: 3 / 12 levels won". */
	void SetProgress(const FText& Text);

	/** A short message (what the last race unlocked). Empty = none. */
	void SetMessage(const FText& Text);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UTextBlock* MakeText(int32 FontSize, const FLinearColor& Colour);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
};

/**
 * Cinematic placeholder: a full screen that says which cinematic belongs here. It ends when the player
 * skips it (any accept or back button) or by itself after its duration, and then calls OnFinished once.
 * A real video replaces this screen later; the caller (AHubPlayerController::PlayNextCinematic) is the one
 * place that decides between a video and the placeholder.
 */
UCLASS()
class COSMICHOTSHOTSGAME_API UCinematicPlaceholderWidget : public URaceScreenWidget
{
	GENERATED_BODY()

public:
	/** Call before the screen is shown. InName = what the cinematic is ("Intro", "Section 2", "Level 1-3"). */
	void Setup(const FText& InName, float InDuration);

	FSimpleMulticastDelegate OnFinished;

protected:
	virtual void BuildScreen() override;
	virtual void HandleButton(int32 ButtonIndex) override;
	virtual bool HandleBackOnRoot() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Finish();

	FText Name;
	float TimeRemaining = 0.f;
	bool bFinished = false;
};

/** Pause menu of the hub: Resume, Options, Level list (dev shortcut into any open level), Main menu. */
UCLASS()
class COSMICHOTSHOTSGAME_API UHubPauseMenuWidget : public URaceScreenWidget
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
