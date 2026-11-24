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
struct FRaceStepView;

/** What a passive sees in one fixed step. */
struct FPassiveStepContext
{
	/** Race state of the previous physics step (ranks, gaps, events). */
	const FRaceStepView* Race = nullptr;
	/** This vehicle's driver id in Race->Racers, INDEX_NONE while it is not in the race yet. */
	int32 RacerId = INDEX_NONE;
	float DeltaTime = 0.f;
	/** The vehicle's own modifier stack. */
	FVehicleModifierStack* Modifiers = nullptr;
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
