// Vehicle modifier stack: temporary stat bonuses and penalties on one vehicle.
//
// The shared system for passives, item power-ups and (later) active abilities. Each
// modifier names its source, the stat it changes, a value, an optional duration and
// fade-in / fade-out times, so a bonus can grow in and decay gradually instead of
// switching on and off. The stack is plain C++ and advances in the fixed physics step,
// so the driving feel stays identical at every frame rate.
//
// Effects on ANOTHER vehicle must go through Queue(): the order in which vehicles run
// their physics step is not fixed, so a queued modifier only starts in the target's next
// step, whichever vehicle happened to step first.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/VehicleStatModifiers.h"
#include "VehicleModifierStack.generated.h"

UENUM(BlueprintType)
enum class EVehicleStat : uint8
{
	MaxSpeed,
	Acceleration,
	Steering,
	Grip,
	RamPower,
	Stability
};

UENUM(BlueprintType)
enum class EVehicleModifierOp : uint8
{
	/** Value is a multiplier: 1.06 = +6 %. */
	Multiply,
	/** Value is added to the multiplier: 0.06 = +6 % (several add up before multiplying). */
	Add
};

/** What a source asks for. One modifier exists per (SourceId, Stat). */
struct FVehicleModifierSpec
{
	FName SourceId;
	EVehicleStat Stat = EVehicleStat::MaxSpeed;
	EVehicleModifierOp Op = EVehicleModifierOp::Multiply;
	float Value = 1.f;
	/** Seconds at full effect before it starts to fade out. Negative = until removed. */
	float Duration = -1.f;
	/** Seconds from nothing to full effect. 0 = at once. */
	float FadeIn = 0.f;
	/** Seconds from full effect to nothing. 0 = at once. */
	float FadeOut = 0.f;
};

/** A modifier in the stack. */
struct FVehicleModifier
{
	FVehicleModifierSpec Spec;
	/** Seconds since it was added or last refreshed. */
	float Elapsed = 0.f;
	/** 0..1: how much of Value applies right now (fade in / fade out). */
	float Strength = 0.f;
	/** Fading out; removed when Strength reaches 0. */
	bool bEnding = false;

	/** Seconds until it starts to fade out; negative = no time limit. */
	float GetTimeRemaining() const { return Spec.Duration < 0.f ? -1.f : FMath::Max(Spec.Duration - Elapsed, 0.f); }
};

class COSMICHOTSHOTSGAME_API FVehicleModifierStack
{
public:
	/**
	 * Adds the modifier, or updates the one with the same source and stat: new value and times,
	 * duration restarted, fade-out cancelled. Its current strength is kept, so a refresh never jumps.
	 */
	void AddOrRefresh(const FVehicleModifierSpec& Spec);

	/** Starts fading out every modifier of this source. */
	void Remove(FName SourceId);

	/** Removes everything at once (character change, reset). */
	void Clear();

	/** For effects on another vehicle: starts in the first step whose SimTime is later than QueuedAtSimTime. */
	void Queue(const FVehicleModifierSpec& Spec, float QueuedAtSimTime);

	/** One fixed step: starts queued modifiers, runs durations and fades, recomputes the totals. */
	void Advance(float DeltaTime, float SimTime);

	bool HasSource(FName SourceId) const;

	/** Combined multipliers of everything in the stack (1 = no change). Mass is never modified. */
	const FVehicleStatModifiers& GetTotals() const { return Totals; }

	const TArray<FVehicleModifier>& GetModifiers() const { return Modifiers; }

private:
	struct FQueuedModifier
	{
		FVehicleModifierSpec Spec;
		float QueuedAtSimTime = 0.f;
	};

	void RecomputeTotals();

	TArray<FVehicleModifier> Modifiers;
	TArray<FQueuedModifier> Queued;
	FVehicleStatModifiers Totals;
};
