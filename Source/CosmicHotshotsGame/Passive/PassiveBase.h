// Character passives: race-state driven abilities that adapt a car while it drives.
//
// UPassiveDefinition is the data asset a UCharacterDefinition points at: the on/off switch
// and the tunables of one passive. UPassiveBase is the runtime object, one per vehicle,
// created by UVehiclePassiveComponent. It is stepped inside the vehicle's fixed physics
// step with the race state of the previous step and answers by adding or removing
// modifiers on the vehicle's modifier stack. A passive never touches physics directly
// and works the same for human and AI drivers.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PassiveBase.generated.h"

class FVehicleModifierStack;
class UPassiveBase;
class URaceTrackerComponent;
struct FHoverSimState;
struct FRaceStepView;
struct FVehicleModifierSpec;

/** What a passive sees in one fixed step. */
struct FPassiveStepContext
{
	/** Race state of the previous physics step (ranks, gaps, events). */
	const FRaceStepView* Race = nullptr;
	/** This vehicle's driver id in Race->Racers, INDEX_NONE while it is not in the race yet. */
	int32 RacerId = INDEX_NONE;
	float DeltaTime = 0.f;
	/** The value the engine passed to this physics step (stamps queued effects on other cars). */
	float SimTime = 0.f;
	/** The vehicle's own modifier stack. */
	FVehicleModifierStack* Modifiers = nullptr;
	/** The vehicle's own driving state after its previous step (drift, slip angle, loss of control). */
	const FHoverSimState* Vehicle = nullptr;
	/** Impacts (walls and cars) this vehicle has counted so far; a change means it was hit. */
	int32 ImpactCount = 0;
	/** For effects on other cars; null when the level has no race. */
	URaceTrackerComponent* Tracker = nullptr;

	/**
	 * Puts a modifier on ANOTHER driver's car. It starts in that car's next physics step, so the
	 * result does not depend on which car stepped first. Returns false when that car is gone.
	 */
	bool QueueModifierOn(int32 OtherRacerId, const FVehicleModifierSpec& Spec) const;
};

/**
 * On/off switch with a short delay in both directions, for passives that follow a race condition:
 * the condition must hold (or stop holding) for the delay before the state changes, so it never flickers.
 */
struct FPassiveSwitch
{
	bool bOn = false;
	float PendingTime = 0.f;

	/** Returns true in the step the state changes. */
	bool Update(bool bWanted, float DeltaTime, float OnDelay, float OffDelay)
	{
		if (bWanted == bOn)
		{
			PendingTime = 0.f;
			return false;
		}
		PendingTime += DeltaTime;
		if (PendingTime < (bWanted ? OnDelay : OffDelay))
		{
			return false;
		}
		bOn = bWanted;
		PendingTime = 0.f;
		return true;
	}

	void Reset()
	{
		bOn = false;
		PendingTime = 0.f;
	}
};

UCLASS(Abstract, BlueprintType)
class COSMICHOTSHOTSGAME_API UPassiveDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Runtime class that implements this passive. */
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const PURE_VIRTUAL(UPassiveDefinition::GetPassiveClass, return nullptr;);

	/** Switch this one passive off without code changes (testing, balancing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bPassiveEnabled = true;

	/** Short name for debug display and, later, the HUD indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive")
	FText DisplayName;
};

UCLASS(Abstract)
class COSMICHOTSHOTSGAME_API UPassiveBase : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const UPassiveDefinition* InDefinition) { Definition = InDefinition; }
	const UPassiveDefinition* GetDefinition() const { return Definition; }

	/** One fixed physics step. */
	virtual void Step(const FPassiveStepContext& Context) PURE_VIRTUAL(UPassiveBase::Step, );

	/** Passive switched off or replaced: let go of everything it added (modifiers fade out). */
	virtual void Deactivate(FVehicleModifierStack& Modifiers) PURE_VIRTUAL(UPassiveBase::Deactivate, );

	/** One line for the dev HUD and the log: is it active, how strong, how long. */
	virtual FString GetDebugString() const PURE_VIRTUAL(UPassiveBase::GetDebugString, return FString(););

	/** State without running timers; the log gets one line whenever this text changes. */
	virtual FString GetLogState() const PURE_VIRTUAL(UPassiveBase::GetLogState, return FString(););

protected:
	UPROPERTY(Transient)
	TObjectPtr<const UPassiveDefinition> Definition;
};
