#include "UI/RaceHudWidget.h"

#include "Ability/VehicleAbilityComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterDefinition.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemSystemComponent.h"
#include "Passive/PassiveBase.h"
#include "Passive/VehiclePassiveComponent.h"
#include "Race/RaceGameState.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierComponent.h"
#include "Vehicle/VehicleStatsComponent.h"
#include "Vehicle/VehicleTuningData.h"

#define LOCTEXT_NAMESPACE "RaceHud"

namespace
{
	// Temporary look. Sizes in Slate units.
	constexpr float HudMargin = 24.f;
	constexpr int32 HudStandingFontSize = 16;
	constexpr int32 HudPlaceFontSize = 54;
	constexpr int32 HudLapFontSize = 22;
	constexpr int32 HudInfoFontSize = 16;
	constexpr float HudLineGap = 2.f;

	// Energy Core meter: bottom centre, above the edge.
	constexpr float HudMeterWidth = 420.f;
	constexpr float HudMeterHeight = 22.f;
	constexpr float HudMeterBottomMargin = 40.f;
	constexpr float HudMeterSegmentGap = 3.f;

	const FLinearColor HudShadowColour(0.f, 0.f, 0.f, 0.85f);
	const FVector2D HudShadowOffset(1.5, 1.5);
	const FLinearColor HudOwnColour(1.f, 0.85f, 0.2f, 1.f);

	// A glowing name (ability running or passive active): outline in the character's colour (px), text pulsing
	// towards white by this share, this fast (radians per second).
	constexpr int32 HudGlowOutlineSize = 2;
	constexpr float HudGlowWhiteShare = 0.7f;
	constexpr float HudGlowPulseRate = 9.f;
	/** A used ability is drawn in this grey and comes back to the character's colour over its cooldown. */
	const FLinearColor HudAbilityUsedColour(0.4f, 0.4f, 0.4f, 1.f);
	/** An ability that is ready but whose condition is not met keeps this share of the character's colour. */
	constexpr float HudAbilityBlockedColourShare = 0.5f;
	// A passive's modifiers are named "Passive.<Character>"; the character assets are "DA_Character_<Character>".
	const FString HudPassiveSourcePrefix(TEXT("Passive."));
	const TCHAR* HudCharacterAssetPrefix = TEXT("DA_Character_");
	const FLinearColor HudDimColour(0.75f, 0.75f, 0.75f, 1.f);

	// Energy Core colours: a full segment can pay for the active ability, a part cannot; all orange while boosting.
	const FLinearColor HudSegmentFullColour(1.f, 0.8f, 0.15f, 1.f);
	const FLinearColor HudSegmentPartColour(0.25f, 0.65f, 1.f, 1.f);
	const FLinearColor HudSegmentBoostColour(1.f, 0.35f, 0.1f, 1.f);

	constexpr float HudPercentScale = 100.f;

	/** 1 -> "1st", 2 -> "2nd", 11 -> "11th". */
	FText MakePlaceText(int32 Place)
	{
		const int32 LastTwo = Place % 100;
		const int32 Last = Place % 10;
		const TCHAR* Suffix = (LastTwo >= 11 && LastTwo <= 13) ? TEXT("th") : Last == 1 ? TEXT("st") : Last == 2 ? TEXT("nd") : Last == 3 ? TEXT("rd") : TEXT("th");
		return FText::FromString(FString::Printf(TEXT("%d%s"), Place, Suffix));
	}
}

UTextBlock* URaceHudWidget::MakeText(int32 FontSize)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = FontSize;
	Block->SetFont(Font);
	// A shadow keeps the text readable over a bright track.
	Block->SetShadowOffset(HudShadowOffset);
	Block->SetShadowColorAndOpacity(HudShadowColour);
	return Block;
}

void URaceHudWidget::RegisterElement(EHudElement Element, UWidget* Widget)
{
	ElementWidgets[static_cast<int32>(Element)] = Widget;
}

