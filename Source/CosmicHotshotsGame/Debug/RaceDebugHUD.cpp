#include "Debug/RaceDebugHUD.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemSystemComponent.h"
#include "Passive/PassiveBase.h"
#include "Passive/VehiclePassiveComponent.h"
#include "Race/RaceDebug.h"
#include "Race/RaceGameState.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierComponent.h"
#include "Vehicle/VehicleStatsComponent.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBoostBar(
		TEXT("ch.Debug.BoostBar"), 1,
		TEXT("1 = draw the dev boost meter bar at the bottom of the screen."),
		ECVF_Cheat);

	// Layout as fractions of the viewport, so the bar fits any resolution and split-screen viewport.
	constexpr float BarWidthFraction = 0.30f;
	constexpr float BarHeightFraction = 0.022f;
	constexpr float BarBottomMarginFraction = 0.06f;
	constexpr float MinBarHeight = 6.f;
	constexpr float BarBorder = 2.f;
	constexpr float TickWidth = 2.f;
	constexpr float LabelGap = 4.f;

	const FLinearColor BackColour(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor PartialColour(0.25f, 0.65f, 1.f, 0.95f);
	const FLinearColor FullSegmentColour(1.f, 0.8f, 0.15f, 0.95f);
	const FLinearColor BoostingColour(1.f, 0.35f, 0.1f, 0.95f);
	const FLinearColor TickColour(1.f, 1.f, 1.f, 0.8f);

	// Race standings table (top left).
	constexpr float TableLeftFraction = 0.02f;
	constexpr float TableTopFraction = 0.08f;
	constexpr float TableWidthFraction = 0.45f;
	constexpr float TableLineHeight = 16.f;
	constexpr float TablePadding = 6.f;
	constexpr float CmToMeters = 0.01f;
	const FLinearColor OwnRowColour(1.f, 0.85f, 0.2f, 1.f);

	// Passive and modifier list (top right).
	constexpr float PassivesLeftFraction = 0.62f;
	constexpr float PassivesWidthFraction = 0.36f;
	constexpr float PercentScale = 100.f;
}

void ARaceDebugHUD::DrawHUD()
{
	Super::DrawHUD();

#if !UE_BUILD_SHIPPING
	DrawRaceStandings();
	DrawPassives();

	if (CVarBoostBar.GetValueOnGameThread() == 0 || !Canvas)
	{
		return;
	}

	const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetOwningPawn());
	const UVehicleTuningData* Tuning = Vehicle ? Vehicle->GetStats()->GetTuning() : nullptr;
	if (!Tuning)
	{
		return;
	}

	const FDriftBoostState& DriftBoost = Vehicle->GetHoverMovement()->GetSimState().DriftBoost;
	const float Charge = FMath::Clamp(DriftBoost.BoostCharge / FDriftBoostState::MaxBoostCharge, 0.f, 1.f);
	const int32 Segments = FMath::Max(Tuning->BoostSegments, 1);
	const float SegmentSize = 1.f / Segments;
	const int32 FullSegments = FMath::FloorToInt32(Charge / SegmentSize + UE_KINDA_SMALL_NUMBER);

	const float Width = Canvas->ClipX * BarWidthFraction;
	const float Height = FMath::Max(Canvas->ClipY * BarHeightFraction, MinBarHeight);
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = Canvas->ClipY * (1.f - BarBottomMarginFraction) - Height;

	DrawRect(BackColour, Left - BarBorder, Top - BarBorder, Width + 2.f * BarBorder, Height + 2.f * BarBorder);

	// Whole segments (each can pay for one active ability) in yellow, the part of the next one in blue.
	// The whole bar turns orange while the boost is draining it.
	const float FullWidth = Width * FullSegments * SegmentSize;
	const float PartialWidth = Width * Charge - FullWidth;
	if (DriftBoost.IsBoosting())
	{
		DrawRect(BoostingColour, Left, Top, Width * Charge, Height);
	}
	else
	{
		DrawRect(FullSegmentColour, Left, Top, FullWidth, Height);
		DrawRect(PartialColour, Left + FullWidth, Top, FMath::Max(PartialWidth, 0.f), Height);
	}

	// Segment borders.
	for (int32 Index = 1; Index < Segments; ++Index)
	{
		DrawRect(TickColour, Left + Width * Index * SegmentSize - TickWidth * 0.5f, Top, TickWidth, Height);
	}

	const TCHAR* DriftLabel = !DriftBoost.bDrifting ? TEXT("") : (DriftBoost.DriftDirection > 0.f ? TEXT("   DRIFT >") : TEXT("   < DRIFT"));
	const TCHAR* BoostLabel = DriftBoost.IsBoosting() ? TEXT("   BOOSTING") : TEXT("");
	const TCHAR* ClosePassLabel = DriftBoost.bClosePassCharging ? TEXT("   CLOSE PASS") : TEXT("");
	const FString Label = FString::Printf(TEXT("BOOST %d%%  (%d/%d)%s%s%s"),
		FMath::RoundToInt(Charge * 100.f), FullSegments, Segments, BoostLabel, DriftLabel, ClosePassLabel);
	DrawText(Label, FLinearColor::White, Left, Top + Height + LabelGap);
#endif
}

