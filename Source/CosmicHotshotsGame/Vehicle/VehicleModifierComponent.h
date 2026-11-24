// Vehicle modifier component: owns the vehicle's modifier stack (see VehicleModifierStack.h).
// UHoverMovementComponent advances it once per fixed physics step, before it builds the
// step's driving values; UVehicleStatsComponent multiplies its totals into the final stats.
// Passives, items and abilities add and remove modifiers through GetStack().

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicle/VehicleModifierStack.h"
#include "VehicleModifierComponent.generated.h"

UCLASS(ClassGroup = (Vehicle))
class COSMICHOTSHOTSGAME_API UVehicleModifierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleModifierComponent();

	/** The stack. Change it only from the fixed physics step (or Queue() from another vehicle's step). */
	FVehicleModifierStack& GetStack() { return Stack; }
	const FVehicleModifierStack& GetStack() const { return Stack; }

private:
	FVehicleModifierStack Stack;
};
