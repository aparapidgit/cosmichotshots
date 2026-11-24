#include "Vehicle/VehicleModifierStack.h"

namespace
{
	constexpr int32 NumStats = static_cast<int32>(EVehicleStat::HitSpeedLoss) + 1;

	/** No stat is ever multiplied down to nothing, whatever is stacked. */
	constexpr float MinTotalMultiplier = 0.1f;
}

void FVehicleModifierStack::AddOrRefresh(const FVehicleModifierSpec& Spec)
{
	FVehicleModifier* Existing = Modifiers.FindByPredicate([&Spec](const FVehicleModifier& Modifier)
	{
		return Modifier.Spec.SourceId == Spec.SourceId && Modifier.Spec.Stat == Spec.Stat;
	});

	if (!Existing)
	{
		Existing = &Modifiers.AddDefaulted_GetRef();
		Existing->Strength = Spec.FadeIn > 0.f ? 0.f : 1.f;
	}
	Existing->Spec = Spec;
	Existing->Elapsed = 0.f;
	Existing->bEnding = false;
	RecomputeTotals();
}

void FVehicleModifierStack::Remove(FName SourceId, float FadeOutOverride)
{
	for (FVehicleModifier& Modifier : Modifiers)
	{
		if (Modifier.Spec.SourceId == SourceId)
		{
			Modifier.bEnding = true;
			if (FadeOutOverride >= 0.f)
			{
				Modifier.Spec.FadeOut = FadeOutOverride;
			}
		}
	}
}

void FVehicleModifierStack::Clear()
{
	Modifiers.Reset();
	Queued.Reset();
	RecomputeTotals();
}

void FVehicleModifierStack::Queue(const FVehicleModifierSpec& Spec, float QueuedAtSimTime)
{
	FQueuedModifier& Entry = Queued.AddDefaulted_GetRef();
	Entry.Spec = Spec;
	Entry.QueuedAtSimTime = QueuedAtSimTime;
}

bool FVehicleModifierStack::HasSource(FName SourceId) const
{
	return Modifiers.ContainsByPredicate([SourceId](const FVehicleModifier& Modifier) { return Modifier.Spec.SourceId == SourceId; });
}

void FVehicleModifierStack::Advance(float DeltaTime, float SimTime)
{
	// Queued by another vehicle in an earlier step: start now. (Same step = wait for the next one.)
	for (int32 Index = 0; Index < Queued.Num();)
	{
		if (Queued[Index].QueuedAtSimTime < SimTime)
		{
			const FVehicleModifierSpec Spec = Queued[Index].Spec;
			Queued.RemoveAt(Index);
			AddOrRefresh(Spec);
		}
		else
		{
			++Index;
		}
	}

	for (int32 Index = Modifiers.Num() - 1; Index >= 0; --Index)
	{
		FVehicleModifier& Modifier = Modifiers[Index];
		Modifier.Elapsed += DeltaTime;
		if (Modifier.Spec.Duration >= 0.f && Modifier.Elapsed >= Modifier.Spec.Duration)
		{
			Modifier.bEnding = true;
		}

		if (Modifier.bEnding)
		{
			Modifier.Strength = Modifier.Spec.FadeOut > 0.f ? Modifier.Strength - DeltaTime / Modifier.Spec.FadeOut : 0.f;
			if (Modifier.Strength <= 0.f)
			{
				// Keep the order of the others: totals multiply in list order.
				Modifiers.RemoveAt(Index);
			}
		}
		else
		{
			Modifier.Strength = Modifier.Spec.FadeIn > 0.f ? FMath::Min(Modifier.Strength + DeltaTime / Modifier.Spec.FadeIn, 1.f) : 1.f;
		}
	}

	RecomputeTotals();
}

void FVehicleModifierStack::RecomputeTotals()
{
	// Per stat: (1 + sum of Add modifiers) x product of Multiply modifiers.
	float Added[NumStats];
	float Multiplied[NumStats];
	for (int32 Index = 0; Index < NumStats; ++Index)
	{
		Added[Index] = 1.f;
		Multiplied[Index] = 1.f;
	}

	for (const FVehicleModifier& Modifier : Modifiers)
	{
		const int32 Index = static_cast<int32>(Modifier.Spec.Stat);
		if (Modifier.Spec.Op == EVehicleModifierOp::Add)
		{
			Added[Index] += Modifier.Spec.Value * Modifier.Strength;
		}
		else
		{
			Multiplied[Index] *= 1.f + (Modifier.Spec.Value - 1.f) * Modifier.Strength;
		}
	}

	auto Total = [&Added, &Multiplied](EVehicleStat Stat)
	{
		const int32 Index = static_cast<int32>(Stat);
		return FMath::Max(Added[Index] * Multiplied[Index], MinTotalMultiplier);
	};

	Totals = FVehicleStatModifiers();
	Totals.MaxSpeed = Total(EVehicleStat::MaxSpeed);
	Totals.Acceleration = Total(EVehicleStat::Acceleration);
	Totals.Steering = Total(EVehicleStat::Steering);
	Totals.Grip = Total(EVehicleStat::Grip);
	Totals.RamPower = Total(EVehicleStat::RamPower);
	Totals.Stability = Total(EVehicleStat::Stability);

	ImpactTotals.SideRamPower = Total(EVehicleStat::SideRamPower);
	ImpactTotals.SideSpeedLoss = Total(EVehicleStat::SideSpeedLoss);
	ImpactTotals.HitRecoveryTime = Total(EVehicleStat::HitRecoveryTime);
	ImpactTotals.HitSpeedLoss = Total(EVehicleStat::HitSpeedLoss);
}