TSharedRef<SWidget> URaceHudWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		ElementWidgets.Init(nullptr, static_cast<int32>(EHudElement::Count));

		// --- standings: top left
		UVerticalBox* StandingsBox = WidgetTree->ConstructWidget<UVerticalBox>();
		for (int32 Index = 0; Index < UCosmicHotshotsSettings::MaxGridSize; ++Index)
		{
			UTextBlock* Line = MakeText(HudStandingFontSize);
			StandingsBox->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, HudLineGap));
			StandingLines.Add(Line);
		}
		UCanvasPanelSlot* StandingsSlot = Root->AddChildToCanvas(StandingsBox);
		StandingsSlot->SetAnchors(FAnchors(0.f, 0.f));
		StandingsSlot->SetPosition(FVector2D(HudMargin, HudMargin));
		StandingsSlot->SetAutoSize(true);
		RegisterElement(EHudElement::Standings, StandingsBox);

		// --- own place and lap: top right
		UVerticalBox* PlaceBox = WidgetTree->ConstructWidget<UVerticalBox>();
		PlaceText = MakeText(HudPlaceFontSize);
		PlaceText->SetColorAndOpacity(FSlateColor(HudOwnColour));
		PlaceBox->AddChildToVerticalBox(PlaceText)->SetHorizontalAlignment(HAlign_Right);
		LapText = MakeText(HudLapFontSize);
		PlaceBox->AddChildToVerticalBox(LapText)->SetHorizontalAlignment(HAlign_Right);
		UCanvasPanelSlot* PlaceSlot = Root->AddChildToCanvas(PlaceBox);
		PlaceSlot->SetAnchors(FAnchors(1.f, 0.f));
		PlaceSlot->SetAlignment(FVector2D(1.0, 0.0));
		PlaceSlot->SetPosition(FVector2D(-HudMargin, HudMargin));
		PlaceSlot->SetAutoSize(true);
		RegisterElement(EHudElement::PlaceAndLap, PlaceBox);

		// --- Energy Core meter: bottom centre (segments are added when the vehicle's tuning is known)
		EnergyCoreBox = WidgetTree->ConstructWidget<UHorizontalBox>();
		UCanvasPanelSlot* MeterSlot = Root->AddChildToCanvas(EnergyCoreBox);
		MeterSlot->SetAnchors(FAnchors(0.5f, 1.f));
		MeterSlot->SetAlignment(FVector2D(0.5, 1.0));
		MeterSlot->SetPosition(FVector2D(0.0, -HudMeterBottomMargin));
		MeterSlot->SetSize(FVector2D(HudMeterWidth, HudMeterHeight));
		RegisterElement(EHudElement::EnergyCore, EnergyCoreBox);

		// --- item slot: bottom left
		ItemSlotText = MakeText(HudLapFontSize);
		UCanvasPanelSlot* ItemSlot = Root->AddChildToCanvas(ItemSlotText);
		ItemSlot->SetAnchors(FAnchors(0.f, 1.f));
		ItemSlot->SetAlignment(FVector2D(0.0, 1.0));
		ItemSlot->SetPosition(FVector2D(HudMargin, -HudMargin));
		ItemSlot->SetAutoSize(true);
		RegisterElement(EHudElement::ItemSlot, ItemSlotText);

		// --- passive indicator and active effects: bottom right, two separate elements in one column
		UVerticalBox* StatusBox = WidgetTree->ConstructWidget<UVerticalBox>();
		EffectsText = MakeText(HudInfoFontSize);
		EffectsText->SetJustification(ETextJustify::Right);
		StatusBox->AddChildToVerticalBox(EffectsText)->SetHorizontalAlignment(HAlign_Right);
		PassiveText = MakeText(HudInfoFontSize);
		PassiveText->SetColorAndOpacity(FSlateColor(HudOwnColour));
		StatusBox->AddChildToVerticalBox(PassiveText)->SetHorizontalAlignment(HAlign_Right);
		AbilityText = MakeText(HudInfoFontSize);
		StatusBox->AddChildToVerticalBox(AbilityText)->SetHorizontalAlignment(HAlign_Right);
		UCanvasPanelSlot* StatusSlot = Root->AddChildToCanvas(StatusBox);
		StatusSlot->SetAnchors(FAnchors(1.f, 1.f));
		StatusSlot->SetAlignment(FVector2D(1.0, 1.0));
		StatusSlot->SetPosition(FVector2D(-HudMargin, -HudMargin));
		StatusSlot->SetAutoSize(true);
		RegisterElement(EHudElement::ActiveEffects, EffectsText);
		RegisterElement(EHudElement::PassiveIndicator, PassiveText);

		// Display only: never in the way of the game or of a menu.
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void URaceHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* Player = GetOwningPlayer();
	const ARaceVehicle* OwnVehicle = Player ? Cast<ARaceVehicle>(Player->GetPawn()) : nullptr;
	const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
	UPlayerSettingsSubsystem* Settings = UPlayerSettingsSubsystem::Get(Player);

	// Nothing to show before the race has drivers, and the results screen takes over when it is finished.
	const bool bRaceRunning = Root && OwnVehicle && RaceState && Settings
		&& RaceState->GetRacePhase() == ERacePhase::Racing && RaceState->FindStanding(OwnVehicle) != nullptr;
	if (Root)
	{
		Root->SetVisibility(bRaceRunning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bRaceRunning)
	{
		return;
	}

	// The visibility rule, in one place: shown with the button AND switched on in Options.
	for (int32 Index = 0; Index < ElementWidgets.Num(); ++Index)
	{
		if (UWidget* Widget = ElementWidgets[Index])
		{
			Widget->SetVisibility(Settings->IsHudElementDrawn(static_cast<EHudElement>(Index)) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	UpdateStandings(*RaceState, OwnVehicle, Settings->IsHudElementEnabled(EHudElement::StandingsItemMarker));
	UpdatePlaceAndLap(*RaceState, *OwnVehicle);
	UpdateEnergyCore(*OwnVehicle);
	UpdateItemSlot(*RaceState, *OwnVehicle);
	UpdatePassiveAndEffects(*OwnVehicle);
	HudTime += InDeltaTime;
	UpdateAbility(*OwnVehicle);
}

bool URaceHudWidget::IsCharacterPowerActive(const ARaceVehicle& Vehicle)
{
	if (Vehicle.GetAbility() && Vehicle.GetAbility()->GetHudState().ActiveRemaining > 0.f)
	{
		return true;
	}
	// The passive: a modifier on the car's own stack that its own passive put there ("Passive.<Character>").
	const UCharacterDefinition* Character = Vehicle.GetCharacter();
	if (!Character || !Vehicle.GetModifiers())
	{
		return false;
	}
	const FName PassiveSource(*(HudPassiveSourcePrefix + Character->GetName().Replace(HudCharacterAssetPrefix, TEXT(""))));
	return Vehicle.GetModifiers()->GetStack().HasSource(PassiveSource);
}

void URaceHudWidget::UpdateAbility(const ARaceVehicle& OwnVehicle)
{
	const FAbilityHudState Ability = OwnVehicle.GetAbility() ? OwnVehicle.GetAbility()->GetHudState() : FAbilityHudState();
	// Shown and hidden together with the passive indicator (one Options toggle).
	AbilityText->SetVisibility(Ability.bHasAbility ? PassiveText->GetVisibility() : ESlateVisibility::Collapsed);
	if (!Ability.bHasAbility)
	{
		return;
	}

	const UCharacterDefinition* Character = OwnVehicle.GetCharacter();
	const FLinearColor Colour = Character ? Character->Colour : HudOwnColour;
	const FString Name = Ability.DisplayName.ToString();
	FString Text;
	FLinearColor Shown = Colour;
	if (Ability.ActiveRemaining > 0.f)
	{
		// Running: bright, pulsing.
		Text = FString::Printf(TEXT("%s  ACTIVE"), *Name);
		Shown = FMath::Lerp(Colour, FLinearColor::White, HudGlowWhiteShare * (0.5f + 0.5f * FMath::Sin(HudTime * HudGlowPulseRate)));
	}
	else if (Ability.CooldownRemaining > 0.f)
	{
		// Used: grey, coming back to the character's colour as the cooldown runs out.
		const float Back = Ability.CooldownTotal > UE_SMALL_NUMBER ? 1.f - Ability.CooldownRemaining / Ability.CooldownTotal : 1.f;
		Text = FString::Printf(TEXT("%s  %.1f s"), *Name, Ability.CooldownRemaining);
		Shown = FMath::Lerp(HudAbilityUsedColour, Colour, FMath::Clamp(Back, 0.f, 1.f));
	}
	else if (!Ability.bEnoughEnergy)
	{
		Text = FString::Printf(TEXT("%s  (needs 1 segment)"), *Name);
		Shown = HudAbilityUsedColour;
	}
	else if (!Ability.bConditionMet)
	{
		Text = FString::Printf(TEXT("%s  (not now)"), *Name);
		Shown = FMath::Lerp(HudAbilityUsedColour, Colour, HudAbilityBlockedColourShare);
	}
	else
	{
		Text = FString::Printf(TEXT("%s  READY"), *Name);
	}
	AbilityText->SetText(FText::FromString(Text));
	AbilityText->SetColorAndOpacity(FSlateColor(Shown));
}

void URaceHudWidget::UpdateStandings(const ARaceGameState& RaceState, const ARaceVehicle* OwnVehicle, bool bShowItemMarker)
{
	const TArray<FRacerStanding>& Standings = RaceState.GetStandings();
	for (int32 Index = 0; Index < StandingLines.Num(); ++Index)
	{
		UTextBlock* Line = StandingLines[Index];
		if (!Standings.IsValidIndex(Index) || !Standings[Index].Vehicle)
		{
			Line->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FRacerStanding& Standing = Standings[Index];
		const UCharacterDefinition* Character = Standing.Vehicle->GetCharacter();
		const bool bOwn = Standing.Vehicle == OwnVehicle;
		const FString Name = Character ? Character->DisplayName.ToString() : GetNameSafe(Standing.Vehicle);
		const bool bHoldsItem = bShowItemMarker && RaceState.GetItemSystem()->GetSlot(Standing.Vehicle).State != EItemSlotState::Empty;

		// "> " marks the local player; "[item]" is the generic "holds an item" mark; a finished driver is ticked off.
		Line->SetText(FText::FromString(FString::Printf(TEXT("%s%d  %s%s%s"), bOwn ? TEXT("> ") : TEXT("   "), Standing.Rank, *Name,
			bHoldsItem ? TEXT("  [item]") : TEXT(""), Standing.bFinished ? TEXT("  (finished)") : TEXT(""))));
		// While the character's ability runs or its passive is giving the car something, the name glows in
		// the character's colour: bright pulsing text with an outline of that colour.
		const FLinearColor Colour = Character ? Character->Colour : FLinearColor::White;
		const bool bGlow = IsCharacterPowerActive(*Standing.Vehicle);
		if (!StandingGlow.IsValidIndex(Index))
		{
			StandingGlow.SetNumZeroed(StandingLines.Num());
		}
		if (StandingGlow[Index] != bGlow)
		{
			StandingGlow[Index] = bGlow;
			FSlateFontInfo Font = Line->GetFont();
			Font.OutlineSettings.OutlineSize = bGlow ? HudGlowOutlineSize : 0;
			Line->SetFont(Font);
		}
		if (bGlow)
		{
			FSlateFontInfo Font = Line->GetFont();
			Font.OutlineSettings.OutlineColor = Colour;
			Line->SetFont(Font);
		}
		const float Pulse = 0.5f + 0.5f * FMath::Sin(HudTime * HudGlowPulseRate);
		Line->SetColorAndOpacity(FSlateColor(bGlow ? FMath::Lerp(Colour, FLinearColor::White, HudGlowWhiteShare * Pulse) : Colour));
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void URaceHudWidget::UpdatePlaceAndLap(const ARaceGameState& RaceState, const ARaceVehicle& OwnVehicle)
{
	const FRacerStanding* Standing = RaceState.FindStanding(&OwnVehicle);
	if (!Standing)
	{
		return;
	}
	PlaceText->SetText(MakePlaceText(Standing->Rank));
	LapText->SetText(FText::Format(LOCTEXT("Lap", "Lap {0}/{1}"), FText::AsNumber(Standing->CurrentLap), FText::AsNumber(RaceState.GetNumLaps())));
}

void URaceHudWidget::UpdateEnergyCore(const ARaceVehicle& OwnVehicle)
{
	const UVehicleTuningData* Tuning = OwnVehicle.GetStats()->GetTuning();
	if (!Tuning)
	{
		return;
	}

	// One bar per segment of the reservoir; rebuilt only when the segment count changes (another character's tuning).
	const int32 NumSegments = FMath::Max(Tuning->BoostSegments, 1);
	if (EnergyCoreSegments.Num() != NumSegments)
	{
		EnergyCoreBox->ClearChildren();
		EnergyCoreSegments.Reset();
		for (int32 Index = 0; Index < NumSegments; ++Index)
		{
			UProgressBar* Segment = WidgetTree->ConstructWidget<UProgressBar>();
			UHorizontalBoxSlot* SegmentSlot = EnergyCoreBox->AddChildToHorizontalBox(Segment);
			SegmentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SegmentSlot->SetPadding(FMargin(HudMeterSegmentGap, 0.f));
			EnergyCoreSegments.Add(Segment);
		}
	}

	const FDriftBoostState& DriftBoost = OwnVehicle.GetHoverMovement()->GetSimState().DriftBoost;
	const float Charge = FMath::Clamp(DriftBoost.BoostCharge / FDriftBoostState::MaxBoostCharge, 0.f, 1.f);
	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		// How full this segment is: the meter fills segment by segment from the left.
		const float Fill = FMath::Clamp(Charge * NumSegments - Index, 0.f, 1.f);
		EnergyCoreSegments[Index]->SetPercent(Fill);
		EnergyCoreSegments[Index]->SetFillColorAndOpacity(DriftBoost.IsBoosting() ? HudSegmentBoostColour
			: Fill >= 1.f - UE_KINDA_SMALL_NUMBER ? HudSegmentFullColour : HudSegmentPartColour);
	}
}

void URaceHudWidget::UpdateItemSlot(const ARaceGameState& RaceState, const ARaceVehicle& OwnVehicle)
{
	const FItemSlot HeldSlot = RaceState.GetItemSystem()->GetSlot(&OwnVehicle);
	switch (HeldSlot.State)
	{
	case EItemSlotState::Ready:
		ItemSlotText->SetText(FText::Format(LOCTEXT("ItemReady", "ITEM  [ {0} ]"), HeldSlot.Item ? HeldSlot.Item->DisplayName : LOCTEXT("ItemUnknown", "?")));
		ItemSlotText->SetColorAndOpacity(FSlateColor(HudOwnColour));
		break;

	case EItemSlotState::Rolling:
		ItemSlotText->SetText(LOCTEXT("ItemRolling", "ITEM  [ ... ]"));
		ItemSlotText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		break;

	default:
		// The slot is visibly empty.
		ItemSlotText->SetText(LOCTEXT("ItemEmpty", "ITEM  [   ]"));
		ItemSlotText->SetColorAndOpacity(FSlateColor(HudDimColour));
		break;
	}
}

void URaceHudWidget::UpdatePassiveAndEffects(const ARaceVehicle& OwnVehicle)
{
	// Passive: the passive's own one-line state (on / off, level, time left).
	const UVehiclePassiveComponent* PassiveComponent = OwnVehicle.GetPassive();
	const UPassiveBase* Passive = PassiveComponent ? PassiveComponent->GetPassive() : nullptr;
	PassiveText->SetText(!Passive ? FText::GetEmpty()
		: FText::FromString(PassiveComponent->IsPassiveRunning() ? Passive->GetDebugString() : FString(TEXT("Passive: off"))));

	// Active effects: every modifier on the car's stack with its time left, and the boost while it runs.
	FString Effects;
	if (OwnVehicle.GetHoverMovement()->GetSimState().DriftBoost.IsBoosting())
	{
		Effects += TEXT("BOOST\n");
	}
	for (const FVehicleModifier& Modifier : OwnVehicle.GetModifiers()->GetStack().GetModifiers())
	{
		const float TimeRemaining = Modifier.GetTimeRemaining();
		const FString TimeText = Modifier.bEnding ? FString(TEXT("ending")) : TimeRemaining < 0.f ? FString() : FString::Printf(TEXT("%.1f s"), TimeRemaining);
		// A multiplier reads as a percentage change, an added value as it is.
		const FString ValueText = Modifier.Spec.Op == EVehicleModifierOp::Multiply
			? FString::Printf(TEXT("%+.0f%%"), (Modifier.Spec.Value - 1.f) * Modifier.Strength * HudPercentScale)
			: FString::Printf(TEXT("%+.2f"), Modifier.Spec.Value * Modifier.Strength);
		Effects += FString::Printf(TEXT("%s %s  %s\n"), *UEnum::GetDisplayValueAsText(Modifier.Spec.Stat).ToString(), *ValueText, *TimeText);
	}
	EffectsText->SetText(FText::FromString(Effects.TrimEnd()));
}

#undef LOCTEXT_NAMESPACE
