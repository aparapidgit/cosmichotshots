// Track zone definition: one TYPE of zone a designer can place on a track (Turbo Hub, Energy Dock,
// Meteor Cluster ...). A zone on the map is an ATrackZone actor that points at one of these.
//
// Every number of a zone lives here; each zone type reads only the values that apply to it (the
// comments say which). All values are placeholders to be tuned by playtesting. The asset values are
// written by Scripts/create_zone_assets.py.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TrackZoneDefinition.generated.h"

UENUM(BlueprintType)
enum class ETrackZoneType : uint8
{
	/** Meteors hit the floor at announced spots; a hit car wobbles, slows and loses boost segments. */
	MeteorCluster,
	/** A car driving through sends a wave that gives every OTHER car a short burst of acceleration. */
	SpectralTower,
	/** A magnetic rock pulls cars towards it, mostly sideways. */
	Magnetonium,
	/** Driving through refills boost segments. */
	EnergyDock,
	/** Slows the car and pulls it to the centre, where it briefly loses control. Boosting powers through. */
	DustVortex,
	/** Strong acceleration and a higher top speed for a few seconds. */
	TurboHub,
	/** Takes ALL the boost; a car with none is slowed and unsettled instead. */
	GravityAbyss,
	/** Moves the car to the exit, a little further along the track. */
	WarpGate,
	/** Pushes the car the way the arrows (the actor's forward direction) point. */
	ArrowsPad
};

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UTrackZoneDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	ETrackZoneType Type = ETrackZoneType::TurboHub;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FText DisplayName;

	/** Placeholder colour of the zone's floor plate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FLinearColor Colour = FLinearColor::White;

	// Shape (cm). Box zones use length and width; Magnetonium and Dust Vortex use the radius.

	/** Half the zone's length along the track. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "50.0"))
	float HalfLength = 500.f;

	/** Half the zone's width across the track. 0 = from wall to wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.0"))
	float HalfWidth = 500.f;

	/** Reach of a round zone (Magnetonium, Dust Vortex) from its centre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "50.0"))
	float Radius = 1200.f;

	// Speed (Turbo Hub, Spectral Tower, Dust Vortex, Gravity Abyss)

	/** Top speed multiplier while the effect lasts (1 = unchanged). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.f;

	/** Acceleration multiplier while the effect lasts (1 = unchanged). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.1"))
	float AccelerationMultiplier = 1.f;

	/** Seconds the effect lasts after the car left the zone (or after the wave reached it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0"))
	float Duration = 2.f;

	/** Seconds the effect takes to fade out at its end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0"))
	float FadeOut = 0.5f;

	// Pull and push (Magnetonium, Dust Vortex, Arrows Pad)

	/** Strongest pull or push (cm/s^2): at the centre of a round zone, anywhere on an Arrows Pad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pull", meta = (ClampMin = "0.0"))
	float PullAcceleration = 0.f;

	/** Share of the pull that acts ALONG the track (Magnetonium pulls mostly sideways: low value). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pull", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlongTrackShare = 1.f;

	/** Dust Vortex: within this distance (cm) of the centre the car loses control for ControlLossTime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pull", meta = (ClampMin = "0.0"))
	float CoreRadius = 300.f;

	// Energy Core and hits (Energy Dock, Meteor Cluster, Gravity Abyss, Dust Vortex)

	/** Boost segments given (Energy Dock) or taken (Meteor Cluster). The Gravity Abyss always takes everything. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy and hits", meta = (ClampMin = "0"))
	int32 Segments = 1;

	/** Seconds of reduced grip and steering (meteor hit, vortex core, abyss with an empty meter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy and hits", meta = (ClampMin = "0.0"))
	float ControlLossTime = 0.f;

	/** Share of its speed the car loses at once (meteor hit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy and hits", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedLossShare = 0.f;

	/** Seconds before the zone affects the same car again (Energy Dock, Gravity Abyss, Spectral Tower wave, Warp Gate, vortex core). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy and hits", meta = (ClampMin = "0.0"))
	float Cooldown = 0.f;

	// Meteor Cluster

	/** Seconds between meteor impacts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meteors", meta = (ClampMin = "0.2"))
	float MeteorPeriod = 1.5f;

	/** Seconds the impact spot is shown before the meteor lands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meteors", meta = (ClampMin = "0.0"))
	float MeteorWarning = 1.f;

	/** A car within this distance (cm) of the impact spot is hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meteors", meta = (ClampMin = "10.0"))
	float MeteorRadius = 250.f;

	// Warp Gate

	/** How far along the track (cm) from the zone's centre the car comes out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warp", meta = (ClampMin = "0.0"))
	float WarpDistance = 1000.f;
};
