#include "Zone/VehicleZoneComponent.h"

#include "CosmicHotshotsGame.h"
#include "Race/RaceTrackerComponent.h"
#include "Race/TrackSplineTable.h"
#include "Vehicle/HoverSim.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierStack.h"
#include "Zone/TrackZoneDefinition.h"
#include "Zone/TrackZoneTable.h"

namespace
{
	// Track projection of the car (same idea as the race tracker): search near the last position, whole lap when lost.
	constexpr float ZoneProjectionWindow = 3000.f;
	constexpr float ZoneReacquireDistance = 2500.f;

	/** A meter with less than this in it counts as empty (Gravity Abyss). */
	constexpr float ZoneEmptyCharge = 0.01f;
	/** While the car sits in a zone its speed modifiers are renewed with at least this duration (s), so they never lapse between steps. */
	constexpr float ZoneMinHoldDuration = 0.1f;

	const TCHAR* ZoneTypeName(ETrackZoneType Type)
	{
		switch (Type)
		{
		case ETrackZoneType::MeteorCluster: return TEXT("MeteorCluster");
		case ETrackZoneType::SpectralTower: return TEXT("SpectralTower");
		case ETrackZoneType::Magnetonium: return TEXT("Magnetonium");
		case ETrackZoneType::EnergyDock: return TEXT("EnergyDock");
		case ETrackZoneType::DustVortex: return TEXT("DustVortex");
		case ETrackZoneType::TurboHub: return TEXT("TurboHub");
		case ETrackZoneType::GravityAbyss: return TEXT("GravityAbyss");
		case ETrackZoneType::WarpGate: return TEXT("WarpGate");
		case ETrackZoneType::ArrowsPad: return TEXT("ArrowsPad");
		default: return TEXT("Zone");
		}
	}

	void AddZoneModifier(FVehicleModifierStack* Modifiers, FName Source, EVehicleStat Stat, float Value, float Duration, float FadeOut)
	{
		if (Modifiers && !FMath::IsNearlyEqual(Value, 1.f))
		{
			FVehicleModifierSpec Spec;
			Spec.SourceId = Source;
			Spec.Stat = Stat;
			Spec.Value = Value;
			Spec.Duration = FMath::Max(Duration, ZoneMinHoldDuration);
			Spec.FadeOut = FadeOut;
			Modifiers->AddOrRefresh(Spec);
		}
	}
}

UVehicleZoneComponent::UVehicleZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleZoneComponent::SetZones(TSharedPtr<const FTrackZoneTable> InZones, TSharedPtr<const FTrackSplineTable> InTrack, URaceTrackerComponent* InTracker)
{
	Zones = MoveTemp(InZones);
	Track = MoveTemp(InTrack);
	Tracker = InTracker;
	VehicleName = GetNameSafe(GetOwner());
	TrackDistance = -1.f;
	RacerId = INDEX_NONE;

	const int32 Count = Zones.IsValid() ? Zones->Zones.Num() : 0;
	States.Reset();
	States.SetNum(Count);
	Definitions.Reset();
	SourceNames.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Definitions.Add(Zones->Zones[Index].Definition);
		SourceNames.Add(FName(*FString::Printf(TEXT("Zone.%d"), Index)));
	}
}

