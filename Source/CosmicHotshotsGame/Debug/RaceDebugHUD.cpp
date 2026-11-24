#include "Debug/RaceDebugHUD.h"

#include "Engine/Canvas.h"
#include "HAL/IConsoleManager.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"
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
}

void ARaceDebugHUD::DrawHUD()
{
	Super::DrawHUD();

#if !UE_BUILD_SHIPPING
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
