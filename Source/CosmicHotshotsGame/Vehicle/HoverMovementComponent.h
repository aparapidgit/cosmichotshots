// Hover movement: drives one rigid body with custom arcade hover physics.
//
// Physics runs in AsyncPhysicsTickComponent, which the engine calls once per
// FIXED physics step (Project Settings > Physics > Tick Physics Async), so the
// driving feel does not depend on the frame rate. The engine runs that callback
// on the game thread while the physics thread waits, so reading the input and
// the tuning asset there is safe; body state must go through the physics-thread
// handle.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicle/HoverSim.h"
#include "Vehicle/VehicleInputState.h"
#include "HoverMovementComponent.generated.h"

class UPrimitiveComponent;
class UVehicleTuningData;

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class COSMICHOTSHOTSGAME_API UHoverMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHoverMovementComponent();

	/** The simulated rigid body this component drives (the vehicle root). */
	void SetUpdatedBody(UPrimitiveComponent* InBody);

	void SetTuning(const UVehicleTuningData* InTuning);

	/** Latest driver intent. Called on the game thread by the owning vehicle. */
	void SetInput(const FVehicleInputState& InInput);

	/** State of the last physics step (for debug display and, later, HUD/VFX). */
	const FHoverSimState& GetSimState() const { return SimState; }

	virtual void BeginPlay() override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Data asset values with console overrides applied. */
	FHoverSimParams BuildSimParams() const;

	void LogAndTestHooks(float DeltaTime);
	void DrawDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedBody;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleTuningData> Tuning;

	FVehicleInputState Input;
	FHoverSimState SimState;

	/** Simulation time since this component started ticking physics. */
	double ElapsedSimTime = 0.0;
	double NextLogTime = 0.0;
};
