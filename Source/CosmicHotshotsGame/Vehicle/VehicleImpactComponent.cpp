#include "Vehicle/VehicleImpactComponent.h"

#include "CosmicHotshotsGame.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/VehicleDebug.h"

namespace
{
	// Debug display only.
	constexpr float DebugImpactRadius = 40.f;
	constexpr float DebugImpactLifetime = 1.5f;
}

UVehicleImpactComponent::UVehicleImpactComponent()
{
	// After physics, so feedback for this frame's physics steps goes out in the same frame.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UVehicleImpactComponent::Initialize(UHoverMovementComponent* InMovement)
{
	Movement = InMovement;
}

void UVehicleImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Movement)
	{
		return;
	}

	Movement->TakePendingImpacts(Impacts);
	for (const FVehicleImpactInfo& Info : Impacts)
	{
		FinishImpact(Info);
	}
	Impacts.Reset();
}

void UVehicleImpactComponent::FinishImpact(const FVehicleImpactInfo& Info)
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	if (VehicleCVars::LogInterval.GetValueOnGameThread() > 0.f)
	{
		static const TCHAR* const ReactionNames[] = { TEXT("none"), TEXT("wobble"), TEXT("spinout") };
		UE_LOG(LogCosmicVehicle, Log, TEXT("IMPACT %s t=%.3f hit %s (%s) closing=%.0f speedChange=%.0f strength=%.2f reaction=%s boostLost=%.3f"),
			*GetNameSafe(GetOwner()), Info.SimTime, *GetNameSafe(Info.OtherActor), Info.bOtherIsVehicle ? TEXT("vehicle") : TEXT("wall"),
			Info.ClosingSpeed, Info.SpeedChange, Info.Strength, ReactionNames[static_cast<int32>(Info.Reaction)], Info.BoostLost);
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
