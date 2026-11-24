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
#include "Vehicle/VehicleImpactSim.h"
#include "Vehicle/VehicleInputState.h"
#include "HoverMovementComponent.generated.h"

namespace Chaos
{
	class FSingleParticlePhysicsProxy;
}

class UPrimitiveComponent;
class UVehicleStatsComponent;
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

	/** Source of the stat multipliers applied on top of the base tuning. Optional. */
	void SetStats(const UVehicleStatsComponent* InStats);

	/** Latest driver intent. Called on the game thread by the owning vehicle. */
	void SetInput(const FVehicleInputState& InInput);

	/**
	 * Moves the impacts the physics steps applied since the last call into OutImpacts.
	 * The impact component turns them into feedback (camera shake, rumble, VFX, audio).
	 */
	void TakePendingImpacts(TArray<FVehicleImpactInfo>& OutImpacts);

	/** State of the last physics step (for debug display and, later, HUD/VFX). */
	const FHoverSimState& GetSimState() const { return SimState; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Data asset values with console overrides applied. */
	FHoverSimParams BuildSimParams() const;

	FVehicleImpactParams BuildImpactParams() const;

	/** Applies the hits the physics engine reported in the previous step. Called from the physics step. */
	void ApplyImpacts(Chaos::FRigidBodyHandle_Internal& Body, Chaos::FSingleParticlePhysicsProxy& Proxy);

	/** Advances the simulation clock and writes the periodic state line to the log. Called from the physics step. */
	void LogStep(float DeltaTime);

	/** Dev-only test helpers (dummy car, screenshot, auto quit). Called from the normal game tick. */
	void RunTestHelpers();
	void DrawDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedBody;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleTuningData> Tuning;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleStatsComponent> Stats;

	FVehicleInputState Input;
	FHoverSimState SimState;

	// Impacts (see VehicleContactSubsystem.h and VehicleImpactSim.h)
	FVehicleImpactState ImpactState;
	/** Impacts applied by physics steps, waiting for the impact component's game tick. */
	TArray<FVehicleImpactInfo> PendingImpacts;
	/** Reused every step so reading contacts does not allocate. */
	TArray<FVehicleRawContact> RawContacts;
	TArray<FVehicleContact> ResolvedContacts;
	/** This body's physics particle as registered with the contact subsystem. */
	const Chaos::FGeometryParticleHandle* RegisteredParticle = nullptr;
	TWeakObjectPtr<UVehicleContactSubsystem> ContactSubsystem;

	/**
	 * Set when the boost button goes down, cleared by the physics step that uses it.
	 * A frame can have zero physics steps (high frame rates), so a short press must be remembered.
	 */
	bool bBoostPressedLatch = false;

	/** Same for the active ability button: a press is remembered until a physics step uses it. */
	bool bAbilityPressedLatch = false;

	bool IsOwnerPlayerControlled() const;

	/** Test helper (ch.Test.PlayerCharacter): the test character has been applied. */
	bool bTestCharacterApplied = false;

	/** Test helper (ch.Test.SpawnDummyAhead): the dummy car has been spawned. */
	bool bTestDummySpawned = false;

	/** Test helper (ch.Test.ScreenshotTime): the one screenshot has been requested. */
	bool bTestScreenshotTaken = false;

	/** Scripted test input only: was the scripted boost button held on the previous step. */
	bool bScriptedBoostWasHeld = false;
	bool bScriptedAbilityWasHeld = false;

	/** Test helper (ch.Test.StartBoostCharge): the starting meter value has been applied. */
	bool bTestBoostChargeApplied = false;

	/** Simulation time since this component started ticking physics. */
	double ElapsedSimTime = 0.0;
	double NextLogTime = 0.0;
};
