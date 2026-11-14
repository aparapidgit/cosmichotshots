// Vehicle hull: the single rigid body of a hovercar.
//
// Collision is two capsules lying side by side along the body's length, so the
// hull is rounded on every side and slides over track seams and along walls.
// Both capsules live in ONE body setup (no welded child components: welding
// loses the capsules' rotation).

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "VehicleHullComponent.generated.h"

class UBodySetup;

UCLASS(ClassGroup = (Vehicle))
class COSMICHOTSHOTSGAME_API UVehicleHullComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UVehicleHullComponent();

	/** Full size of the hull (length, width, height). Rebuilds the collision. */
	void SetHullSize(const FVector& InHullSize);

	const FVector& GetHullSize() const { return HullSize; }

	virtual UBodySetup* GetBodySetup() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	void RebuildBodySetup();

	UPROPERTY(VisibleAnywhere, Category = "Hull")
	FVector HullSize = FVector(400.0, 200.0, 100.0);

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UBodySetup> HullBodySetup;
};
