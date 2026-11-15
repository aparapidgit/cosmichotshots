#include "Vehicle/VehicleImpactComponent.h"

#include "CosmicHotshotsGame.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleDebug.h"
#include "Vehicle/VehicleStatsComponent.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	// A contact whose normal is this vertical is the ground (hard landing), not a wall.
	constexpr double GroundNormalZ = 0.7;

	// Debug display only.
	constexpr float DebugImpactRadius = 40.f;
	constexpr float DebugImpactLifetime = 1.5f;
}

UVehicleImpactComponent::UVehicleImpactComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleImpactComponent::Initialize(UPrimitiveComponent* InBody, UHoverMovementComponent* InMovement, const UVehicleStatsComponent* InStats)
{
	Body = InBody;
	Movement = InMovement;
	Stats = InStats;
	Tuning = InStats ? InStats->GetTuning() : nullptr;
}

void UVehicleImpactComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Body)
	{
		Body->OnComponentHit.AddDynamic(this, &UVehicleImpactComponent::HandleBodyHit);
	}
}

bool UVehicleImpactComponent::IsOnCooldown(const AActor* OtherActor) const
{
	const double* EndTime = CooldownEndTimes.Find(OtherActor);
	return EndTime && GetWorld()->GetTimeSeconds() < *EndTime;
}

void UVehicleImpactComponent::StartCooldown(const AActor* OtherActor)
{
	const double Now = GetWorld()->GetTimeSeconds();

	// Drop finished cooldowns so the map never grows (one entry per wall segment otherwise).
	for (auto It = CooldownEndTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= Now)
		{
			It.RemoveCurrent();
		}
	}
	CooldownEndTimes.Add(OtherActor, Now + Tuning->ImpactCooldown);
}

void UVehicleImpactComponent::HandleBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!Body || !Tuning || !OtherActor || OtherActor == GetOwner() || IsOnCooldown(OtherActor))
	{
		return;
	}

	// Normal pointing from what was hit towards this vehicle (hit results are not consistent about its sign).
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return;
	}

	ARaceVehicle* OtherVehicle = Cast<ARaceVehicle>(OtherActor);
	const FVector AwayFromOther = OtherVehicle
		? Body->GetComponentLocation() - OtherVehicle->GetActorLocation()
		: Body->GetComponentLocation() - Hit.ImpactPoint;
	if (FVector::DotProduct(Normal, AwayFromOther) < 0.0)
	{
		Normal = -Normal;
	}

	// The physics engine has already stopped the two bodies closing in on each other, so the
	// closing speed is recovered from the impulse it used: impulse = reduced mass * closing speed.
	const float SelfMass = Body->GetMass();
	float ReducedMass = SelfMass;
	if (OtherVehicle && OtherComponent)
	{
		const float OtherMass = OtherComponent->GetMass();
		ReducedMass = (SelfMass * OtherMass) / FMath::Max(SelfMass + OtherMass, UE_KINDA_SMALL_NUMBER);
	}
	const float ClosingSpeed = static_cast<float>(FMath::Abs(FVector::DotProduct(NormalImpulse, Normal))) / FMath::Max(ReducedMass, UE_KINDA_SMALL_NUMBER);

	if (ClosingSpeed < Tuning->MinImpactSpeed)
	{
		return;
	}

	if (OtherVehicle)
	{
		HandleVehicleHit(*OtherVehicle, Normal, ClosingSpeed, Hit);
	}
	else if (FMath::Abs(Normal.Z) < GroundNormalZ)
	{
		HandleWallHit(OtherActor, Normal, ClosingSpeed, Hit);
	}
}

void UVehicleImpactComponent::HandleVehicleHit(ARaceVehicle& OtherVehicle, const FVector& Normal, float ClosingSpeed, const FHitResult& Hit)
{
	const float SelfMass = Body->GetMass();
	const float OtherMass = OtherVehicle.GetBodyMass();
	const float ReducedMass = (SelfMass * OtherMass) / FMath::Max(SelfMass + OtherMass, UE_KINDA_SMALL_NUMBER);

	// Knock-back this car receives: the attacker's RamPower pushes it up, this car's Stability
	// pulls it down, and the reduced mass makes the heavier car move less.
	const float OtherRamPower = OtherVehicle.GetStats()->GetRamPower();
	const float Impulse = Tuning->VehicleBounce * ClosingSpeed * ReducedMass * (OtherRamPower / FMath::Max(Stats->GetStability(), UE_KINDA_SMALL_NUMBER));
	const float SpeedChange = FMath::Min(Impulse / FMath::Max(SelfMass, UE_KINDA_SMALL_NUMBER), Tuning->MaxImpactSpeedChange);

	Body->AddImpulse(Normal * SpeedChange, NAME_None, /*bVelChange=*/true);

	FVehicleImpactInfo Info;
	Info.OtherActor = &OtherVehicle;
	Info.bOtherIsVehicle = true;
	Info.ImpactPoint = Hit.ImpactPoint;
	Info.Normal = Normal;
	Info.ClosingSpeed = ClosingSpeed;
	Info.SpeedChange = SpeedChange;
	ApplyReaction(Info, /*bAllowSpinOut=*/true);

	StartCooldown(&OtherVehicle);
	FinishImpact(Info);
}

