#include "Ability/ActiveAbility.h"

#include "Ability/VehicleAbilityComponent.h"
#include "Race/RaceTrackerComponent.h"
#include "Race/TrackSplineTable.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierComponent.h"
#include "Vehicle/VehicleModifierStack.h"

namespace
{
	// One modifier source per ability, so its speed changes show up (and end) as one thing.
	const FName FreyaAbilitySource(TEXT("Ability.Freya"));
	const FName SulgwenAbilitySource(TEXT("Ability.Sulgwen"));
	const FName KilljokeAbilitySource(TEXT("Ability.Killjoke"));

	const int32 LeaderRank = 1;

	FVehicleModifierSpec MakeAbilityModifier(FName Source, EVehicleStat Stat, float Value, float Duration, float FadeOut)
	{
		FVehicleModifierSpec Spec;
		Spec.SourceId = Source;
		Spec.Stat = Stat;
		Spec.Value = Value;
		Spec.Duration = Duration;
		Spec.FadeOut = FadeOut;
		return Spec;
	}
}

// ---------------------------------------------------------------------------- context

const FRaceStepRacer& FAbilityContext::Self() const
{
	return Race->Racers[RacerId];
}

bool FAbilityContext::IsRival(int32 OtherId) const
{
	return OtherId != RacerId && Race->Racers.IsValidIndex(OtherId) && Race->Racers[OtherId].bInRace && !Race->Racers[OtherId].bFinished;
}

float FAbilityContext::GetAheadDistance(int32 OtherId) const
{
	return Track->WrappedDelta(Self().TrackDistance, Race->Racers[OtherId].TrackDistance);
}

int32 FAbilityContext::FindNearestRivalAhead(float MaxDistance) const
{
	int32 Nearest = INDEX_NONE;
	float NearestAhead = MaxDistance;
	for (int32 OtherId = 0; OtherId < Race->Racers.Num(); ++OtherId)
	{
		if (!IsRival(OtherId))
		{
			continue;
		}
		const float Ahead = GetAheadDistance(OtherId);
		if (Ahead > 0.f && Ahead <= NearestAhead)
		{
			Nearest = OtherId;
			NearestAhead = Ahead;
		}
	}
	return Nearest;
}

bool FAbilityContext::QueueModifierOn(int32 OtherId, const FVehicleModifierSpec& Spec) const
{
	const ARaceVehicle* Other = Tracker ? Tracker->GetRacerVehicle(OtherId) : nullptr;
	if (!Other || !Other->GetModifiers())
	{
		return false;
	}
	Other->GetModifiers()->GetStack().Queue(Spec, SimTime);
	return true;
}

bool FAbilityContext::QueueEffectOn(int32 OtherId, FAbilityRemoteEffect Effect) const
{
	const ARaceVehicle* Other = Tracker ? Tracker->GetRacerVehicle(OtherId) : nullptr;
	if (!Other || !Other->GetAbility())
	{
		return false;
	}
	Effect.QueuedAtSimTime = SimTime;
	Other->GetAbility()->QueueEffect(Effect);
	return true;
}

// ---------------------------------------------------------------------------- Freya

TSubclassOf<UActiveAbilityBase> UFreyaSurgeDefinition::GetAbilityClass() const
{
	return UFreyaSurgeAbility::StaticClass();
}

void UFreyaSurgeAbility::Activate(const FAbilityContext& Context)
{
	const UFreyaSurgeDefinition* Def = GetDefinitionAs<UFreyaSurgeDefinition>();
	Context.Modifiers->AddOrRefresh(MakeAbilityModifier(FreyaAbilitySource, EVehicleStat::Acceleration, Def->SelfAcceleration, Def->ActiveTime, Def->FadeOut));
	Context.Modifiers->AddOrRefresh(MakeAbilityModifier(FreyaAbilitySource, EVehicleStat::MaxSpeed, Def->SelfMaxSpeed, Def->ActiveTime, Def->FadeOut));

	// Everybody else gets a weaker burst at the same moment.
	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (Context.IsRival(OtherId))
		{
			Context.QueueModifierOn(OtherId, MakeAbilityModifier(FreyaAbilitySource, EVehicleStat::Acceleration, Def->OthersAcceleration, Def->OthersDuration, Def->FadeOut));
			Context.QueueModifierOn(OtherId, MakeAbilityModifier(FreyaAbilitySource, EVehicleStat::MaxSpeed, Def->OthersMaxSpeed, Def->OthersDuration, Def->FadeOut));
		}
	}
}

