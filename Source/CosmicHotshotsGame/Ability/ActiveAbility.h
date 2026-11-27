// Active abilities: what a character does when its driver presses the ability button.
//
// Built like the passives. UActiveAbilityDefinition is the data asset a UCharacterDefinition
// points at (every number of the ability). UActiveAbilityBase is the runtime object, one per
// vehicle, owned by UVehicleAbilityComponent, which asks it two things inside the vehicle's fixed
// physics step: "can you fire now?" (CanActivate) and "fire" (Activate). The component handles
// the cost (one whole Energy Core segment, only when the ability really fires), the cooldown and
// the button; an ability only says what it does.
//
// Abilities act through systems that exist: the modifier stack (also on OTHER cars, queued so it
// starts in their next step) and two things the component offers for other cars: a pull or push
// for a while, and moving a car to another place on the track. Positions, "ahead" and ranges are
// measured along the track line. All values are placeholders to be tuned by playtesting.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActiveAbility.generated.h"

class FTrackSplineTable;
class FVehicleModifierStack;
class UActiveAbilityBase;
class URaceTrackerComponent;
struct FRaceStepRacer;
struct FRaceStepView;
struct FVehicleModifierSpec;

/** Something an ability does to a car that is applied by THAT car, in its next physics step. */
struct FAbilityRemoteEffect
{
	float QueuedAtSimTime = 0.f;
	/** Move the car to this place on the track (distance along the line, sideways offset), aligned with the track, speed kept. */
	bool bRelocate = false;
	float TrackDistance = 0.f;
	float Lateral = 0.f;
	/** A pull or push (cm/s^2, world space) for Duration seconds. */
	FVector Acceleration = FVector::ZeroVector;
	float Duration = 0.f;
};

/** What an ability sees when it is asked whether it can fire, and when it fires. */
struct FAbilityContext
{
	/** Race state after the previous physics step. Never null when an ability is asked. */
	const FRaceStepView* Race = nullptr;
	int32 RacerId = INDEX_NONE;
	float SimTime = 0.f;
	/** This car's own modifier stack. */
	FVehicleModifierStack* Modifiers = nullptr;
	URaceTrackerComponent* Tracker = nullptr;
	const FTrackSplineTable* Track = nullptr;

	const FRaceStepRacer& Self() const;

	/** Is this other driver a car still racing (on the track, not finished)? */
	bool IsRival(int32 OtherId) const;

	/** How far the other car is ahead of this one along the track (cm, negative = behind). */
	float GetAheadDistance(int32 OtherId) const;

	/** Nearest rival ahead along the track within MaxDistance (cm), or INDEX_NONE. */
	int32 FindNearestRivalAhead(float MaxDistance) const;

	/** Puts a modifier on another driver's car; it starts in that car's next step. */
	bool QueueModifierOn(int32 OtherId, const FVehicleModifierSpec& Spec) const;

	/** A pull, push or move for a car (this one included); that car applies it in its next step. */
	bool QueueEffectOn(int32 OtherId, FAbilityRemoteEffect Effect) const;
};

UCLASS(Abstract, BlueprintType)
class COSMICHOTSHOTSGAME_API UActiveAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const PURE_VIRTUAL(UActiveAbilityDefinition::GetAbilityClass, return nullptr;);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	FText DisplayName;

	/** One or two sentences for the character select screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (MultiLine = "true"))
	FText Description;

	/** Seconds the ability counts as running after it fired (HUD glow; the cooldown starts when it ends). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0.0"))
	float ActiveTime = 2.f;

	/** Seconds after the ability ended before it can be used again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0.0"))
	float Cooldown = 2.f;

	/** Computer drivers fire the ability this long (s, somewhere between the two, by driver) after it became possible. Simple rule until the second AI pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0.0"))
	float AIUseDelayMin = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0.0"))
	float AIUseDelayMax = 4.f;

	/** Seconds the speed changes of the ability take to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0.0"))
	float FadeOut = 0.5f;
};

UCLASS(Abstract)
class COSMICHOTSHOTSGAME_API UActiveAbilityBase : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const UActiveAbilityDefinition* InDefinition) { Definition = InDefinition; }
	const UActiveAbilityDefinition* GetDefinition() const { return Definition; }

	/** Is the ability's own condition met right now? (Energy and cooldown are not its business.) */
	virtual bool CanActivate(const FAbilityContext& Context) const { return true; }

	/** Fire. Called only when CanActivate said yes in the same step. */
	virtual void Activate(const FAbilityContext& Context) PURE_VIRTUAL(UActiveAbilityBase::Activate, );

protected:
	template <typename DefinitionType>
	const DefinitionType* GetDefinitionAs() const { return CastChecked<DefinitionType>(Definition); }

private:
	UPROPERTY(Transient)
	TObjectPtr<const UActiveAbilityDefinition> Definition;
};