FVehicleZoneStep UVehicleZoneComponent::StepZones(float DeltaTime, float SimTime, const FVector& Location, const FVector& Velocity,
	FHoverSimState& State, FVehicleModifierStack* Modifiers, int32 BoostSegments)
{
	FVehicleZoneStep Result;
	if (!Zones.IsValid() || Zones->Zones.IsEmpty() || !Track.IsValid() || !Track->IsValid())
	{
		return Result;
	}

	// Where the car is on the track.
	const FTrackProjection Projection = TrackDistance < 0.f
		? Track->Project(Location)
		: Track->ProjectNear(Location, TrackDistance, ZoneProjectionWindow, ZoneReacquireDistance);
	TrackDistance = Projection.Distance;
	const FVector Along = Projection.Direction.GetSafeNormal2D();
	const FVector Right(-Along.Y, Along.X, 0.0);

	// The race after the previous step: the clock of the meteors, and where the other cars are.
	const URaceTrackerComponent* RaceTracker = Tracker.Get();
	const FRaceStepView* Race = RaceTracker ? &RaceTracker->GetViewForStep(SimTime) : nullptr;
	if (RaceTracker && RacerId == INDEX_NONE)
	{
		RacerId = RaceTracker->FindRacerId(Cast<ARaceVehicle>(GetOwner()));
	}
	const double RaceTime = Race ? Race->RaceTime : 0.0;

	FDriftBoostState& Boost = State.DriftBoost;
	const float SegmentSize = FDriftBoostState::MaxBoostCharge / FMath::Max(BoostSegments, 1);

	for (int32 Index = 0; Index < Zones->Zones.Num(); ++Index)
	{
		const FTrackZoneEntry& Zone = Zones->Zones[Index];
		const UTrackZoneDefinition* Def = Zone.Definition;
		if (!Def)
		{
			continue;
		}
		FZoneState& Mine = States[Index];
		Mine.Cooldown = FMath::Max(Mine.Cooldown - DeltaTime, 0.f);

		// The car relative to the zone's centre: ahead of it along the track (cm), and to its right (cm).
		const float Ahead = Track->WrappedDelta(Zone.TrackDistance, TrackDistance);
		const float Side = Projection.LateralOffset - Zone.Lateral;
		const float FromCentre = FMath::Sqrt(Ahead * Ahead + Side * Side);
		const bool bRound = Def->Type == ETrackZoneType::Magnetonium || Def->Type == ETrackZoneType::DustVortex;
		const bool bInside = bRound ? FromCentre <= Def->Radius : (FMath::Abs(Ahead) <= Def->HalfLength && FMath::Abs(Side) <= Zone.HalfWidth);
		const bool bEntered = bInside && !Mine.bInside;
		Mine.bInside = bInside;
		const bool bReady = bInside && Mine.Cooldown <= 0.f;
		const FName Source = SourceNames[Index];

		switch (Def->Type)
		{
		case ETrackZoneType::TurboHub:
			// Strong acceleration and a higher top speed, kept up for Duration after the car left the pad.
			if (bInside)
			{
				AddZoneModifier(Modifiers, Source, EVehicleStat::Acceleration, Def->AccelerationMultiplier, Def->Duration, Def->FadeOut);
				AddZoneModifier(Modifiers, Source, EVehicleStat::MaxSpeed, Def->SpeedMultiplier, Def->Duration, Def->FadeOut);
			}
			if (bEntered)
			{
				UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: speed x%.2f acceleration x%.2f for %.1f s"),
					*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Def->SpeedMultiplier, Def->AccelerationMultiplier, Def->Duration);
			}
			break;

		case ETrackZoneType::EnergyDock:
			if (bReady)
			{
				const float Before = Boost.BoostCharge;
				Boost.BoostCharge = FMath::Min(Boost.BoostCharge + Def->Segments * SegmentSize, FDriftBoostState::MaxBoostCharge);
				Mine.Cooldown = Def->Cooldown;
				UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: meter %.3f -> %.3f"),
					*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Before, Boost.BoostCharge);
			}
			break;

		case ETrackZoneType::GravityAbyss:
			if (bReady)
			{
				Mine.Cooldown = Def->Cooldown;
				if (Boost.BoostCharge > ZoneEmptyCharge)
				{
					// Everything that was saved up is gone (a running boost ends with the empty meter).
					UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: meter %.3f -> 0"),
						*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Boost.BoostCharge);
					Boost.BoostCharge = 0.f;
				}
				else
				{
					// Nothing to take: the car is slowed and unsettled instead.
					AddZoneModifier(Modifiers, Source, EVehicleStat::MaxSpeed, Def->SpeedMultiplier, Def->Duration, Def->FadeOut);
					State.ControlLossTimeRemaining = FMath::Max(State.ControlLossTimeRemaining, Def->ControlLossTime);
					UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: empty meter, speed x%.2f for %.1f s, control loss %.2f s"),
						*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Def->SpeedMultiplier, Def->Duration, Def->ControlLossTime);
				}
			}
			break;

		case ETrackZoneType::Magnetonium:
		case ETrackZoneType::DustVortex:
			if (bInside)
			{
				// Pull towards the centre, stronger the closer the car is. AlongTrackShare below 1 makes it mostly sideways.
				const FVector Towards = (Along * (-Ahead * Def->AlongTrackShare) + Right * -Side).GetSafeNormal2D();
				Result.Acceleration += Towards * (Def->PullAcceleration * (1.f - FromCentre / Def->Radius));

				if (Def->Type == ETrackZoneType::DustVortex)
				{
					// The dust slows the car; a car that is boosting powers through it at its boost speed.
					if (!Boost.bBoosting)
					{
						AddZoneModifier(Modifiers, Source, EVehicleStat::MaxSpeed, Def->SpeedMultiplier, Def->Duration, Def->FadeOut);
					}
					// Dragged into the eye: a short loss of control.
					if (FromCentre <= Def->CoreRadius && Mine.Cooldown <= 0.f)
					{
						State.ControlLossTimeRemaining = FMath::Max(State.ControlLossTimeRemaining, Def->ControlLossTime);
						Mine.Cooldown = Def->Cooldown;
						UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: in the eye, control loss %.2f s"),
							*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Def->ControlLossTime);
					}
				}
			}
			if (bEntered)
			{
				UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: entered %.0f cm from the centre"),
					*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, FromCentre);
			}
			break;

		case ETrackZoneType::ArrowsPad:
			if (bInside)
			{
				Result.Acceleration += Zone.ArrowDirection * Def->PullAcceleration;
			}
			if (bEntered)
			{
				UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: pushed"), *VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name);
			}
			break;

		case ETrackZoneType::SpectralTower:
			// Another car in the zone sets the tower off: this car gets the burst. The car that set it off gets nothing.
			if (Race && Mine.Cooldown <= 0.f)
			{
				bool bSetOff = false;
				for (int32 Other = 0; Other < Race->Racers.Num() && !bSetOff; ++Other)
				{
					const FRaceStepRacer& Racer = Race->Racers[Other];
					bSetOff = Other != RacerId && Racer.bInRace
						&& FMath::Abs(Track->WrappedDelta(Zone.TrackDistance, Racer.TrackDistance)) <= Def->HalfLength
						&& FMath::Abs(Racer.LateralOffset - Zone.Lateral) <= Zone.HalfWidth;
				}
				if (bSetOff)
				{
					AddZoneModifier(Modifiers, Source, EVehicleStat::Acceleration, Def->AccelerationMultiplier, Def->Duration, Def->FadeOut);
					AddZoneModifier(Modifiers, Source, EVehicleStat::MaxSpeed, Def->SpeedMultiplier, Def->Duration, Def->FadeOut);
					Mine.Cooldown = Def->Cooldown;
					UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: wave from a rival, acceleration x%.2f for %.1f s"),
						*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Def->AccelerationMultiplier, Def->Duration);
				}
			}
			break;

		case ETrackZoneType::MeteorCluster:
		{
			// Impact number N lands at race time N x MeteorPeriod, on a spot that depends only on the zone and N.
			const int32 Impact = FMath::FloorToInt32(static_cast<float>(RaceTime) / Def->MeteorPeriod);
			if (Mine.MeteorHandled == INDEX_NONE)
			{
				Mine.MeteorHandled = Impact;
			}
			else if (Impact > Mine.MeteorHandled)
			{
				Mine.MeteorHandled = Impact;
				float SpotAhead = 0.f;
				float SpotSide = 0.f;
				FTrackZoneTable::GetMeteorSpot(Zone, Impact, SpotAhead, SpotSide);
				if (FMath::Sqrt(FMath::Square(Ahead - SpotAhead) + FMath::Square(Side - SpotSide)) <= Def->MeteorRadius)
				{
					const float Before = Boost.BoostCharge;
					Boost.BoostCharge = FMath::Max(Boost.BoostCharge - Def->Segments * SegmentSize, 0.f);
					State.ControlLossTimeRemaining = FMath::Max(State.ControlLossTimeRemaining, Def->ControlLossTime);
					Result.VelocityScale *= 1.f - Def->SpeedLossShare;
					UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: HIT by meteor %d, meter %.3f -> %.3f, speed x%.2f, control loss %.2f s"),
						*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Impact, Before, Boost.BoostCharge, 1.f - Def->SpeedLossShare, Def->ControlLossTime);
				}
			}
			break;
		}

		case ETrackZoneType::WarpGate:
			if (bReady && !Result.bTeleport)
			{
				// Out at the exit: the same distance from the centre line and the same height above the road, facing along the track, speed kept.
				const FTrackFrame Here = Track->GetFrameAtDistance(TrackDistance);
				const FTrackFrame Exit = Track->GetFrameAtDistance(Zone.TrackDistance + Def->WarpDistance);
				Result.bTeleport = true;
				Result.TeleportLocation = Exit.Location + Exit.Right * Projection.LateralOffset + FVector(0.0, 0.0, Location.Z - Here.Location.Z);
				Result.TeleportRotation = FRotationMatrix::MakeFromX(Exit.Direction.GetSafeNormal2D()).ToQuat();
				Result.TeleportVelocity = Exit.Direction.GetSafeNormal2D() * Velocity.Size2D();
				Mine.Cooldown = Def->Cooldown;
				// The car is somewhere else now: find it again from scratch next step.
				TrackDistance = -1.f;
				UE_LOG(LogCosmicRace, Log, TEXT("ZONE %s t=%.4f %s %s: warped %.0f cm ahead"),
					*VehicleName, RaceTime, ZoneTypeName(Def->Type), *Zone.Name, Track->WrappedDelta(Projection.Distance, Zone.TrackDistance + Def->WarpDistance));
			}
			break;
		}
	}

	return Result;
}
