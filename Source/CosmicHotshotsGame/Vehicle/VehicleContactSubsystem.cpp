#include "Vehicle/VehicleContactSubsystem.h"

#include "Chaos/ChaosScene.h"
#include "Chaos/ContactModification.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/SimCallbackObject.h"
#include "Engine/World.h"
#include "Misc/ScopeLock.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Vehicle/RaceVehicle.h"

/**
 * Physics-thread side: records, for every registered vehicle, the contacts of the current step.
 * It only reads the contacts; it never changes how the solver handles them.
 */
class FVehicleContactCallback : public Chaos::TSimCallbackObject<
	Chaos::FSimCallbackNoInput,
	Chaos::FSimCallbackNoOutput,
	Chaos::ESimCallbackOptions::ContactModification | Chaos::ESimCallbackOptions::ParticleUnregister>
{
public:
	void Register(const Chaos::FGeometryParticleHandle* Particle, const IPhysicsProxyBase* Proxy, ARaceVehicle* Vehicle)
	{
		FScopeLock Lock(&Mutex);
		Vehicles.Add(Particle, { Vehicle, Proxy });
	}

	void Unregister(const Chaos::FGeometryParticleHandle* Particle)
	{
		FScopeLock Lock(&Mutex);
		Vehicles.Remove(Particle);
		Contacts.Remove(Particle);
	}

	void Take(const Chaos::FGeometryParticleHandle* Particle, TArray<FVehicleRawContact>& OutContacts)
	{
		FScopeLock Lock(&Mutex);
		OutContacts.Reset();
		if (TArray<FVehicleRawContact>* Found = Contacts.Find(Particle))
		{
			OutContacts = MoveTemp(*Found);
			Found->Reset();
		}
	}

	ARaceVehicle* Find(const Chaos::FGeometryParticleHandle* Particle) const
	{
		FScopeLock Lock(&Mutex);
		const FVehicleEntry* Found = Vehicles.Find(Particle);
		return Found ? Found->Vehicle.Get() : nullptr;
	}

private:
	struct FVehicleEntry
	{
		TWeakObjectPtr<ARaceVehicle> Vehicle;
		/** Identity only: compared with the proxies of bodies that leave the scene. */
		const IPhysicsProxyBase* Proxy = nullptr;
	};

	virtual FName GetFNameForStatId() const override
	{
		static const FName Name(TEXT("FVehicleContactCallback"));
		return Name;
	}

	/** A body leaves the physics scene: forget it as a vehicle and drop every contact that mentions it. */
	virtual void OnParticleUnregistered_Internal(TArray<TTuple<Chaos::FUniqueIdx, Chaos::FSingleParticlePhysicsProxy*>>& UnregisteredProxies) override
	{
		FScopeLock Lock(&Mutex);

		for (const TTuple<Chaos::FUniqueIdx, Chaos::FSingleParticlePhysicsProxy*>& Unregistered : UnregisteredProxies)
		{
			const IPhysicsProxyBase* Proxy = Unregistered.Get<1>();
			for (auto It = Vehicles.CreateIterator(); It; ++It)
			{
				if (It.Value().Proxy == Proxy)
				{
					Contacts.Remove(It.Key());
					It.RemoveCurrent();
				}
			}
			for (TPair<const Chaos::FGeometryParticleHandle*, TArray<FVehicleRawContact>>& Entry : Contacts)
			{
				Entry.Value.RemoveAll([Proxy](const FVehicleRawContact& Contact) { return Contact.OtherProxy == Proxy; });
			}
		}
	}

	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override
	{
		FScopeLock Lock(&Mutex);

		for (const TPair<const Chaos::FGeometryParticleHandle*, FVehicleEntry>& Entry : Vehicles)
		{
			// Only the newest step counts: contacts nobody took are stale.
			TArray<FVehicleRawContact>& VehicleContacts = Contacts.FindOrAdd(Entry.Key);
			VehicleContacts.Reset();

			Chaos::FGeometryParticleHandle* Self = const_cast<Chaos::FGeometryParticleHandle*>(Entry.Key);
			for (Chaos::FContactPairModifier& Pair : Modifier.GetContacts(Self))
			{
				if (Pair.GetIsProbe() || Pair.GetNumContacts() <= 0)
				{
					continue;
				}

				const Chaos::TVec2<Chaos::FGeometryParticleHandle*> Particles = Pair.GetParticlePair();
				const int32 SelfIndex = Particles[0] == Self ? 0 : 1;
				const Chaos::FGeometryParticleHandle* Other = Particles[1 - SelfIndex];
				const int32 PointIndex = Pair.GetDeepestContactIndex();

				// The engine's normal points away from the SECOND body; flip it so it points from the other body to us.
				FVector Normal = FVector(Pair.GetWorldNormal(PointIndex));
				if (SelfIndex == 1)
				{
					Normal = -Normal;
				}

				FVehicleRawContact& Contact = VehicleContacts.AddDefaulted_GetRef();
				Contact.OtherParticle = Other;
				Contact.OtherProxy = Other ? Other->PhysicsProxy() : nullptr;
				Contact.Normal = Normal.GetSafeNormal();
				Contact.Point = FVector(Pair.GetWorldContactLocation(PointIndex));
				Contact.SelfVelocityBeforeSolve = LinearVelocity(Self);
				if (const Chaos::FPBDRigidParticleHandle* OtherRigid = Other ? Other->CastToRigidParticle() : nullptr)
				{
					Contact.OtherMass = static_cast<float>(OtherRigid->M());
				}
				Contact.Separation = static_cast<float>(Pair.GetSeparation(PointIndex));
				if (Other)
				{
					Contact.OtherRotation = FQuat(Other->GetR());
				}
			}
		}
	}

	/** Velocity of a body's centre. Static bodies (walls) do not move. */
	static FVector LinearVelocity(const Chaos::FGeometryParticleHandle* Particle)
	{
		const Chaos::FKinematicGeometryParticleHandle* Kinematic = Particle ? Particle->CastToKinematicParticle() : nullptr;
		return Kinematic ? FVector(Kinematic->GetV()) : FVector::ZeroVector;
	}

	mutable FCriticalSection Mutex;
	TMap<const Chaos::FGeometryParticleHandle*, FVehicleEntry> Vehicles;
	TMap<const Chaos::FGeometryParticleHandle*, TArray<FVehicleRawContact>> Contacts;
};

