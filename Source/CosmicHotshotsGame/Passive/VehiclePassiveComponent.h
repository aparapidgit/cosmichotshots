// Vehicle passive component: runs the character's passive on one vehicle.
// It creates the UPassiveBase object for the character's UPassiveDefinition and steps it
// from the vehicle's fixed physics step (called by UHoverMovementComponent, right after the
// modifier stack advanced), with the race state of the previous step. Two switches turn
// passives off for testing: the console variable ch.Passives.Enabled (all of them) and
// UPassiveDefinition::bPassiveEnabled (one character).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehiclePassiveComponent.generated.h"

class UPassiveBase;
class UPassiveDefinition;
struct FHoverSimState;
class URaceTrackerComponent;
class UVehicleModifierComponent;

UCLASS(ClassGroup = (Vehicle))
class COSMICHOTSHOTSGAME_API UVehiclePassiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehiclePassiveComponent();

	/** The vehicle's modifier stack the passive writes to. */
	void SetModifiers(UVehicleModifierComponent* InModifiers);

	/** Passive of the vehicle's character; null = no passive. Replaces the previous one (its modifiers fade out). */
	void SetDefinition(const UPassiveDefinition* InDefinition);

	/** One fixed physics step. SimTime is the value the engine passed to the step. */
	void StepPassive(float DeltaTime, float SimTime, const FHoverSimState& VehicleState, int32 ImpactCount);

	/** Null when the character has no passive. */
	const UPassiveBase* GetPassive() const { return Passive; }

	/** False while the passive is switched off (console variable or its definition). */
	bool IsPassiveRunning() const { return bRunning; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UPassiveBase> Passive;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleModifierComponent> Modifiers;

	TWeakObjectPtr<URaceTrackerComponent> Tracker;
	int32 RacerId = INDEX_NONE;
	bool bRunning = false;
	/** Last logged debug text, so the log only gets a line when the passive's state changes. */
	FString LastLoggedState;
};