void UVehicleImpactComponent::HandleWallHit(AActor* OtherActor, const FVector& Normal, float ClosingSpeed, const FHitResult& Hit)
{
	// Bounce back off the wall.
	const float BounceSpeed = FMath::Min(Tuning->WallBounce * ClosingSpeed, Tuning->MaxImpactSpeedChange);

	// Lose part of the speed along the wall: a light scrape costs little, a hard hit costs a lot.
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const FVector AlongWall = Velocity - Normal * FVector::DotProduct(Velocity, Normal);
	const float Hardness = FMath::Clamp(ClosingSpeed / Tuning->WallHardHitSpeed, 0.f, 1.f);
	const FVector ScrapeLoss = -AlongWall * (Tuning->WallSpeedLoss * Hardness);

	Body->AddImpulse(Normal * BounceSpeed + ScrapeLoss, NAME_None, /*bVelChange=*/true);

	FVehicleImpactInfo Info;
	Info.OtherActor = OtherActor;
	Info.bOtherIsVehicle = false;
	Info.ImpactPoint = Hit.ImpactPoint;
	Info.Normal = Normal;
	Info.ClosingSpeed = ClosingSpeed;
	// For feedback, a wall hit is as strong as the speed it took away into the wall.
	Info.SpeedChange = ClosingSpeed;
	// Walls wobble the car but never spin it out: wall contact is too common to punish that hard.
	ApplyReaction(Info, /*bAllowSpinOut=*/false);

	StartCooldown(OtherActor);
	FinishImpact(Info);
}

void UVehicleImpactComponent::ApplyReaction(FVehicleImpactInfo& Info, bool bAllowSpinOut)
{
	Info.Strength = Tuning->SpinOutThreshold > 0.f ? FMath::Clamp(Info.SpeedChange / Tuning->SpinOutThreshold, 0.f, 1.f) : 1.f;

	if (bAllowSpinOut && Info.SpeedChange >= Tuning->SpinOutThreshold)
	{
		Info.Reaction = EImpactReaction::SpinOut;

		// Spin away from the side that was hit.
		const FVector ToImpact = Info.ImpactPoint - Body->GetComponentLocation();
		const double Side = FVector::DotProduct(FVector::CrossProduct(Body->GetForwardVector(), ToImpact), Body->GetUpVector());
		const double SpinSign = Side >= 0.0 ? -1.0 : 1.0;
		Body->AddAngularImpulseInDegrees(Body->GetUpVector() * (SpinSign * Tuning->SpinOutYawRate), NAME_None, /*bVelChange=*/true);

		if (Movement)
		{
			Movement->ApplyControlLoss(Tuning->SpinOutDuration);
		}
	}
	else if (Info.SpeedChange >= Tuning->WobbleThreshold)
	{
		Info.Reaction = EImpactReaction::Wobble;
		if (Movement)
		{
			Movement->ApplyControlLoss(Tuning->WobbleDuration);
		}
	}
}

void UVehicleImpactComponent::FinishImpact(const FVehicleImpactInfo& Info)
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	if (VehicleCVars::LogInterval.GetValueOnGameThread() > 0.f)
	{
		static const TCHAR* const ReactionNames[] = { TEXT("none"), TEXT("wobble"), TEXT("spinout") };
		UE_LOG(LogCosmicVehicle, Log, TEXT("IMPACT %s hit %s (%s) closing=%.0f speedChange=%.0f strength=%.2f reaction=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Info.OtherActor), Info.bOtherIsVehicle ? TEXT("vehicle") : TEXT("wall"),
			Info.ClosingSpeed, Info.SpeedChange, Info.Strength, ReactionNames[static_cast<int32>(Info.Reaction)]);
	}

	if (VehicleCVars::DebugDraw.GetValueOnGameThread() != 0)
	{
		const FColor Color = Info.Reaction == EImpactReaction::SpinOut ? FColor::Red
			: (Info.Reaction == EImpactReaction::Wobble ? FColor::Orange : FColor::Yellow);
		DrawDebugSphere(GetWorld(), Info.ImpactPoint, DebugImpactRadius * (0.5f + Info.Strength), 12, Color, false, DebugImpactLifetime);
	}
#endif

	OnImpact.Broadcast(Info);
}
