#include "Camera/RaceCameraComponent.h"

#include "CosmicHotshotsGame.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	const FName CameraProbeTag(TEXT("RaceCameraProbe"));

	constexpr float KmhToCmPerSecond = 100000.f / 3600.f;

	// FOV values in the tuning asset are horizontal degrees on a screen of this shape.
	constexpr float ReferenceAspectRatio = 16.f / 9.f;

	// Keeps the camera from ending up exactly on the pivot when an obstacle is right behind the car.
	constexpr float MinArmFraction = 0.05f;

	// When the camera has more than this much yaw left to turn, it is swinging to or from look-back.
	constexpr float LookBackSwingAngle = 100.f;

	TAutoConsoleVariable<float> CVarBaseFov(
		TEXT("ch.Camera.BaseFOV"), -1.f,
		TEXT("Dev override of the base FOV (horizontal degrees on 16:9), e.g. 80 = Close, 95 = Normal, 110 = Wide. Negative = use the player's setting."),
		ECVF_Cheat);

	// Scalar parameter the speed lines post-process material must expose.
	const FName SpeedLinesIntensityParameter(TEXT("Intensity"));

	TAutoConsoleVariable<float> CVarSpeedLines(
		TEXT("ch.Camera.SpeedLines"), -1.f,
		TEXT("Dev override of the speed lines strength, 0 (off) to 1 (full). Negative = use the player's setting."),
		ECVF_Cheat);

	// The shake's yaw wobbles at a different rate than its pitch, so it does not read as a circle.
	constexpr float ShakeYawFrequencyRatio = 1.37f;

	TAutoConsoleVariable<float> CVarShake(
		TEXT("ch.Camera.Shake"), -1.f,
		TEXT("Dev override of the camera shake strength, 0 (off) to 1 (full). Negative = use the player's setting."),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarCameraLog(
		TEXT("ch.Debug.CameraLog"), 0.f,
		TEXT("Seconds between camera state lines in the log. 0 = off."),
		ECVF_Cheat);
}

URaceCameraComponent::URaceCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After physics, so the camera follows the car's final position for this frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// The camera places itself in world space every frame.
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);

	// FieldOfView is driven as a VERTICAL FOV, so wide viewports see more to the sides
	// instead of being cropped top and bottom (ultrawide, split-screen).
	bConstrainAspectRatio = false;
	bOverrideAspectRatioAxisConstraint = true;
	AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
}

const UCameraTuningData* URaceCameraComponent::GetActiveTuning() const
{
	if (Tuning)
	{
		return Tuning;
	}
	if (const UCameraTuningData* ProjectDefault = GetDefault<UCosmicHotshotsSettings>()->CameraTuning.LoadSynchronous())
	{
		return ProjectDefault;
	}
	return GetDefault<UCameraTuningData>();
}

void URaceCameraComponent::SetBaseFov(float InBaseFov)
{
	const UCameraTuningData* ActiveTuning = GetActiveTuning();
	BaseFovOverride = FMath::Clamp(InBaseFov, ActiveTuning->MinBaseFov, ActiveTuning->MaxBaseFov);
}

void URaceCameraComponent::SetBaseFovPreset(ECameraFovPreset Preset)
{
	SetBaseFov(GetActiveTuning()->GetPresetFov(Preset));
}

float URaceCameraComponent::GetBaseFov() const
{
	const UCameraTuningData* ActiveTuning = GetActiveTuning();

	float BaseFov = BaseFovOverride >= 0.f ? BaseFovOverride : ActiveTuning->FovNormal;
#if !UE_BUILD_SHIPPING
	const float DevOverride = CVarBaseFov.GetValueOnGameThread();
	if (DevOverride >= 0.f)
	{
		BaseFov = DevOverride;
	}
#endif
	return FMath::Clamp(BaseFov, ActiveTuning->MinBaseFov, ActiveTuning->MaxBaseFov);
}

void URaceCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Speed lines are one post-process material on this camera, so each player's viewport gets its own.
	if (UMaterialInterface* SpeedLinesMaterial = GetActiveTuning()->SpeedLinesMaterial.LoadSynchronous())
	{
		SpeedLinesMaterialInstance = UMaterialInstanceDynamic::Create(SpeedLinesMaterial, this);
		SpeedLinesMaterialInstance->SetScalarParameterValue(SpeedLinesIntensityParameter, 0.f);
		PostProcessSettings.AddBlendable(SpeedLinesMaterialInstance, 1.f);
		PostProcessBlendWeight = 1.f;
	}

	SnapBehindVehicle();
}

