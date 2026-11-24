// Step input source: a driver that is asked for its input inside the fixed physics step.
//
// A human's input arrives once per frame through ARaceVehicle::SetInput. A computer driver
// must not work that way: deciding once per frame would make AI races depend on the frame
// rate. So an AI driver registers itself here and the vehicle asks it for the input of every
// physics step. The vehicle still does not know who drives: it only sees this interface.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/VehicleInputState.h"

class FVehicleModifierStack;
struct FHoverSimState;

/** What the vehicle tells its step driver about itself, read from the physics body in the step. */
struct FVehicleStepInputContext
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Velocity = FVector::ZeroVector;
	float DeltaTime = 0.f;
	/** The value the engine passed to this physics step. */
	float SimTime = 0.f;
	/** Seconds this vehicle has been simulating. */
	double ElapsedSimTime = 0.0;
	/** Driving state after the vehicle's previous step (boost meter, drift, loss of control). */
	const FHoverSimState* State = nullptr;
	/** The vehicle's modifier stack (rubber banding goes through it). May be null. */
	FVehicleModifierStack* Modifiers = nullptr;
};

class IVehicleStepInputSource
{
public:
	virtual ~IVehicleStepInputSource() = default;

	/**
	 * Does this driver take the car in this step? A driver that only steps in at some point (the computer
	 * driver that takes a human's car after the finish) answers false until then, and the human's input is used.
	 */
	virtual bool WantsControl(const FVehicleStepInputContext& Context) { return true; }

	/** The driver's input for this physics step. */
	virtual FVehicleInputState GetStepInput(const FVehicleStepInputContext& Context) = 0;
};
