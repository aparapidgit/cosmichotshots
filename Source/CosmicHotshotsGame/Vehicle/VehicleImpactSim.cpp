#include "Vehicle/VehicleImpactSim.h"

#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Vehicle/HoverSim.h"

namespace
{
	// A contact whose normal is this vertical is the ground (hard landing), not a wall.
	constexpr double GroundNormalZ = 0.7;

	// Contacts include "speculative" ones the solver only prepares for (for example the end face of the
	// next wall segment while sliding along the wall). A contact counts as a touch only when the solver
	// really pushed the car along its normal by more than this (cm/s), or the gap is closed (cm).
	constexpr double MinSolverPush = 5.0;
	constexpr double ContactTolerance = 1.0;

	// Its address is the cooldown key shared by all walls and objects.
	constexpr uint8 WallCooldownKey = 0;

	/** Wobble or spin-out for a received speed change; fills in the feedback part of the info. */
	void ApplyReaction(Chaos::FRigidBodyHandle_Internal& Body, const FVehicleImpactParams& Params, FVehicleImpactInfo& Info, bool bAllowSpinOut, FHoverSimState& SimState)
	{
		Info.Strength = Params.SpinOutThreshold > 0.f ? FMath::Clamp(Info.SpeedChange / Params.SpinOutThreshold, 0.f, 1.f) : 1.f;

		if (bAllowSpinOut && Info.SpeedChange >= Params.SpinOutThreshold)
		{
			Info.Reaction = EImpactReaction::SpinOut;

			// Spin away from the side that was hit.
			const FQuat Rotation = Body.R();
			const FVector Up = Rotation.GetUpVector();
			const FVector ToImpact = Info.ImpactPoint - FVector(Body.X());
			const double Side = FVector::DotProduct(FVector::CrossProduct(Rotation.GetForwardVector(), ToImpact), Up);
			const double SpinSign = Side >= 0.0 ? -1.0 : 1.0;
			Body.SetW(Body.W() + Up * (SpinSign * FMath::DegreesToRadians(Params.SpinOutYawRate)));

			SimState.ControlLossTimeRemaining = FMath::Max(SimState.ControlLossTimeRemaining, Params.SpinOutDuration * Params.HitRecoveryTime);
		}
		else if (Info.SpeedChange >= Params.WobbleThreshold)
		{
			Info.Reaction = EImpactReaction::Wobble;
			SimState.ControlLossTimeRemaining = FMath::Max(SimState.ControlLossTimeRemaining, Params.WobbleDuration * Params.HitRecoveryTime);
		}
	}
}