void URaceCameraComponent::AddImpactShake(float Strength)
{
	const float ClampedStrength = FMath::Clamp(Strength, 0.f, 1.f);

	// A weaker hit never cuts a stronger shake that is still running.
	if (ShakeTimeRemaining <= 0.f || ClampedStrength >= ShakeStrength)
	{
		ShakeStrength = ClampedStrength;
		ShakeTimeRemaining = GetActiveTuning()->ImpactShakeDuration;
	}
}

float URaceCameraComponent::GetShakeScale() const
{
	float Scale = ShakeScale;
#if !UE_BUILD_SHIPPING
	const float DevOverride = CVarShake.GetValueOnGameThread();
	if (DevOverride >= 0.f)
	{
		Scale = DevOverride;
	}
#endif
	return FMath::Clamp(Scale, 0.f, 1.f);
}

float URaceCameraComponent::GetSpeedLinesScale() const
{
	float Scale = SpeedLinesScale;
#if !UE_BUILD_SHIPPING
	const float DevOverride = CVarSpeedLines.GetValueOnGameThread();
	if (DevOverride >= 0.f)
	{
		Scale = DevOverride;
	}
#endif
	return FMath::Clamp(Scale, 0.f, 1.f);
}

void URaceCameraComponent::SnapBehindVehicle()
{
	UpdateCamera(0.f, /*bSnap=*/true);
	bHasSnapped = true;
}

void URaceCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateCamera(DeltaTime, /*bSnap=*/!bHasSnapped);
	bHasSnapped = true;
}

FVector URaceCameraComponent::ComputeDesiredLookDirection(const UCameraTuningData& ActiveTuning) const
{
	const AActor* Owner = GetOwner();
	const FVector Forward = Owner->GetActorForwardVector();
	const FVector Velocity = Owner->GetVelocity();
	const double Speed = Velocity.Size();

	// Only blend towards the direction of travel when moving forwards fast enough;
	// when reversing or standing the camera simply stays behind the car.
	const double MinSpeed = ActiveTuning.VelocityLookMinSpeedKmh * KmhToCmPerSecond;
	const double FullSpeed = FMath::Max(ActiveTuning.VelocityLookFullSpeedKmh * KmhToCmPerSecond, MinSpeed + 1.0);
	if (Speed <= MinSpeed || FVector::DotProduct(Velocity, Forward) <= 0.0)
	{
		return Forward;
	}

	const double SpeedWeight = FMath::Clamp((Speed - MinSpeed) / (FullSpeed - MinSpeed), 0.0, 1.0);
	const FVector TravelDirection = Velocity / Speed;
	return FMath::Lerp(Forward, TravelDirection, ActiveTuning.VelocityLookBlend * SpeedWeight).GetSafeNormal(UE_SMALL_NUMBER, Forward);
}