bool UVehicleContactSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UVehicleContactSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FPhysScene* Scene = InWorld.GetPhysicsScene();
	Chaos::FPhysicsSolver* Solver = Scene ? Scene->GetSolver() : nullptr;
	if (Solver && !Callback)
	{
		Callback = Solver->CreateAndRegisterSimCallbackObject_External<FVehicleContactCallback>();
	}
}

void UVehicleContactSubsystem::Deinitialize()
{
	if (Callback)
	{
		const UWorld* World = GetWorld();
		FPhysScene* Scene = World ? World->GetPhysicsScene() : nullptr;
		if (Chaos::FPhysicsSolver* Solver = Scene ? Scene->GetSolver() : nullptr)
		{
			Solver->UnregisterAndFreeSimCallbackObject_External(Callback);
		}
		Callback = nullptr;
	}
	Super::Deinitialize();
}

void UVehicleContactSubsystem::RegisterVehicle(const Chaos::FGeometryParticleHandle* Particle, const IPhysicsProxyBase* Proxy, ARaceVehicle* Vehicle)
{
	if (Callback && Particle)
	{
		Callback->Register(Particle, Proxy, Vehicle);
	}
}

void UVehicleContactSubsystem::UnregisterVehicle(const Chaos::FGeometryParticleHandle* Particle)
{
	if (Callback && Particle)
	{
		Callback->Unregister(Particle);
	}
}

void UVehicleContactSubsystem::TakeContacts(const Chaos::FGeometryParticleHandle* Particle, TArray<FVehicleRawContact>& OutContacts)
{
	OutContacts.Reset();
	if (Callback && Particle)
	{
		Callback->Take(Particle, OutContacts);
	}
}

ARaceVehicle* UVehicleContactSubsystem::FindVehicle(const Chaos::FGeometryParticleHandle* Particle) const
{
	return Callback && Particle ? Callback->Find(Particle) : nullptr;
}
