#include "Vehicle/VehicleHullComponent.h"

#include "PhysicsEngine/BodySetup.h"

namespace
{
	// Capsule elements are built along Z; pitch them so they lie along the body's length (X).
	const FRotator LengthwiseCapsuleRotation(90.0, 0.0, 0.0);
}

UVehicleHullComponent::UVehicleHullComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetGenerateOverlapEvents(false);
}

void UVehicleHullComponent::SetHullSize(const FVector& InHullSize)
{
	HullSize = InHullSize;
	RebuildBodySetup();

	if (IsPhysicsStateCreated())
	{
		RecreatePhysicsState();
	}
	UpdateBounds();
}

UBodySetup* UVehicleHullComponent::GetBodySetup()
{
	if (!HullBodySetup)
	{
		RebuildBodySetup();
	}
	return HullBodySetup;
}

void UVehicleHullComponent::RebuildBodySetup()
{
	HullBodySetup = NewObject<UBodySetup>(this, NAME_None, RF_Transient);
	HullBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

	const FVector HalfSize = HullSize * 0.5;

	// Radius = half height, so the two capsules together fill the hull box.
	const double Radius = FMath::Min(HalfSize.Z, HalfSize.Y);
	const double CylinderLength = FMath::Max(HullSize.X - 2.0 * Radius, 0.0);
	const double SideOffset = FMath::Max(HalfSize.Y - Radius, 0.0);

	for (const double Side : { -1.0, 1.0 })
	{
		FKSphylElem Capsule;
		Capsule.Center = FVector(0.0, Side * SideOffset, 0.0);
		Capsule.Rotation = LengthwiseCapsuleRotation;
		Capsule.Radius = static_cast<float>(Radius);
		Capsule.Length = static_cast<float>(CylinderLength);
		HullBodySetup->AggGeom.SphylElems.Add(Capsule);
	}
}

FBoxSphereBounds UVehicleHullComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FBox(-HullSize * 0.5, HullSize * 0.5)).TransformBy(LocalToWorld);
}