// ---------------------------------------------------------------------------- Sulgwen

TSubclassOf<UActiveAbilityBase> USulgwenSurgeDefinition::GetAbilityClass() const
{
	return USulgwenSurgeAbility::StaticClass();
}

bool USulgwenSurgeAbility::CanActivate(const FAbilityContext& Context) const
{
	// Blocked while she leads: there is nobody to disrupt.
	return Context.Self().Rank != LeaderRank;
}

void USulgwenSurgeAbility::Activate(const FAbilityContext& Context)
{
	const USulgwenSurgeDefinition* Def = GetDefinitionAs<USulgwenSurgeDefinition>();
	Context.Modifiers->AddOrRefresh(MakeAbilityModifier(SulgwenAbilitySource, EVehicleStat::Acceleration, Def->SelfAcceleration, Def->ActiveTime, Def->FadeOut));
	Context.Modifiers->AddOrRefresh(MakeAbilityModifier(SulgwenAbilitySource, EVehicleStat::MaxSpeed, Def->SelfMaxSpeed, Def->ActiveTime, Def->FadeOut));

	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (Context.IsRival(OtherId) && Context.Race->Racers[OtherId].Rank == LeaderRank)
		{
			Context.QueueModifierOn(OtherId, MakeAbilityModifier(SulgwenAbilitySource, EVehicleStat::MaxSpeed, Def->LeaderMaxSpeed, Def->LeaderDuration, Def->FadeOut));
		}
	}
}

// ---------------------------------------------------------------------------- Killjoke

TSubclassOf<UActiveAbilityBase> UKilljokeDisruptionDefinition::GetAbilityClass() const
{
	return UKilljokeDisruptionAbility::StaticClass();
}

bool UKilljokeDisruptionAbility::CanActivate(const FAbilityContext& Context) const
{
	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (Context.IsRival(OtherId) && Context.Race->Racers[OtherId].Rank < Context.Self().Rank)
		{
			return true;
		}
	}
	return false;
}

void UKilljokeDisruptionAbility::Activate(const FAbilityContext& Context)
{
	const UKilljokeDisruptionDefinition* Def = GetDefinitionAs<UKilljokeDisruptionDefinition>();
	// First prototype: every rival ahead suffers the same effect.
	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (Context.IsRival(OtherId) && Context.Race->Racers[OtherId].Rank < Context.Self().Rank)
		{
			Context.QueueModifierOn(OtherId, MakeAbilityModifier(KilljokeAbilitySource, EVehicleStat::MaxSpeed, Def->RivalMaxSpeed, Def->RivalDuration, Def->FadeOut));
		}
	}
}

// ---------------------------------------------------------------------------- Glick

TSubclassOf<UActiveAbilityBase> UGlickMagnetDefinition::GetAbilityClass() const
{
	return UGlickMagnetAbility::StaticClass();
}

bool UGlickMagnetAbility::IsInRange(const FAbilityContext& Context, int32 OtherId) const
{
	if (!Context.IsRival(OtherId))
	{
		return false;
	}
	const float Ahead = Context.GetAheadDistance(OtherId);
	const float Sideways = Context.Race->Racers[OtherId].LateralOffset - Context.Self().LateralOffset;
	return FMath::Sqrt(Ahead * Ahead + Sideways * Sideways) <= GetDefinitionAs<UGlickMagnetDefinition>()->Range;
}

bool UGlickMagnetAbility::CanActivate(const FAbilityContext& Context) const
{
	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (IsInRange(Context, OtherId))
		{
			return true;
		}
	}
	return false;
}

void UGlickMagnetAbility::Activate(const FAbilityContext& Context)
{
	const UGlickMagnetDefinition* Def = GetDefinitionAs<UGlickMagnetDefinition>();
	const FTrackFrame OwnFrame = Context.Track->GetFrameAtDistance(Context.Self().TrackDistance);
	const FVector OwnPlace = OwnFrame.Location + OwnFrame.Right * Context.Self().LateralOffset;

	// Each car in range is pulled towards where Glick is now: a car ahead is held back, one behind is
	// drawn closer, one beside is drawn sideways.
	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (!IsInRange(Context, OtherId))
		{
			continue;
		}
		const FRaceStepRacer& Other = Context.Race->Racers[OtherId];
		const FTrackFrame OtherFrame = Context.Track->GetFrameAtDistance(Other.TrackDistance);
		const FVector OtherPlace = OtherFrame.Location + OtherFrame.Right * Other.LateralOffset;

		FAbilityRemoteEffect Pull;
		Pull.Acceleration = (OwnPlace - OtherPlace).GetSafeNormal2D() * Def->PullAcceleration;
		Pull.Duration = Def->ActiveTime;
		Context.QueueEffectOn(OtherId, Pull);
	}
}

