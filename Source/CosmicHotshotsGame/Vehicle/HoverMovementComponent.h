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
#include "Vehicle/VehicleStepInput.h"
#include "HoverMovementComponent.generated.h"

namespace Chaos
{
	class FSingleParticlePhysicsProxy;
}

class FTrackSplineTable;
class UPrimitiveComponent;
class UVehicleModifierComponent;
class UVehicleAbilityComponent;
class UVehiclePassiveComponent;
class UVehicleZoneComponent;
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

	/** The modifier stack to advance and the passive to run at the start of every physics step. Both optional. */
	void SetModifierComponents(UVehicleModifierComponent* InModifiers, UVehiclePassiveComponent* InPassive);

	/** The component that applies the track's zones to this car; it is stepped inside the physics step. */
	void SetZoneComponent(UVehicleZoneComponent* InZones) { Zones = InZones; }

	/** The component that runs the character's active ability; it is stepped inside the physics step. */
	void SetAbilityComponent(UVehicleAbilityComponent* InAbility) { Ability = InAbility; }

	/** Latest driver intent. Called on the game thread by the owning vehicle. */
	void SetInput(const FVehicleInputState& InInput);

	/**
	 * A driver that is asked for its input inside every physics step (the AI). While one is set, it
	 * replaces the input from SetInput. The source must clear itself before it goes away.
	 */
	void SetStepInputSource(IVehicleStepInputSource* InSource);

	/** Removes the step driver, if it is still the given one. */
	void ClearStepInputSource(const IVehicleStepInputSource* InSource);

	/**
	 * Moves the impacts the physics steps applied since the last call into OutImpacts.
	 * The impact component turns them into feedback (camera shake, rumble, VFX, audio).
	 */
	void TakePendingImpacts(TArray<FVehicleImpactInfo>& OutImpacts);

	/**
	 * Test helper (ch.Test.Bots): from now on this car ignores its driver and drives the test autopilot
	 * along a line LateralOffset cm beside the track centre, with full throttle scaled by ThrottleScale.
	 */
	void SetTestBot(float LateralOffset, float ThrottleScale);

	/**
	 * Use-item button presses counted so far, and the SimTime of the physics step that counted the latest.
	 * The item system uses a ready item when the count changes (see UItemSystemComponent).
	 */
	int32 GetUseItemPressCount() const { return UseItemPressCount; }
	float GetUseItemPressSimTime() const { return UseItemPressSimTime; }

	/** State of the last physics step (for debug display and, later, HUD/VFX). */
	const FHoverSimState& GetSimState() const { return SimState; }

	/** The input the last physics step really used (human, AI or scripted), for debug display. */
	const FVehicleInputState& GetLastStepInput() const { return LastStepInput; }

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

	/** Dev-only test helpers (test character, start boost, screenshot, auto quit). Called from the normal game tick. */
	void RunTestHelpers();
	void DrawDebug() const;

	/**
	 * Test helper (ch.Test.ScriptedInput 6): steers towards a point ahead on the track line at full
	 * throttle, so a headless test can drive whole laps. Called from the physics step.
	 * NOT the race AI: that gets its own controller in Phase 2.
	 */
	FVehicleInputState GetAutopilotInput(const Chaos::FRigidBodyHandle_Internal& Body, float LateralOffset, float ThrottleScale, float DeltaTime, float MaxSpeed);

	/** Autopilot: seconds spent (almost) standing still; long enough means stuck against something. */
	float AutopilotStuckTime = 0.f;
	/** Autopilot: seconds of reversing left to get free. */
	float AutopilotReverseTimeRemaining = 0.f;
	/** Test helper (scripted mode 8): simulation time the scripted drifts started; negative = not yet. */
	double DriftChainStartTime = -1.0;

	/** Test helper (ch.Test.Bots): this car is a test bot and drives the autopilot. */
	bool bTestBot = false;
	/** Test bot: its line beside the track centre line (cm, positive = right). */
	float TestBotLateralOffset = 0.f;
	/** Test bot: full throttle is scaled by this, so bots differ in pace. */
	float TestBotThrottleScale = 1.f;

	/** Test helper: the track line for the autopilot. Immutable, safe to read in the physics step. */
	TSharedPtr<const FTrackSplineTable> AutopilotTrack;
	/** Test helper: the autopilot's last distance along the track line, negative = not placed yet. */
	float AutopilotDistance = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedBody;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleTuningData> Tuning;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleStatsComponent> Stats;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleModifierComponent> Modifiers;

	UPROPERTY(Transient)
	TObjectPtr<UVehiclePassiveComponent> Passive;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleZoneComponent> Zones;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleAbilityComponent> Ability;

	/** Test helper (ch.Test.SpeedModifier): the test modifier has been added. */
	bool bTestModifierApplied = false;

	/** Impacts counted so far (walls and cars). Passives watch it to notice a hit. */
	int32 ImpactCounter = 0;

	FVehicleInputState Input;
	FVehicleInputState LastStepInput;

	/** Running totals for the DRIVE summary log line of a human's car (dev builds). */
	struct FDriveSummary
	{
		int32 Steps = 0;
		int32 DriftSteps = 0;
		int32 BoostSteps = 0;
		float Throttle = 0.f;
		float Brake = 0.f;
		float Steer = 0.f;
		float Speed = 0.f;
		float MaxSpeed = 0.f;
		float Time = 0.f;
	};
	FDriveSummary DriveSummary;

	/** Step driver (AI), not owned. Null = the input from SetInput is used. */
	IVehicleStepInputSource* StepInputSource = nullptr;
	bool bStepBoostWasHeld = false;
	bool bStepAbilityWasHeld = false;
	bool bStepUseItemWasHeld = false;

	/** The race tracker, for the start lock. Null in levels without a race. */
	TWeakObjectPtr<class URaceTrackerComponent> RaceTracker;
	bool bRaceTrackerSearched = false;
	/** This physics step is before the start of the race: no input is used. */
	bool bRaceLocked = false;

	/** Set when the human's use-item button goes down; cleared by the physics step that counts it. */
	bool bUseItemPressedLatch = false;
	/** This step's input came from a scripted test, so its use-item button is read like a step driver's. */
	bool bScriptedItemInput = false;
	/** Use-item presses so far and the SimTime of the physics step that counted the latest one. */
	int32 UseItemPressCount = 0;
	float UseItemPressSimTime = 0.f;
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