void FVehicleImpactSim::Apply(
	Chaos::FRigidBodyHandle_Internal& Body,
	const FVehicleImpactParams& Params,
	TConstArrayView<FVehicleContact> Contacts,
	double SimTime,
	FVehicleImpactState& State,
	FHoverSimState& SimState,
	TArray<FVehicleImpactInfo>& OutImpacts)
{
	// Forget finished cooldowns so the map never grows (one entry per wall segment otherwise).
	for (auto It = State.CooldownEndTimes.CreateIterator(); It; ++It)
	{
		if (It.Value() <= SimTime)
		{
			It.RemoveCurrent();
		}
	}

	const double SelfMass = Body.M();

	// Velocity right after the solve that handled these contacts (nothing has changed it yet this step).
	const FVector VelocityAfterSolve(Body.V());

	// How hard each contact really hit: the closing speed the solver took away. This is the same
	// measure as impulse / reduced mass: the solver's push on this car along the normal, scaled up
	// to the relative speed of the pair (for a wall the reduced mass is this car's own mass).
	struct FTouch
	{
		const FVehicleContact* Contact = nullptr;
		float ClosingSpeed = 0.f;
	};
	TArray<FTouch, TInlineAllocator<8>> Touches;
	for (const FVehicleContact& Contact : Contacts)
	{
		const FVehicleRawContact& Raw = Contact.Raw;
		if (Raw.Normal.IsNearlyZero())
		{
			continue;
		}

		const double Push = FVector::DotProduct(VelocityAfterSolve - Raw.SelfVelocityBeforeSolve, Raw.Normal);
		if (Push <= MinSolverPush && Raw.Separation > ContactTolerance)
		{
			continue; // a contact the solver only prepared for
		}

		double MassFactor = 1.0;
		if (Contact.bOtherIsVehicle && Contact.OtherMass > 0.f)
		{
			const double ReducedMass = (SelfMass * Contact.OtherMass) / FMath::Max(SelfMass + Contact.OtherMass, UE_DOUBLE_KINDA_SMALL_NUMBER);
			MassFactor = SelfMass / FMath::Max(ReducedMass, UE_DOUBLE_KINDA_SMALL_NUMBER);
		}
		const float ClosingSpeed = static_cast<float>(FMath::Max(Push, 0.0) * MassFactor);

		// One touch per other body: the hardest one.
		FTouch* Existing = Touches.FindByPredicate([&Raw](const FTouch& Other) { return Other.Contact->Raw.OtherParticle == Raw.OtherParticle; });
		if (!Existing)
		{
			Touches.Add({ &Contact, ClosingSpeed });
		}
		else if (ClosingSpeed > Existing->ClosingSpeed)
		{
			*Existing = { &Contact, ClosingSpeed };
		}
	}

	// Hardest first, so when several contacts share one cooldown the strongest is the one that counts.
	Touches.StableSort([](const FTouch& A, const FTouch& B) { return A.ClosingSpeed > B.ClosingSpeed; });

	for (const FTouch& Touch : Touches)
	{
		const FVehicleContact* Contact = Touch.Contact;
		const FVehicleRawContact& Raw = Contact->Raw;
		const FVector& Normal = Raw.Normal;
		const float ClosingSpeed = Touch.ClosingSpeed;
		const bool bIsWall = !Contact->bOtherIsVehicle && FMath::Abs(Normal.Z) < GroundNormalZ;
		if (!Contact->bOtherIsVehicle && !bIsWall)
		{
			continue; // the ground
		}

		// Any touch of a wall or object, however light and even during the cooldown, stops
		// close-pass charging for a moment: scraping along the barrier must never pay.
		if (bIsWall)
		{
			SimState.DriftBoost.ClosePassLockoutRemaining = FMath::Max(SimState.DriftBoost.ClosePassLockoutRemaining, Params.ClosePassHitLockout);
		}

		// A barrier is built from many pieces: touching two at a seam, or sliding from one onto the next,
		// is still one hit. So all walls and objects share one cooldown; each other car has its own.
		const void* CooldownKey = bIsWall ? static_cast<const void*>(&WallCooldownKey) : Raw.OtherParticle;
		if (State.CooldownEndTimes.Contains(CooldownKey) || ClosingSpeed < Params.MinImpactSpeed)
		{
			continue;
		}

		FVehicleImpactInfo Info;
		Info.OtherActor = Contact->OtherActor;
		Info.bOtherIsVehicle = Contact->bOtherIsVehicle;
		Info.ImpactPoint = Raw.Point;
		Info.Normal = Normal;
		Info.ClosingSpeed = ClosingSpeed;
		Info.SimTime = static_cast<float>(SimTime);

		if (Contact->bOtherIsVehicle)
		{
			// Knock-back this car receives: the attacker's RamPower pushes it up, this car's Stability
			// pulls it down, and the reduced mass makes the heavier car move less.
			const double ReducedMass = (SelfMass * Contact->OtherMass) / FMath::Max(SelfMass + Contact->OtherMass, UE_DOUBLE_KINDA_SMALL_NUMBER);
			// Passives: a side-contact specialist pushes harder (attacker) or is pushed less (this car), and
			// some cars lose less speed in any hit.
			const bool bSideContact = FMath::Abs(FVector::DotProduct(Normal, FQuat(Body.R()).GetRightVector())) >= Params.SideContactMinCos;
			const double PassiveScale = Contact->OtherSideRamPower * Params.HitSpeedLoss * (bSideContact ? Params.SideSpeedLoss : 1.f);
			const double Impulse = Params.VehicleBounce * ClosingSpeed * ReducedMass * (Contact->OtherRamPower / FMath::Max(Params.Stability, UE_KINDA_SMALL_NUMBER)) * PassiveScale;
			const float SpeedChange = static_cast<float>(FMath::Min(Impulse / FMath::Max(SelfMass, UE_DOUBLE_KINDA_SMALL_NUMBER), static_cast<double>(Params.MaxImpactSpeedChange)));

			Body.SetV(Body.V() + Normal * SpeedChange);

			Info.SpeedChange = SpeedChange;
			ApplyReaction(Body, Params, Info, /*bAllowSpinOut=*/true, SimState);
		}
		else
		{
			// Bounce back off the wall.
			const float BounceSpeed = FMath::Min(Params.WallBounce * ClosingSpeed, Params.MaxImpactSpeedChange);

			// Lose part of the speed along the wall: a light scrape costs little, a hard hit costs a lot.
			const FVector Velocity(Body.V());
			const FVector AlongWall = Velocity - Normal * FVector::DotProduct(Velocity, Normal);
			const float Hardness = FMath::Clamp(ClosingSpeed / Params.WallHardHitSpeed, 0.f, 1.f);
			const FVector ScrapeLoss = -AlongWall * (Params.WallSpeedLoss * Hardness * Params.HitSpeedLoss);

			Body.SetV(Velocity + Normal * BounceSpeed + ScrapeLoss);

			// For feedback, a wall hit is as strong as the speed it took away into the wall.
			Info.SpeedChange = ClosingSpeed;
			// Walls wobble the car but never spin it out: wall contact is too common to punish that hard.
			ApplyReaction(Body, Params, Info, /*bAllowSpinOut=*/false, SimState);

			// A wall or object hit also costs boost fluid: a light hit takes the minimum share of the
			// meter, a hard one the maximum. Once per hit (the cooldown covers the rest of the contact).
			Info.BoostLost = FMath::Lerp(Params.WallBoostLossMin, Params.WallBoostLossMax, Hardness);
			FDriftBoostState& DriftBoost = SimState.DriftBoost;
			DriftBoost.BoostCharge = FMath::Max(DriftBoost.BoostCharge - Info.BoostLost, 0.f);
		}

		State.CooldownEndTimes.Add(CooldownKey, SimTime + Params.ImpactCooldown);
		OutImpacts.Add(Info);
	}
}