// ---------------------------------------------------------------------------- Spark

TSubclassOf<UActiveAbilityBase> USparkTeleportDefinition::GetAbilityClass() const
{
	return USparkTeleportAbility::StaticClass();
}

int32 USparkTeleportAbility::FindTarget(const FAbilityContext& Context) const
{
	const USparkTeleportDefinition* Def = GetDefinitionAs<USparkTeleportDefinition>();
	const int32 Target = Context.FindNearestRivalAhead(Def->Range);
	// The spot behind the rival must still be ahead of Spark: no teleporting backwards.
	if (Target == INDEX_NONE || Context.GetAheadDistance(Target) <= Def->BehindDistance)
	{
		return INDEX_NONE;
	}

	// The spot must be free of other cars.
	const FRaceStepRacer& Rival = Context.Race->Racers[Target];
	const float SpotDistance = Rival.TrackDistance - Def->BehindDistance;
	for (int32 OtherId = 0; OtherId < Context.Race->Racers.Num(); ++OtherId)
	{
		if (!Context.IsRival(OtherId))
		{
			continue;
		}
		const FRaceStepRacer& Other = Context.Race->Racers[OtherId];
		if (FMath::Abs(Context.Track->WrappedDelta(SpotDistance, Other.TrackDistance)) < Def->FreeLength
			&& FMath::Abs(Other.LateralOffset - Rival.LateralOffset) < Def->FreeWidth)
		{
			return INDEX_NONE;
		}
	}
	return Target;
}

bool USparkTeleportAbility::CanActivate(const FAbilityContext& Context) const
{
	return FindTarget(Context) != INDEX_NONE;
}

void USparkTeleportAbility::Activate(const FAbilityContext& Context)
{
	const int32 Target = FindTarget(Context);
	if (Target == INDEX_NONE)
	{
		return;
	}
	// Behind the rival, on the rival's line; he still has to overtake by driving.
	FAbilityRemoteEffect Move;
	Move.bRelocate = true;
	Move.TrackDistance = Context.Race->Racers[Target].TrackDistance - GetDefinitionAs<USparkTeleportDefinition>()->BehindDistance;
	Move.Lateral = Context.Race->Racers[Target].LateralOffset;
	Context.QueueEffectOn(Context.RacerId, Move);
}

// ---------------------------------------------------------------------------- Wilqis

TSubclassOf<UActiveAbilityBase> UWilqisSwapDefinition::GetAbilityClass() const
{
	return UWilqisSwapAbility::StaticClass();
}

bool UWilqisSwapAbility::CanActivate(const FAbilityContext& Context) const
{
	return Context.FindNearestRivalAhead(GetDefinitionAs<UWilqisSwapDefinition>()->Range) != INDEX_NONE;
}

void UWilqisSwapAbility::Activate(const FAbilityContext& Context)
{
	const int32 Target = Context.FindNearestRivalAhead(GetDefinitionAs<UWilqisSwapDefinition>()->Range);
	if (Target == INDEX_NONE)
	{
		return;
	}
	// Both cars move in their NEXT step, so they are never in the same place: each takes the other's
	// spot on the track, aligned with the track, with its own speed.
	const FRaceStepRacer& Rival = Context.Race->Racers[Target];
	FAbilityRemoteEffect ToRivalsPlace;
	ToRivalsPlace.bRelocate = true;
	ToRivalsPlace.TrackDistance = Rival.TrackDistance;
	ToRivalsPlace.Lateral = Rival.LateralOffset;

	FAbilityRemoteEffect ToOwnPlace;
	ToOwnPlace.bRelocate = true;
	ToOwnPlace.TrackDistance = Context.Self().TrackDistance;
	ToOwnPlace.Lateral = Context.Self().LateralOffset;

	Context.QueueEffectOn(Context.RacerId, ToRivalsPlace);
	Context.QueueEffectOn(Target, ToOwnPlace);
}