void URaceCameraComponent::UpdateCamera(float DeltaTime, bool bSnap)
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	const UCameraTuningData& ActiveTuning = *GetActiveTuning();
	const ARaceVehicle* Vehicle = Cast<ARaceVehicle>(Owner);

	// ----------------------------------------------------------------- Pivot
	// The point above the car that the camera orbits. It trails the car a little (position lag).
	const FVector PivotTarget = Owner->GetActorLocation() + FVector::UpVector * ActiveTuning.PivotHeight;
	if (bSnap)
	{
		SmoothedPivot = PivotTarget;
	}
	else
	{
		SmoothedPivot = FMath::VInterpTo(SmoothedPivot, PivotTarget, DeltaTime, ActiveTuning.PositionLagSpeed);
		const FVector Lag = SmoothedPivot - PivotTarget;
		if (Lag.SizeSquared() > FMath::Square(ActiveTuning.MaxPositionLag))
		{
			SmoothedPivot = PivotTarget + Lag.GetSafeNormal() * ActiveTuning.MaxPositionLag;
		}
	}

	// -------------------------------------------------------- Look direction
	const FVector DesiredDirection = bLookBack
		? -Owner->GetActorForwardVector()
		: ComputeDesiredLookDirection(ActiveTuning);

	FRotator DesiredLook = DesiredDirection.Rotation();
	DesiredLook.Roll = 0.0;

	if (bSnap)
	{
		SmoothedLookRotation = DesiredLook;
	}
	else
	{
		const float YawLeft = FMath::Abs(FMath::FindDeltaAngleDegrees(SmoothedLookRotation.Yaw, DesiredLook.Yaw));
		const bool bSwinging = bLookBack || YawLeft > LookBackSwingAngle;
		const float TurnSpeed = bSwinging ? ActiveTuning.LookBackTurnSpeed : ActiveTuning.RotationLagSpeed;
		SmoothedLookRotation = FMath::RInterpTo(SmoothedLookRotation, DesiredLook, DeltaTime, TurnSpeed);
		SmoothedLookRotation.Roll = 0.0;
	}
	const FVector LookDirection = SmoothedLookRotation.Vector();

	// ----------------------------------------------------- Speed and boost
	float SpeedRatio = 0.f;
	bool bBoosting = false;
	if (Vehicle)
	{
		const float TopSpeed = Vehicle->GetActiveTuning()->MaxSpeedKmh * KmhToCmPerSecond;
		const float ForwardSpeed = FMath::Abs(FVector::DotProduct(Owner->GetVelocity(), Owner->GetActorForwardVector()));
		SpeedRatio = TopSpeed > 0.f ? FMath::Clamp(ForwardSpeed / TopSpeed, 0.f, 1.f) : 0.f;
		bBoosting = Vehicle->GetHoverMovement()->GetSimState().DriftBoost.IsBoosting();
	}
	const float BoostTarget = bBoosting ? 1.f : 0.f;
	BoostEffect = bSnap ? BoostTarget : FMath::FInterpTo(BoostEffect, BoostTarget, DeltaTime, ActiveTuning.BoostEffectInterpSpeed);

	// --------------------------------------------------------------- Framing
	// Distance and height follow the player's base FOV (Close = nearer and lower, Wide = farther and higher).
	const float BaseFov = GetBaseFov();
	const float Distance = ActiveTuning.DistanceByBaseFov.GetRichCurveConst()->Eval(BaseFov) + ActiveTuning.BoostPullBack * BoostEffect;
	const float Height = ActiveTuning.HeightByBaseFov.GetRichCurveConst()->Eval(BaseFov);

	const FVector IdealLocation = SmoothedPivot - LookDirection * Distance + FVector::UpVector * (Height - ActiveTuning.PivotHeight);

	// ------------------------------------------------------------- Collision
	// Probe from the pivot to the ideal spot. Moving in front of an obstacle is instant
	// (never show the inside of a wall); moving back out is smooth.
	FCollisionQueryParams QueryParams(CameraProbeTag, /*bTraceComplex=*/false, Owner);
	FHitResult Hit;
	const bool bBlocked = World->SweepSingleByChannel(
		Hit, SmoothedPivot, IdealLocation, FQuat::Identity, ECC_Camera,
		FCollisionShape::MakeSphere(ActiveTuning.ProbeRadius), QueryParams);

	const float FreeFraction = bBlocked ? FMath::Max(Hit.Time, MinArmFraction) : 1.f;
	if (bSnap || FreeFraction < ArmFraction)
	{
		ArmFraction = FreeFraction;
	}
	else
	{
		ArmFraction = FMath::FInterpTo(ArmFraction, FreeFraction, DeltaTime, ActiveTuning.CollisionRecoverySpeed);
	}

	const FVector CameraLocation = SmoothedPivot + (IdealLocation - SmoothedPivot) * ArmFraction;

	// Aim ahead of the car so more of the road is visible.
	const FVector LookTarget = SmoothedPivot + LookDirection * ActiveTuning.LookAheadDistance;
	FRotator CameraRotation = (LookTarget - CameraLocation).Rotation();
	CameraRotation.Roll = 0.0;

	// ----------------------------------------------------------------- Shake
	// Impact shake: a small pitch and yaw wobble that fades out. Rotation only, so it cannot push the camera into a wall.
	if (ShakeTimeRemaining > 0.f)
	{
		ShakeTimeRemaining = FMath::Max(ShakeTimeRemaining - DeltaTime, 0.f);
		const float Fade = ShakeTimeRemaining / FMath::Max(ActiveTuning.ImpactShakeDuration, UE_KINDA_SMALL_NUMBER);
		const float Amplitude = ActiveTuning.ImpactShakeMaxAngle * ShakeStrength * Fade * GetShakeScale();
		const float Phase = (ActiveTuning.ImpactShakeDuration - ShakeTimeRemaining) * ActiveTuning.ImpactShakeFrequency * UE_TWO_PI;
		CameraRotation.Pitch += Amplitude * FMath::Sin(Phase);
		CameraRotation.Yaw += Amplitude * FMath::Sin(Phase * ShakeYawFrequencyRatio);
	}

	SetWorldLocationAndRotation(CameraLocation, CameraRotation);

	// ------------------------------------------------------------------- FOV
	// Speed and boost widen the view on top of the base FOV, up to a hard limit.
	const float SpeedEffectScale = ActiveTuning.SpeedEffectByBaseFov.GetRichCurveConst()->Eval(BaseFov, 1.f);
	const float FovOffset = (ActiveTuning.SpeedFovOffset * SpeedRatio + ActiveTuning.BoostFovOffset * BoostEffect) * SpeedEffectScale;
	const float TargetFov = FMath::Min(BaseFov + FovOffset, FMath::Max(ActiveTuning.MaxTotalFov, BaseFov));

	SmoothedFov = bSnap ? TargetFov : FMath::FInterpTo(SmoothedFov, TargetFov, DeltaTime, ActiveTuning.FovInterpSpeed);
	SetFieldOfView(ToVerticalFov(SmoothedFov, ActiveTuning));

	// ----------------------------------------------------------- Speed lines
	// Fade in from a speed threshold up to top speed, and go to full strength while boosting.
	const float StartRatio = FMath::Min(ActiveTuning.SpeedLinesStartSpeedRatio, 1.f - UE_KINDA_SMALL_NUMBER);
	const float SpeedPart = FMath::Clamp((SpeedRatio - StartRatio) / (1.f - StartRatio), 0.f, 1.f) * ActiveTuning.SpeedLinesTopSpeedIntensity;
	const float TargetSpeedLines = FMath::Max(SpeedPart, BoostEffect * ActiveTuning.SpeedLinesBoostIntensity) * GetSpeedLinesScale();
	SpeedLinesIntensity = bSnap ? TargetSpeedLines : FMath::FInterpTo(SpeedLinesIntensity, TargetSpeedLines, DeltaTime, ActiveTuning.SpeedLinesInterpSpeed);
	if (SpeedLinesMaterialInstance)
	{
		SpeedLinesMaterialInstance->SetScalarParameterValue(SpeedLinesIntensityParameter, SpeedLinesIntensity);
	}

	LogState(DeltaTime, SmoothedFov, FVector::Dist(CameraLocation, Owner->GetActorLocation()));
}

