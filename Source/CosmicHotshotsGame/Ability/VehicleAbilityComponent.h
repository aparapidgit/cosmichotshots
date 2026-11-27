// Vehicle ability component: runs the character's active ability on one vehicle.
//
// Stepped from the vehicle's fixed physics step (UHoverMovementComponent), so abilities behave the
// same at every frame rate and for humans and computer drivers. It owns the button rules:
//  - a press fires the ability only when it is off cooldown, the meter holds a whole Energy Core
//    segment and the ability's own condition is met; only then is the segment spent. A press that
//    fires nothing costs nothing.
//  - after it fired, the ability runs for its ActiveTime and can be used again Cooldown seconds
//    after that (a double press never spends two segments in a row).
//  - computer drivers use a simple rule for now: fire a little after it becomes possible.
// A car whose character has no ability keeps the old placeholder behaviour (the physics spends the
// segment). It also applies what other cars' abilities do to this car: a pull for a while (Glick)
// or a move to another place on the track (Wilqis' swap; Spark moves himself the same way).

#pragma once

#include "CoreMinimal.h"
#include "Ability/ActiveAbility.h"
#include "Components/ActorComponent.h"
#include "Zone/VehicleZoneComponent.h"
#include "VehicleAbilityComponent.generated.h"

class FTrackSplineTable;
class FVehicleModifierStack;
class URaceTrackerComponent;
struct FHoverSimState;

/** Why a press of the ability button did nothing. */
enum class EAbilityBlock : uint8
{
	None,
	Cooldown,
	NoEnergy,
	Condition
};

/** The ability as the HUD shows it. Copied out of the physics step. */
struct FAbilityHudState
{
	bool bHasAbility = false;
	FText DisplayName;
	/** Seconds the fired ability still runs; above 0 = active. */
	float ActiveRemaining = 0.f;
	/** Seconds until it can be used again after it ended, and the length of that wait. */
	float CooldownRemaining = 0.f;
	float CooldownTotal = 0.f;
	/** The meter holds a whole segment. */
	bool bEnoughEnergy = false;
	/** The ability's own condition is met right now. */
	bool bConditionMet = false;
};

UCLASS(ClassGroup = (Vehicle))
class COSMICHOTSHOTSGAME_API UVehicleAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleAbilityComponent();

	virtual void BeginPlay() override;

	/** Ability of the vehicle's character; null = none. Game thread, before the race. */
	void SetDefinition(const UActiveAbilityDefinition* InDefinition);

	/**
	 * One physics step. bAbilityPressed = the button went down. Sets bOutLegacyPress when the car has no
	 * ability and the press must be handled the old way (the physics spends a segment). Returns what the
	 * car has to do in this step (pull from another car's ability, move to another place).
	 */
	FVehicleZoneStep StepAbility(float DeltaTime, float SimTime, const FVector& Location, const FVector& Velocity, bool bAbilityPressed,
		FHoverSimState& State, FVehicleModifierStack* Modifiers, int32 BoostSegments, bool& bOutLegacyPress);

	/** Another car's ability (or this car's own) asks this car for a pull or a move; applied in its next step. Physics step. */
	void QueueEffect(const FAbilityRemoteEffect& Effect);

	/** Game thread: the ability's state for the HUD. */
	FAbilityHudState GetHudState() const;

private:
	struct FActivePull
	{
		FVector Acceleration = FVector::ZeroVector;
		float Remaining = 0.f;
	};

	UPROPERTY(Transient)
	TObjectPtr<UActiveAbilityBase> Ability;

	TSharedPtr<const FTrackSplineTable> Track;
	TWeakObjectPtr<URaceTrackerComponent> Tracker;
	FString VehicleName;

	// Owned by the physics step.
	int32 RacerId = INDEX_NONE;
	float ActiveRemaining = 0.f;
	float CooldownRemaining = 0.f;
	/** Computer driver: seconds the ability has been possible; it fires at the driver's delay. */
	float AIReadyTime = 0.f;
	/** Test helper (ch.Test.UseAbilityEvery): race time of the next automatic press. */
	double NextTestPress = 0.0;
	TArray<FAbilityRemoteEffect> QueuedEffects;
	TArray<FActivePull> Pulls;

	mutable FCriticalSection HudLock;
	FAbilityHudState HudState;
};