// ---------------------------------------------------------------------------- Freya: shared surge

/** Freya: a strong, short burst of acceleration for her; a weaker one for everybody else at the same moment. */
UCLASS()
class COSMICHOTSHOTSGAME_API UFreyaSurgeDefinition : public UActiveAbilityDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared surge", meta = (ClampMin = "1.0"))
	float SelfAcceleration = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared surge", meta = (ClampMin = "1.0"))
	float SelfMaxSpeed = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared surge", meta = (ClampMin = "1.0"))
	float OthersAcceleration = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared surge", meta = (ClampMin = "1.0"))
	float OthersMaxSpeed = 1.05f;

	/** Seconds the others' burst lasts (hers lasts ActiveTime). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared surge", meta = (ClampMin = "0.0"))
	float OthersDuration = 1.5f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UFreyaSurgeAbility : public UActiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual void Activate(const FAbilityContext& Context) override;
};

// ---------------------------------------------------------------------------- Sulgwen: surge with leader disruption

/** Sulgwen: a short burst for her, and the current leader briefly loses part of their speed. Blocked while she leads. */
UCLASS()
class COSMICHOTSHOTSGAME_API USulgwenSurgeDefinition : public UActiveAbilityDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surge", meta = (ClampMin = "1.0"))
	float SelfAcceleration = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surge", meta = (ClampMin = "1.0"))
	float SelfMaxSpeed = 1.12f;

	/** Top speed multiplier on the leader (below 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surge", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LeaderMaxSpeed = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surge", meta = (ClampMin = "0.0"))
	float LeaderDuration = 1.5f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API USulgwenSurgeAbility : public UActiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate(const FAbilityContext& Context) const override;
	virtual void Activate(const FAbilityContext& Context) override;
};

// ---------------------------------------------------------------------------- Killjoke: drive disruption

/** Killjoke: every rival AHEAD of him (by race place) briefly loses part of their speed; they keep control. */
UCLASS()
class COSMICHOTSHOTSGAME_API UKilljokeDisruptionDefinition : public UActiveAbilityDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Disruption", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RivalMaxSpeed = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Disruption", meta = (ClampMin = "0.0"))
	float RivalDuration = 1.5f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UKilljokeDisruptionAbility : public UActiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate(const FAbilityContext& Context) const override;
	virtual void Activate(const FAbilityContext& Context) override;
};

// ---------------------------------------------------------------------------- Glick: magnetic field

/** Glick: briefly pulls nearby cars towards himself (those ahead slow down, those behind close in, those beside move over). */
UCLASS()
class COSMICHOTSHOTSGAME_API UGlickMagnetDefinition : public UActiveAbilityDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const override;

	/** Cars within this distance (cm, along and across the track) are pulled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic field", meta = (ClampMin = "100.0"))
	float Range = 2500.f;

	/** Pull (cm/s^2) for ActiveTime seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic field", meta = (ClampMin = "0.0"))
	float PullAcceleration = 1500.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UGlickMagnetAbility : public UActiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate(const FAbilityContext& Context) const override;
	virtual void Activate(const FAbilityContext& Context) override;

private:
	bool IsInRange(const FAbilityContext& Context, int32 OtherId) const;
};

// ---------------------------------------------------------------------------- Spark: teleport

/** Spark: appears a safe distance behind the nearest rival ahead, if that rival is in range and the spot is free. */
UCLASS()
class COSMICHOTSHOTSGAME_API USparkTeleportDefinition : public UActiveAbilityDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const override;

	/** The rival must be at most this far ahead (cm along the track). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "100.0"))
	float Range = 6000.f;

	/** Spark comes out this far (cm) behind the rival, on the rival's line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "100.0"))
	float BehindDistance = 900.f;

	/** No other car may be within this length (cm along the track) and width (cm across) of the spot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "0.0"))
	float FreeLength = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "0.0"))
	float FreeWidth = 300.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API USparkTeleportAbility : public UActiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate(const FAbilityContext& Context) const override;
	virtual void Activate(const FAbilityContext& Context) override;

private:
	/** The rival to appear behind, or INDEX_NONE when the teleport is not possible now. */
	int32 FindTarget(const FAbilityContext& Context) const;
};

// ---------------------------------------------------------------------------- Wilqis: position swap

/** Wilqis: swaps places with the nearest rival ahead within a short range; both keep their own speed. */
UCLASS()
class COSMICHOTSHOTSGAME_API UWilqisSwapDefinition : public UActiveAbilityDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UActiveAbilityBase> GetAbilityClass() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position swap", meta = (ClampMin = "100.0"))
	float Range = 3000.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UWilqisSwapAbility : public UActiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate(const FAbilityContext& Context) const override;
	virtual void Activate(const FAbilityContext& Context) override;
};