float URaceCameraComponent::GetViewportAspectRatio() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || !LocalPlayer->ViewportClient)
	{
		return ReferenceAspectRatio;
	}

	FVector2D ViewportSize = FVector2D::ZeroVector;
	LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);

	// LocalPlayer->Size is this player's share of the window in split-screen (1,1 when alone).
	const double Width = ViewportSize.X * LocalPlayer->Size.X;
	const double HeightPixels = ViewportSize.Y * LocalPlayer->Size.Y;
	return (Width > 1.0 && HeightPixels > 1.0) ? static_cast<float>(Width / HeightPixels) : ReferenceAspectRatio;
}

float URaceCameraComponent::ToVerticalFov(float HorizontalFovOn16By9, const UCameraTuningData& ActiveTuning) const
{
	const float ViewportAspect = GetViewportAspectRatio();

	// Vertical FOV that gives the requested horizontal FOV on a 16:9 screen.
	float HalfVertical = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(HorizontalFovOn16By9 * 0.5f)) / ReferenceAspectRatio);

	// On very wide viewports that vertical FOV would give an extreme horizontal FOV: cap it.
	const float HalfHorizontal = FMath::Atan(FMath::Tan(HalfVertical) * ViewportAspect);
	const float MaxHalfHorizontal = FMath::DegreesToRadians(ActiveTuning.MaxHorizontalFov * 0.5f);
	if (HalfHorizontal > MaxHalfHorizontal)
	{
		HalfVertical = FMath::Atan(FMath::Tan(MaxHalfHorizontal) / ViewportAspect);
	}

	return FMath::RadiansToDegrees(HalfVertical * 2.f);
}

void URaceCameraComponent::LogState(float DeltaTime, float HorizontalFov, float Distance)
{
#if !UE_BUILD_SHIPPING
	ElapsedTime += DeltaTime;

	const float LogInterval = CVarCameraLog.GetValueOnGameThread();
	if (LogInterval > 0.f && ElapsedTime >= NextLogTime)
	{
		NextLogTime = ElapsedTime + LogInterval;
		const float YawBehind = FMath::FindDeltaAngleDegrees(GetOwner()->GetActorRotation().Yaw, SmoothedLookRotation.Yaw);
		UE_LOG(LogCosmicVehicle, Log,
			TEXT("CAMERA %s t=%.3f baseFov=%.1f fov=%.2f verticalFov=%.2f distance=%.1f arm=%.2f yawOffset=%.2f boostEffect=%.2f speedLines=%.2f material=%d"),
			*GetNameSafe(GetOwner()), ElapsedTime, GetBaseFov(), HorizontalFov, FieldOfView, Distance, ArmFraction, YawBehind, BoostEffect,
			SpeedLinesIntensity, SpeedLinesMaterialInstance ? 1 : 0);
	}
#endif
}
