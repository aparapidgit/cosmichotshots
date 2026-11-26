// Vehicle zone component: what the track's zones do to THIS car.
//
// Called from the car's own fixed physics step (UHoverMovementComponent), so zones act the same at
// every frame rate and the same on humans and computer drivers. Each step the car finds where it
// is on the track, checks every zone of the race (FTrackZoneTable) and applies the zone's effect
// through systems that already exist: the modifier stack (speed, acceleration), the Energy Core
// (boost segments), control loss (the wobble of a strong hit). Two things are new and are handed
// back to the car: a pull or push (an acceleration) and moving the car (Warp Gate).
// Whatever depends on OTHER cars (a Spectral Tower's wave) or on time (meteors) is read from the
// race tracker's state after the previous step, so it does not matter which car steps first.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleZoneComponent.generated.h"

class FTrackSplineTable;
class FVehicleModifierStack;
class URaceTrackerComponent;
class UTrackZoneDefinition;
struct FHoverSimState;
struct FTrackZoneTable;

/** What the zones ask the car to do in this physics step, beyond what they changed themselves. */
struct FVehicleZoneStep
{
	/** Pull or push (cm/s^2, world space), applied while the car is on the ground. */
	FVector Acceleration = FVector::ZeroVector;
	/** Multiplier on the car's velocity, applied once (a meteor hit). 1 = none. */
	float VelocityScale = 1.f;
	/** Warp Gate: put the car here, facing and moving this way. */
	bool bTeleport = false;
	FVector TeleportLocation = FVector::ZeroVector;
	FQuat TeleportRotation = FQuat::Identity;
	FVector TeleportVelocity = FVector::ZeroVector;
};

UCLASS(ClassGroup = (Race))
class COSMICHOTSHOTSGAME_API UVehicleZoneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleZoneComponent();

	/** The zones of this race. Game thread, when the race is set up (before the first physics step of the race). */
	void SetZones(TSharedPtr<const FTrackZoneTable> InZones, TSharedPtr<const FTrackSplineTable> InTrack, URaceTrackerComponent* InTracker);

	/** One physics step: applies the zones the car is in. Changes State (boost meter, control loss) and the modifier stack. */
	FVehicleZoneStep StepZones(float DeltaTime, float SimTime, const FVector& Location, const FVector& Velocity,
		FHoverSimState& State, FVehicleModifierStack* Modifiers, int32 BoostSegments);

private:
	/** What this car remembers about one zone. */
	struct FZoneState
	{
		bool bInside = false;
		/** Seconds until the zone affects this car again. */
		float Cooldown = 0.f;
		/** Number of the last meteor impact this car was checked against; negative = none yet. */
		int32 MeteorHandled = INDEX_NONE;
	};

	TSharedPtr<const FTrackZoneTable> Zones;
	TSharedPtr<const FTrackSplineTable> Track;
	TWeakObjectPtr<URaceTrackerComponent> Tracker;

	/** Keeps the zone types alive while the table points at them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UTrackZoneDefinition>> Definitions;

	/** One modifier source per zone ("Zone.3"), so two zones of one type do not overwrite each other. */
	TArray<FName> SourceNames;
	FString VehicleName;

	// Owned by the physics step.
	TArray<FZoneState> States;
	float TrackDistance = -1.f;
	int32 RacerId = INDEX_NONE;
};