void ARaceDebugHUD::DrawRaceStandings()
{
#if !UE_BUILD_SHIPPING
	if (CosmicHotshots::RaceCVars::DebugStandings.GetValueOnGameThread() == 0 || !Canvas)
	{
		return;
	}

	const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
	if (!RaceState)
	{
		return;
	}

	const float Left = Canvas->ClipX * TableLeftFraction;
	float Top = Canvas->ClipY * TableTopFraction;
	DrawRect(BackColour, Left - TablePadding, Top - TablePadding, Canvas->ClipX * TableWidthFraction + 2.f * TablePadding,
		TableLineHeight * (RaceState->GetStandings().Num() + 2) + 2.f * TablePadding);

	const TCHAR* PhaseName = RaceState->GetRacePhase() == ERacePhase::Racing ? TEXT("RACING")
		: RaceState->GetRacePhase() == ERacePhase::Finished ? TEXT("FINISHED") : TEXT("NO TRACK");
	const FRaceGap LeaderGap = RaceState->GetLeaderToSecondGap();
	DrawText(FString::Printf(TEXT("%s  t=%.2f  laps=%d  1st-2nd: %.1f m / %.2f s"), PhaseName, RaceState->GetRaceTime(),
		RaceState->GetNumLaps(), LeaderGap.DistanceCm * CmToMeters, LeaderGap.TimeSeconds), FLinearColor::White, Left, Top);
	Top += TableLineHeight;

	const APawn* OwnPawn = GetOwningPawn();
	for (const FRacerStanding& Standing : RaceState->GetStandings())
	{
		const FString GapText = Standing.GapToCarAhead.bValid
			? FString::Printf(TEXT("+%.1f m  +%.2f s"), Standing.GapToCarAhead.DistanceCm * CmToMeters, Standing.GapToCarAhead.TimeSeconds)
			: FString(TEXT("leader"));
		const FString Line = FString::Printf(TEXT("%d. %-16s lap %d/%d  cp %d  %.0f m  lat %.1f m  %s%s"),
			Standing.Rank, *GetNameSafe(Standing.Vehicle), Standing.CurrentLap, RaceState->GetNumLaps(), Standing.NextCheckpoint,
			Standing.Progress * CmToMeters, Standing.LateralOffset * CmToMeters, *GapText,
			Standing.bFinished ? *FString::Printf(TEXT("  FINISHED %.2f s"), Standing.FinishTime) : TEXT(""));
		DrawText(Line, Standing.Vehicle == OwnPawn ? OwnRowColour : FLinearColor::White, Left, Top);
		Top += TableLineHeight;
	}
#endif
}

void ARaceDebugHUD::DrawPassives()
{
#if !UE_BUILD_SHIPPING
	if (CosmicHotshots::RaceCVars::DebugPassives.GetValueOnGameThread() == 0 || !Canvas)
	{
		return;
	}

	const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetOwningPawn());
	if (!Vehicle)
	{
		return;
	}

	const UVehiclePassiveComponent* PassiveComponent = Vehicle->GetPassive();
	const TArray<FVehicleModifier>& Modifiers = Vehicle->GetModifiers()->GetStack().GetModifiers();
	const FVehicleStatModifiers& Totals = Vehicle->GetModifiers()->GetStack().GetTotals();

	const float Left = Canvas->ClipX * PassivesLeftFraction;
	float Top = Canvas->ClipY * TableTopFraction;
	DrawRect(BackColour, Left - TablePadding, Top - TablePadding, Canvas->ClipX * PassivesWidthFraction + 2.f * TablePadding,
		TableLineHeight * (Modifiers.Num() + 3) + 2.f * TablePadding);

	FString PassiveLine = TEXT("Passive: none");
	if (const UPassiveBase* Passive = PassiveComponent->GetPassive())
	{
		PassiveLine = PassiveComponent->IsPassiveRunning() ? Passive->GetDebugString() : FString(TEXT("Passive: switched OFF"));
	}
	DrawText(PassiveLine, OwnRowColour, Left, Top);
	Top += TableLineHeight;

	// Item slot (dev stand-in for the HUD slot).
	if (const ARaceGameState* RaceState = GetWorld()->GetGameState<ARaceGameState>())
	{
		const FItemSlot Slot = RaceState->GetItemSystem()->GetSlot(Vehicle);
		const FString ItemName = Slot.Item ? Slot.Item->DisplayName.ToString() : FString(TEXT("?"));
		const FString SlotText = Slot.State == EItemSlotState::Ready ? FString::Printf(TEXT("[%s] READY"), *ItemName)
			: Slot.State == EItemSlotState::Rolling ? FString::Printf(TEXT("roulette... %.1f s"), Slot.RollTimeRemaining)
			: FString(TEXT("(empty)"));
		DrawText(FString::Printf(TEXT("Item: %s   replaced x%d"), *SlotText, Slot.ReplaceCount), OwnRowColour, Left, Top);
		Top += TableLineHeight;
	}

	DrawText(FString::Printf(TEXT("Stack total: max speed %+.1f%%  accel %+.1f%%"),
		(Totals.MaxSpeed - 1.f) * PercentScale, (Totals.Acceleration - 1.f) * PercentScale), FLinearColor::White, Left, Top);
	Top += TableLineHeight;

	for (const FVehicleModifier& Modifier : Modifiers)
	{
		const float TimeRemaining = Modifier.GetTimeRemaining();
		const FString TimeText = Modifier.bEnding ? FString(TEXT("fading out"))
			: TimeRemaining < 0.f ? FString(TEXT("until removed")) : FString::Printf(TEXT("%.1f s left"), TimeRemaining);
		DrawText(FString::Printf(TEXT("%s  %s %s%.3f  strength %.0f%%  %s"),
			*Modifier.Spec.SourceId.ToString(), *UEnum::GetDisplayValueAsText(Modifier.Spec.Stat).ToString(),
			Modifier.Spec.Op == EVehicleModifierOp::Multiply ? TEXT("x") : TEXT("+"), Modifier.Spec.Value,
			Modifier.Strength * PercentScale, *TimeText), FLinearColor::White, Left, Top);
		Top += TableLineHeight;
	}
#endif
}
