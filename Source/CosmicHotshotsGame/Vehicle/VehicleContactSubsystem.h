// Vehicle contacts, captured inside every fixed physics step.
//
// Hit events (OnComponentHit) reach the game thread once per FRAME, so a reaction
// built on them depends on the frame rate. Instead, a Chaos sim callback with
// contact modification sees every contact pair of every physics step (it only reads
// them, it never changes how the solver handles them). For each registered vehicle it
// records the contacts of that step: what was touched, the normal, and the vehicle's
// velocity just before the solve. The vehicle's next physics step takes them, compares
// with its velocity after the solve to see how hard the solver really pushed, and
// applies the arcade reaction (FVehicleImpactSim). Every frame rate gets exactly the
// same impacts, always one step (1/120 s) after the contact.
//
// Threading: the callback runs on the physics thread during the solve; registration
// and TakeContacts run on the game thread (TakeContacts from AsyncPhysicsTickComponent,
// while the physics thread waits). A lock guards the shared data anyway. When a body
// leaves the physics scene, contacts with it are dropped before anyone can read them.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleContactSubsystem.generated.h"

class ARaceVehicle;
class FVehicleContactCallback;
class IPhysicsProxyBase;

/** One contact of a vehicle in one physics step, as the physics engine saw it before solving. */
struct FVehicleRawContact
{
	/** The other body, as an identity key only (never dereferenced outside the callback). */
	const Chaos::FGeometryParticleHandle* OtherParticle = nullptr;
	/** The other body's proxy: its owner tells which actor was hit. Valid while the contact exists. */
	const IPhysicsProxyBase* OtherProxy = nullptr;
	/** Unit vector from the other body towards this vehicle. */
	FVector Normal = FVector::ZeroVector;
	FVector Point = FVector::ZeroVector;
	/**
	 * This vehicle's velocity just before the solver handled the contacts. Compared with the velocity
	 * after the solve, it tells how hard the solver really pushed along each contact normal.
	 */
	FVector SelfVelocityBeforeSolve = FVector::ZeroVector;
	/** Gap along the normal before the solve (negative = already overlapping). */
	float Separation = 0.f;
	/** Mass of the other body in kg (0 for walls and other bodies that do not move). */
	float OtherMass = 0.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UVehicleContactSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Starts recording contacts for a vehicle body. Call from the physics step once the particle exists. */
	void RegisterVehicle(const Chaos::FGeometryParticleHandle* Particle, const IPhysicsProxyBase* Proxy, ARaceVehicle* Vehicle);
	void UnregisterVehicle(const Chaos::FGeometryParticleHandle* Particle);

	/** Moves the contacts recorded for this vehicle in the last physics step into OutContacts. */
	void TakeContacts(const Chaos::FGeometryParticleHandle* Particle, TArray<FVehicleRawContact>& OutContacts);

	/** The vehicle that owns this particle, or null when it is not a registered vehicle. Game thread. */
	ARaceVehicle* FindVehicle(const Chaos::FGeometryParticleHandle* Particle) const;

	bool IsActive() const { return Callback != nullptr; }

private:
	FVehicleContactCallback* Callback = nullptr;
};
