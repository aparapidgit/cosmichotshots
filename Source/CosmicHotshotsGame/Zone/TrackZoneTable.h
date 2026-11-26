// Track zone table: where the zones of a race are, in track coordinates. Built once on the game
// thread when the race is set up (ARaceGameMode) and then only read, by every car in its own
// physics step (UVehicleZoneComponent) and by the zone actors for their visuals.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Zone/TrackZoneDefinition.h"

/** One zone on the track. */
struct FTrackZoneEntry
{
	/** Values of the zone's type. Kept alive by the zone actor; never changed during a race. */
	const UTrackZoneDefinition* Definition = nullptr;
	FString Name;
	/** Centre of the zone: distance along the track line and sideways offset from it (cm). */
	float TrackDistance = 0.f;
	float Lateral = 0.f;
	/** Half the width across the track (cm), already resolved (a wall-to-wall zone has the track's half width). */
	float HalfWidth = 0.f;
	FVector WorldLocation = FVector::ZeroVector;
	/** Arrows Pad: the way the arrows point (horizontal unit vector). */
	FVector ArrowDirection = FVector::ForwardVector;
	/** Seed of the zone's own schedule (meteors); its position in the list of zones sorted by name. */
	int32 Seed = 0;
};

struct FTrackZoneTable
{
	TArray<FTrackZoneEntry> Zones;

	/**
	 * Meteor Cluster: where impact number Index lands, relative to the zone's centre (cm along the
	 * track, cm sideways). The same answer for every car and for the visuals; it depends only on the
	 * zone's seed and the impact number. Impact Index lands at race time Index x MeteorPeriod.
	 */
	static void GetMeteorSpot(const FTrackZoneEntry& Zone, int32 Index, float& OutAlong, float& OutSideways)
	{
		FRandomStream Stream(static_cast<int32>(HashCombineFast(::GetTypeHash(Zone.Seed), ::GetTypeHash(Index))));
		OutAlong = Stream.FRandRange(-1.f, 1.f) * (Zone.Definition ? Zone.Definition->HalfLength : 0.f);
		OutSideways = Stream.FRandRange(-1.f, 1.f) * Zone.HalfWidth;
	}
};
