#include "Vehicle/VehicleModifierStack.h"

namespace
{
	float& GetStat(FVehicleStatModifiers& Stats, EVehicleStat Stat)
	{
		switch (Stat)
		{
		case EVehicleStat::Acceleration: return Stats.Acceleration;
		case EVehicleStat::Steering: return Stats.Steering;
		case EVehicleStat::Grip: return Stats.Grip;
		case EVehicleStat::RamPower: return Stats.RamPower;
		case EVehicleStat::Stability: return Stats.Stability;
		case EVehicleStat::MaxSpeed:
		default: return Stats.MaxSpeed;
		}
	}

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

void FVehicleModifierStack::Remove(FName SourceId)
{
	for (FVehicleModifier& Modifier : Modifiers)
	{
		if (Modifier.Spec.SourceId == SourceId)
		{
			Modifier.bEnding = true;
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
	FVehicleStatModifiers Added;      // sums of Add modifiers, starting from 1
	FVehicleStatModifiers Multiplied; // products of Multiply modifiers
	for (const FVehicleModifier& Modifier : Modifiers)
	{
		if (Modifier.Spec.Op == EVehicleModifierOp::Add)
		{
			GetStat(Added, Modifier.Spec.Stat) += Modifier.Spec.Value * Modifier.Strength;
		}
		else
		{
			GetStat(Multiplied, Modifier.Spec.Stat) *= 1.f + (Modifier.Spec.Value - 1.f) * Modifier.Strength;
		}
	}

	Totals = FVehicleStatModifiers();
	for (const EVehicleStat Stat : { EVehicleStat::MaxSpeed, EVehicleStat::Acceleration, EVehicleStat::Steering,
		EVehicleStat::Grip, EVehicleStat::RamPower, EVehicleStat::Stability })
	{
		GetStat(Totals, Stat) = FMath::Max(GetStat(Added, Stat) * GetStat(Multiplied, Stat), MinTotalMultiplier);
	}
}
