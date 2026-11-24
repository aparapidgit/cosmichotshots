#include "Vehicle/HoverMovementComponent.h"

#include "CosmicHotshotsGame.h"
#include "Chaos/Framework/PhysicsProxyBase.h"
#include "Chaos/ParticleHandle.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/RaceVehicle.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Race/TrackSplineActor.h"
#include "Race/TrackSplineTable.h"
#include "UnrealClient.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Passive/VehiclePassiveComponent.h"
#include "Vehicle/VehicleDebug.h"
#include "Vehicle/VehicleModifierComponent.h"
#include "Vehicle/VehicleStatsComponent.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	const FName HoverTraceTag(TEXT("HoverTrace"));

	constexpr float KmhToCmPerSecond = 100000.f / 3600.f;
	constexpr float CmPerSecondToKmh = 1.f / KmhToCmPerSecond;

	// Debug display only.
	constexpr float DebugPointSize = 8.f;

	// How long the placeholder "ability used" message stays on screen.
	constexpr float AbilityMessageSeconds = 2.f;

#if !UE_BUILD_SHIPPING
	// Test helper (ch.Test.SpeedModifier): when the test modifier starts, how long it holds and fades.
	const FName TestModifierSource(TEXT("Test.SpeedModifier"));
	constexpr double TestModifierStartTime = 2.0;
	constexpr float TestModifierDuration = 2.f;
	constexpr float TestModifierFade = 1.f;

	// Test helper: scripted mode 6 drives around the track line (GetAutopilotInput).
	constexpr int32 AutopilotMode = 6;
	constexpr double AutopilotSettleEnd = 1.0;
	/** Cm ahead of the car's track position the autopilot aims at, plus speed x time. */
	constexpr float AutopilotLookAheadBase = 1500.f;
	constexpr float AutopilotLookAheadTime = 0.6f;
	/** Angle to the aim point (degrees) that gives full steering lock. */
	constexpr float AutopilotFullSteerAngle = 20.f;
	/** Above this angle to the aim point the autopilot eases off the throttle. */
	constexpr float AutopilotSlowAngle = 25.f;
	constexpr float AutopilotSlowThrottle = 0.4f;
	constexpr float AutopilotProjectionWindow = 3000.f;
	constexpr float AutopilotReacquireDistance = 2500.f;
	/** Below this speed (cm/s) for longer than the delay (s), the autopilot reverses for the duration (s). */
	constexpr float AutopilotStuckSpeed = 300.f;
	constexpr float AutopilotStuckDelay = 1.f;
	constexpr float AutopilotReverseDuration = 1.2f;

	// Test helper: scripted mode 8 (drift chain). Distances along the track line in cm, times in seconds.
	constexpr int32 DriftChainMode = 8;
	/** Speed cap (cm/s) until the drifts are done, and seconds it stays on after them. */
	constexpr float DriftChainMaxSpeed = 2300.f;
	/** Line beside the track centre (cm, negative = left = outside of the sweeper) while drifting: a drift tightens the turn, so it needs room on the inside. */
	constexpr float DriftChainLateralOffset = -1000.f;
	constexpr double DriftChainSlowAfter = 2.0;
	constexpr float DriftChainStartDistance = 33000.f;
	constexpr float DriftChainStartWindow = 5000.f;
	constexpr double DriftChainHold = 2.1;
	constexpr double DriftChainPause = 1.0;

	/**
	 * Fixed input sequence for the headless frame-rate test (ch.Test.ScriptedInput 1).
	 * Driven by simulation time, so every frame rate gets exactly the same input.
	 */
	FVehicleInputState GetScriptedDriftBoostInput(double SimTime);

	FVehicleInputState GetScriptedTestInput(int32 Mode, double SimTime)
	{
		constexpr int32 DriftBoostMode = 2;
		constexpr int32 WallHitMode = 3;
		constexpr int32 ClosePassMode = 4;
		constexpr int32 OvertakeMode = 5;
		constexpr int32 SideContactMode = 7;
		if (Mode == SideContactMode)
		{
			// Mode 7: pull away and lean left into a dummy car standing right beside (side contact).
			constexpr double SideSettleEnd = 1.0;
			constexpr double SideSteerEnd = 1.8;
			constexpr double SideThrottleEnd = 5.0;
			constexpr float SideSteer = -0.25f;

			FVehicleInputState SideScripted;
			if (SimTime >= SideSettleEnd && SimTime < SideThrottleEnd)
			{
				SideScripted.Throttle = 1.f;
				SideScripted.Steer = SimTime < SideSteerEnd ? SideSteer : 0.f;
			}
			return SideScripted;
		}
		if (Mode == OvertakeMode)
		{
			// Mode 5: settle, then full throttle straight ahead (past a dummy car placed ahead and to the side).
			constexpr double OvertakeSettleEnd = 1.0;
			constexpr double OvertakeThrottleEnd = 7.0;

			FVehicleInputState OvertakeScripted;
			OvertakeScripted.Throttle = (SimTime >= OvertakeSettleEnd && SimTime < OvertakeThrottleEnd) ? 1.f : 0.f;
			return OvertakeScripted;
		}
		if (Mode == DriftBoostMode)
		{
			return GetScriptedDriftBoostInput(SimTime);
		}
		if (Mode == WallHitMode)
		{
			// Mode 3: accelerate, steer right briefly, then hold the wheel straight into the right-hand wall.
			constexpr double WallSettleEnd = 1.0;
			constexpr double WallTurnStart = 3.0;
			constexpr double WallTurnEnd = 3.5;
			constexpr double WallThrottleEnd = 5.5;
			constexpr float WallTurnSteer = 0.5f;

			FVehicleInputState WallScripted;
			if (SimTime >= WallSettleEnd && SimTime < WallThrottleEnd)
			{
				WallScripted.Throttle = 1.f;
				WallScripted.Steer = (SimTime >= WallTurnStart && SimTime < WallTurnEnd) ? WallTurnSteer : 0.f;
			}
			return WallScripted;
		}
		if (Mode == ClosePassMode)
		{
			// Mode 4: accelerate, turn a little to the right, then hold the wheel straight so the car
			// closes in on the right-hand wall slowly (close-pass charging rises) until it touches it.
			constexpr double PassSettleEnd = 1.0;
			constexpr double PassTurnStart = 3.0;
			constexpr double PassTurnEnd = 3.3;
			constexpr double PassThrottleEnd = 8.0;
			constexpr float PassTurnSteer = 0.15f;

			FVehicleInputState PassScripted;
			if (SimTime >= PassSettleEnd && SimTime < PassThrottleEnd)
			{
				PassScripted.Throttle = 1.f;
				PassScripted.Steer = (SimTime >= PassTurnStart && SimTime < PassTurnEnd) ? PassTurnSteer : 0.f;
			}
			return PassScripted;
		}

		constexpr double SettleEnd = 1.0;
		constexpr double StraightEnd = 4.0;
		constexpr double TurnRightEnd = 4.5;
		constexpr double TurnLeftEnd = 5.0;
		constexpr double ThrottleEnd = 5.5;
		constexpr float TurnSteer = 0.5f;

		// Settle, accelerate straight, steer right then back left (stays clear of the walls), then coast.
		FVehicleInputState Scripted;
		if (SimTime >= SettleEnd && SimTime < ThrottleEnd)
		{
			Scripted.Throttle = 1.f;
			if (SimTime >= StraightEnd && SimTime < TurnRightEnd)
			{
				Scripted.Steer = TurnSteer;
			}
			else if (SimTime >= TurnRightEnd && SimTime < TurnLeftEnd)
			{
				Scripted.Steer = -TurnSteer;
			}
		}
		return Scripted;
	}

	/**
	 * Mode 2 (ch.Test.ScriptedInput 2): accelerate, drift left on a wide line, straighten up,
	 * boost, then release the throttle while the boost is still running.
	 */
	FVehicleInputState GetScriptedDriftBoostInput(double SimTime)
	{
		constexpr double SettleEnd = 1.0;
		constexpr double DriftStart = 4.0;
		constexpr double DriftWidenStart = 4.1;
		constexpr double DriftEnd = 4.6;
		constexpr double StraightenEnd = 5.1;
		constexpr double BoostPressStart = 5.3;
		constexpr double BoostPressEnd = 6.5;
		constexpr double AbilityPressStart = 3.0;
		constexpr double AbilityPressEnd = 3.1;
		constexpr double ThrottleEnd = 6.0;
		constexpr double OtherButtonStart = 4.3;
		// The stick points AGAINST the drift from the start: the button alone must pick the direction.
		constexpr float DriftStartSteer = 0.3f;
		constexpr float DriftWidenSteer = 1.f;
		constexpr float StraightenSteer = 0.6f;

		FVehicleInputState Scripted;
		Scripted.Throttle = (SimTime >= SettleEnd && SimTime < ThrottleEnd) ? 1.f : 0.f;
		Scripted.bDriftLeft = SimTime >= DriftStart && SimTime < DriftEnd;
		// Pressing the other drift button during a drift must not change or end it.
		Scripted.bDriftRight = SimTime >= OtherButtonStart && SimTime < DriftEnd;
		// Boost is held: it runs until the button is released or the meter is empty.
		Scripted.bBoost = SimTime >= BoostPressStart && SimTime < BoostPressEnd;
		// One press of the ability button spends one whole segment (if a full one is available).
		Scripted.bUseAbility = SimTime >= AbilityPressStart && SimTime < AbilityPressEnd;

		if (SimTime >= DriftStart && SimTime < DriftWidenStart)
		{
			Scripted.Steer = DriftStartSteer;
		}
		else if (SimTime >= DriftWidenStart && SimTime < DriftEnd)
		{
			Scripted.Steer = DriftWidenSteer;
		}
		else if (SimTime >= DriftEnd && SimTime < StraightenEnd)
		{
			Scripted.Steer = StraightenSteer;
		}
		return Scripted;
	}
#endif
}

UHoverMovementComponent::UHoverMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// The engine asserts that every component registered for the async physics tick is active.
	bAutoActivate = true;
}

void UHoverMovementComponent::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	// Test helper: the autopilot (scripted mode 6) reads the track line inside the physics step.
	AutopilotTrack = ATrackSplineActor::FindTable(GetWorld());
#endif

	// Enabled here, not in the constructor, so the physics step never sees a component that is not active yet.
	SetAsyncPhysicsTickEnabled(true);
}

void UHoverMovementComponent::SetUpdatedBody(UPrimitiveComponent* InBody)
{
	UpdatedBody = InBody;
}

void UHoverMovementComponent::SetTuning(const UVehicleTuningData* InTuning)
{
	Tuning = InTuning;
}

void UHoverMovementComponent::SetStats(const UVehicleStatsComponent* InStats)
{
	Stats = InStats;
}

void UHoverMovementComponent::SetModifierComponents(UVehicleModifierComponent* InModifiers, UVehiclePassiveComponent* InPassive)
{
	Modifiers = InModifiers;
	Passive = InPassive;
}

void UHoverMovementComponent::SetInput(const FVehicleInputState& InInput)
{
	if (InInput.bBoost && !Input.bBoost)
	{
		bBoostPressedLatch = true;
	}
	if (InInput.bUseAbility && !Input.bUseAbility)
	{
		bAbilityPressedLatch = true;
	}
	Input = InInput;
}

void UHoverMovementComponent::TakePendingImpacts(TArray<FVehicleImpactInfo>& OutImpacts)
{
	OutImpacts = MoveTemp(PendingImpacts);
	PendingImpacts.Reset();
}

void UHoverMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVehicleContactSubsystem* Contacts = ContactSubsystem.Get(); Contacts && RegisteredParticle)
	{
		Contacts->UnregisterVehicle(RegisteredParticle);
	}
	RegisteredParticle = nullptr;

	Super::EndPlay(EndPlayReason);
}

FVehicleImpactParams UHoverMovementComponent::BuildImpactParams() const
{
	FVehicleImpactParams Params;
	Params.MinImpactSpeed = Tuning->MinImpactSpeed;
	Params.ImpactCooldown = Tuning->ImpactCooldown;
	Params.VehicleBounce = Tuning->VehicleBounce;
	Params.MaxImpactSpeedChange = Tuning->MaxImpactSpeedChange;
	Params.WobbleThreshold = Tuning->WobbleThreshold;
	Params.WobbleDuration = Tuning->WobbleDuration;
	Params.SpinOutThreshold = Tuning->SpinOutThreshold;
	Params.SpinOutDuration = Tuning->SpinOutDuration;
	Params.SpinOutYawRate = Tuning->SpinOutYawRate;
	Params.WallBounce = Tuning->WallBounce;
	Params.WallSpeedLoss = Tuning->WallSpeedLoss;
	Params.WallHardHitSpeed = Tuning->WallHardHitSpeed;
	Params.WallBoostLossMin = Tuning->WallBoostLossMin;
	Params.WallBoostLossMax = Tuning->WallBoostLossMax;
	Params.ClosePassHitLockout = Tuning->ClosePassHitLockout;
	Params.Stability = Stats ? Stats->GetStability() : 1.f;
	Params.SideContactMinCos = FMath::Cos(FMath::DegreesToRadians(Tuning->SideContactMaxAngle));
	if (Modifiers)
	{
		const FVehicleImpactModifiers& ImpactModifiers = Modifiers->GetStack().GetImpactTotals();
		Params.SideSpeedLoss = ImpactModifiers.SideSpeedLoss;
		Params.HitRecoveryTime = ImpactModifiers.HitRecoveryTime;
		Params.HitSpeedLoss = ImpactModifiers.HitSpeedLoss;
	}
	return Params;
}

void UHoverMovementComponent::ApplyImpacts(Chaos::FRigidBodyHandle_Internal& Body, Chaos::FSingleParticlePhysicsProxy& Proxy)
{
	UVehicleContactSubsystem* Contacts = GetWorld()->GetSubsystem<UVehicleContactSubsystem>();
	const Chaos::FGeometryParticleHandle* Particle = Proxy.GetHandle_LowLevel();
	if (!Contacts || !Contacts->IsActive() || !Particle)
	{
		return;
	}

	// The body's particle exists only once physics has created it, so register here, in the physics step.
	if (Particle != RegisteredParticle)
	{
		if (RegisteredParticle)
		{
			Contacts->UnregisterVehicle(RegisteredParticle);
		}
		Contacts->RegisterVehicle(Particle, &Proxy, Cast<ARaceVehicle>(GetOwner()));
		RegisteredParticle = Particle;
		ContactSubsystem = Contacts;
	}

	Contacts->TakeContacts(Particle, RawContacts);
	if (RawContacts.IsEmpty())
	{
		// Nothing touched; cooldowns still have to run out, so let the sim see an empty list.
		ResolvedContacts.Reset();
	}
	else
	{
		// Who was hit. The physics thread waits while this step runs, so reading the other body's owner is safe.
		ResolvedContacts.Reset(RawContacts.Num());
		const float SideContactMinCos = FMath::Cos(FMath::DegreesToRadians(Tuning->SideContactMaxAngle));
		for (const FVehicleRawContact& Raw : RawContacts)
		{
			FVehicleContact& Contact = ResolvedContacts.AddDefaulted_GetRef();
			Contact.Raw = Raw;
			if (const ARaceVehicle* OtherVehicle = Contacts->FindVehicle(Raw.OtherParticle))
			{				Contact.bOtherIsVehicle = true;
				Contact.OtherActor = const_cast<ARaceVehicle*>(OtherVehicle);
				// Mass straight from the physics particle: the game-thread body API is off limits inside the step.
				Contact.OtherMass = Raw.OtherMass;
				Contact.OtherRamPower = OtherVehicle->GetStats()->GetRamPower();
				// Touched on the OTHER car's side: its side-contact push (Glick) counts on top of its RamPower.
				// Passives keep this value constant while they are on, so it does not matter which car stepped first.
				const bool bOnOthersSide = FMath::Abs(FVector::DotProduct(Raw.Normal, Raw.OtherRotation.GetRightVector())) >= SideContactMinCos;
				if (bOnOthersSide && OtherVehicle->GetModifiers())
				{
					Contact.OtherSideRamPower = OtherVehicle->GetModifiers()->GetStack().GetImpactTotals().SideRamPower;
				}
			}
			else if (Raw.OtherProxy)
			{
				UObject* Owner = Raw.OtherProxy->GetOwner();
				if (const UActorComponent* OwnerComponent = Cast<UActorComponent>(Owner))
				{
					Contact.OtherActor = OwnerComponent->GetOwner();
				}
				else
				{
					Contact.OtherActor = Cast<AActor>(Owner);
				}
			}
		}
	}

	const int32 ImpactsBefore = PendingImpacts.Num();
	FVehicleImpactSim::Apply(Body, BuildImpactParams(), ResolvedContacts, ElapsedSimTime, ImpactState, SimState, PendingImpacts);
	ImpactCounter += PendingImpacts.Num() - ImpactsBefore;
}

FHoverSimParams UHoverMovementComponent::BuildSimParams() const
{
	using namespace CosmicHotshots;

	FHoverSimParams Params;

	Params.HoverPoints.Append(Tuning->HoverPoints);
	Params.RideHeight = VehicleCVars::Resolve(VehicleCVars::RideHeight, Tuning->RideHeight);
	Params.HoverFrequency = VehicleCVars::Resolve(VehicleCVars::HoverFrequency, Tuning->HoverFrequency);
	Params.HoverDampingRatio = VehicleCVars::Resolve(VehicleCVars::HoverDamping, Tuning->HoverDampingRatio);
	Params.TraceRadius = Tuning->HoverTraceRadius;
	Params.GroundedExtraReach = Tuning->GroundedExtraReach;
	Params.MinGroundedPoints = Tuning->MinGroundedPoints;

	Params.GravityAcceleration = VehicleCVars::Resolve(VehicleCVars::Gravity, Tuning->GravityAcceleration);
	Params.AirGravityMultiplier = Tuning->AirGravityMultiplier;

	Params.MaxSpeed = VehicleCVars::Resolve(VehicleCVars::MaxSpeedKmh, Tuning->MaxSpeedKmh) * KmhToCmPerSecond;
	Params.Acceleration = VehicleCVars::Resolve(VehicleCVars::Acceleration, Tuning->Acceleration);
	Params.AccelFalloffExponent = Tuning->AccelFalloffExponent;
	Params.BrakeDeceleration = Tuning->BrakeDeceleration;
	Params.ReverseMaxSpeed = Tuning->ReverseMaxSpeedKmh * KmhToCmPerSecond;
	Params.ReverseAcceleration = Tuning->ReverseAcceleration;
	Params.BrakeToReverseSpeed = Tuning->BrakeToReverseSpeedKmh * KmhToCmPerSecond;
	Params.CoastDrag = Tuning->CoastDrag;
	Params.OverspeedDrag = Tuning->OverspeedDrag;
	Params.AirThrustMultiplier = Tuning->AirThrustMultiplier;

	Params.MaxYawRate = FMath::DegreesToRadians(VehicleCVars::Resolve(VehicleCVars::MaxYawRate, Tuning->MaxYawRate));
	Params.YawResponse = VehicleCVars::Resolve(VehicleCVars::YawResponse, Tuning->YawResponse);
	Params.bInvertSteeringInReverse = Tuning->bInvertSteeringInReverse;
	Params.AirSteeringMultiplier = Tuning->AirSteeringMultiplier;

	// Bake the steering curve into samples so the physics step does not touch the asset's curve.
	const FRichCurve* SteeringCurve = Tuning->SteeringBySpeed.GetRichCurveConst();
	for (int32 Index = 0; Index < FHoverSimParams::SteeringSamples; ++Index)
	{
		const float SpeedRatio = FHoverSimParams::SteeringSampleRange * Index / (FHoverSimParams::SteeringSamples - 1);
		Params.SteeringBySpeed[Index] = SteeringCurve->Eval(SpeedRatio, 1.f);
	}

	Params.LateralGrip = VehicleCVars::Resolve(VehicleCVars::LateralGrip, Tuning->LateralGrip);

	Params.DriftGrip = VehicleCVars::Resolve(VehicleCVars::DriftGrip, Tuning->DriftGrip);
	Params.DriftYawMultiplier = Tuning->DriftYawMultiplier;
	Params.DriftMinTurn = Tuning->DriftMinTurn;
	Params.DriftMinSpeed = Tuning->DriftMinSpeedKmh * KmhToCmPerSecond;
	Params.DriftEndSpeed = Tuning->DriftEndSpeedKmh * KmhToCmPerSecond;
	Params.DriftChargeRate = Tuning->DriftChargeRate;
	Params.DriftFullChargeSlipAngle = Tuning->DriftFullChargeSlipAngle;
	Params.DriftCriticalSlipAngle = VehicleCVars::Resolve(VehicleCVars::DriftCriticalAngle, Tuning->DriftCriticalSlipAngle);
	Params.DriftRecoveryTime = Tuning->DriftRecoveryTime;
	Params.OverslideGripMultiplier = Tuning->OverslideGripMultiplier;
	Params.OverslideSteerMultiplier = Tuning->OverslideSteerMultiplier;

	Params.BodyHalfWidth = Tuning->BodySize.Y * 0.5f;
	Params.ClosePassMinSpeed = Tuning->ClosePassMinSpeedKmh * KmhToCmPerSecond;
	Params.ClosePassMaxDistance = Tuning->ClosePassMaxDistance;
	Params.ClosePassFullDistance = FMath::Min(Tuning->ClosePassFullDistance, Tuning->ClosePassMaxDistance);
	Params.ClosePassContactDistance = Tuning->ClosePassContactDistance;
	Params.ClosePassChargeRate = Tuning->ClosePassChargeRate;
	Params.ClosePassHitLockout = Tuning->ClosePassHitLockout;
	Params.ClosePassTraceRadius = FMath::Min(Tuning->ClosePassTraceRadius, Tuning->BodySize.Y * 0.5f);

	Params.BoostSegments = Tuning->BoostSegments;
	Params.BoostDrainRate = Tuning->BoostDrainRate;
	Params.BoostStartCost = Tuning->BoostStartCost;
	Params.BoostMinStartCharge = Tuning->BoostMinStartCharge;
	Params.BoostImpulse = Tuning->BoostImpulse;
	Params.BoostMaxSpeed = Tuning->BoostMaxSpeedKmh * KmhToCmPerSecond;
	Params.BoostAccelMultiplier = Tuning->BoostAccelMultiplier;
#if !UE_BUILD_SHIPPING
	Params.bInfiniteBoost = VehicleCVars::InfiniteBoost.GetValueOnGameThread() != 0;
#endif

	Params.ControlLossGripMultiplier = Tuning->ControlLossGripMultiplier;
	Params.ControlLossSteerMultiplier = Tuning->ControlLossSteerMultiplier;

	Params.AirLevelStrength = Tuning->AirLevelStrength;
	Params.AirLevelDamping = Tuning->AirLevelDamping;

	// Character x modifier stack (passives, items) multipliers on top of the base tuning.
	// Mass, RamPower and Stability are not driving values: the body and the impact component use them.
	if (Stats)
	{
		const FVehicleStatModifiers StatModifiers = Stats->GetModifiers();
		Params.MaxSpeed *= StatModifiers.MaxSpeed;
		Params.BoostMaxSpeed *= StatModifiers.MaxSpeed;
		Params.Acceleration *= StatModifiers.Acceleration;
		Params.MaxYawRate *= StatModifiers.Steering;
		Params.LateralGrip *= StatModifiers.Grip;
		Params.DriftGrip *= StatModifiers.Grip;
	}

	return Params;
}

void UHoverMovementComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);

	UWorld* World = GetWorld();
	if (!World || !UpdatedBody || !Tuning)
	{
		return;
	}

	FBodyInstance* BodyInstance = UpdatedBody->GetBodyInstance();
	if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
	{
		return;
	}

	FPhysicsActorHandle Proxy = BodyInstance->GetPhysicsActor();
	Chaos::FRigidBodyHandle_Internal* Body = Proxy ? Proxy->GetPhysicsThreadAPI() : nullptr;
	if (!Body)
	{
		return;
	}

	const Chaos::EObjectStateType ObjectState = Body->ObjectState();
	if (ObjectState != Chaos::EObjectStateType::Dynamic && ObjectState != Chaos::EObjectStateType::Sleeping)
	{
		return;
	}

	FCollisionQueryParams QueryParams(HoverTraceTag, /*bTraceComplex=*/false, GetOwner());

	// Ground = world geometry that blocks vehicles. Querying by object type leaves other vehicles out
	// (a car never hovers on another car); the response check leaves out triggers, checkpoints and
	// item boxes, which only overlap.
	FCollisionObjectQueryParams GroundObjectTypes;
	GroundObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	// Reused by every hover point of this step, so the sweeps do not allocate each time.
	TArray<FHitResult> Hits;

	auto Trace = [World, &QueryParams, &GroundObjectTypes, &Hits](const FVector& Start, const FVector& End, float Radius, float& OutDistance)
	{
		Hits.Reset();
		World->SweepMultiByObjectType(
			Hits, Start, End, FQuat::Identity,
			GroundObjectTypes, FCollisionShape::MakeSphere(Radius), QueryParams);

		bool bFoundGround = false;
		for (const FHitResult& Hit : Hits)
		{
			const UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!HitComponent || HitComponent->GetCollisionResponseToChannel(ECC_Vehicle) != ECR_Block)
			{
				continue;
			}

			// Ground already touching the point: report zero clearance so the spring pushes at full strength.
			const float Distance = Hit.bStartPenetrating ? 0.f : Hit.Distance;
			if (!bFoundGround || Distance < OutDistance)
			{
				OutDistance = Distance;
				bFoundGround = true;
			}
		}
		return bFoundGround;
	};

	FVehicleInputState StepInput = Input;
	bool bBoostPressed = bBoostPressedLatch;
	bool bAbilityPressed = bAbilityPressedLatch;
#if !UE_BUILD_SHIPPING
	// Scripted input drives the player's car only; test dummies just sit there.
	const int32 ScriptedMode = CosmicHotshots::VehicleCVars::ScriptedInput.GetValueOnGameThread();
	if (bTestBot)
	{
		// Test bots only steer and use the throttle; no boost, no ability.
		StepInput = GetAutopilotInput(*Body, TestBotLateralOffset, TestBotThrottleScale, DeltaTime, 0.f);
		bBoostPressed = false;
		bAbilityPressed = false;
	}
	else if (ScriptedMode != 0 && IsOwnerPlayerControlled())
	{
		if (ScriptedMode == DriftChainMode)
		{
			// Mode 8: the autopilot at reduced pace, with two long right-hand drifts in the first sweeper.
			if (DriftChainStartTime < 0.0 && AutopilotDistance >= DriftChainStartDistance && AutopilotDistance < DriftChainStartDistance + DriftChainStartWindow)
			{
				DriftChainStartTime = ElapsedSimTime;
			}
			const double SinceStart = DriftChainStartTime < 0.0 ? -1.0 : ElapsedSimTime - DriftChainStartTime;
			const bool bFirstDrift = SinceStart >= 0.0 && SinceStart < DriftChainHold;
			const bool bSecondDrift = SinceStart >= DriftChainHold + DriftChainPause && SinceStart < 2.0 * DriftChainHold + DriftChainPause;
			// Slow enough to hold the corner until both drifts are done, then flat out (the next corner ends in a wall).
			const bool bDriftsDone = SinceStart >= 2.0 * DriftChainHold + DriftChainPause + DriftChainSlowAfter;
			StepInput = GetAutopilotInput(*Body, bDriftsDone ? 0.f : DriftChainLateralOffset, 1.f, DeltaTime, bDriftsDone ? 0.f : DriftChainMaxSpeed);
			StepInput.bDriftRight = bFirstDrift || bSecondDrift;
		}
		else
		{
			StepInput = ScriptedMode == AutopilotMode ? GetAutopilotInput(*Body, 0.f, 1.f, DeltaTime, 0.f) : GetScriptedTestInput(ScriptedMode, ElapsedSimTime);
		}
		bBoostPressed = StepInput.bBoost && !bScriptedBoostWasHeld;
		bScriptedBoostWasHeld = StepInput.bBoost;
		bAbilityPressed = StepInput.bUseAbility && !bScriptedAbilityWasHeld;
		bScriptedAbilityWasHeld = StepInput.bUseAbility;
	}
#endif

	// Temporary bonuses first: run durations and fades, then let the passive react to the race state
	// of the previous step. Impacts and the hover step below then use the resulting stats.
	if (Modifiers)
	{
		Modifiers->GetStack().Advance(DeltaTime, SimTime);
#if !UE_BUILD_SHIPPING
		// Test helper: one max-speed modifier with fade in, hold and fade out, to check the stack on its own.
		const float TestSpeedModifier = CosmicHotshots::VehicleCVars::TestSpeedModifier.GetValueOnGameThread();
		if (TestSpeedModifier > 0.f && !bTestModifierApplied && ElapsedSimTime >= TestModifierStartTime && IsOwnerPlayerControlled())
		{
			bTestModifierApplied = true;
			FVehicleModifierSpec Spec;
			Spec.SourceId = TestModifierSource;
			Spec.Stat = EVehicleStat::MaxSpeed;
			Spec.Value = TestSpeedModifier;
			Spec.Duration = TestModifierDuration;
			Spec.FadeIn = TestModifierFade;
			Spec.FadeOut = TestModifierFade;
			Modifiers->GetStack().AddOrRefresh(Spec);
		}
#endif
	}
	if (Passive)
	{
		Passive->StepPassive(DeltaTime, SimTime, SimState, ImpactCounter);
	}

	// Hits from the previous step first, so the hover step below already drives the bounced car.
	ApplyImpacts(*Body, *Proxy);

	const int32 AbilityUsesBefore = SimState.DriftBoost.AbilityUseCount;

	FHoverSim::Step(*Body, BuildSimParams(), StepInput, bBoostPressed, bAbilityPressed, Trace, DeltaTime, SimState);
	bBoostPressedLatch = false;
	bAbilityPressedLatch = false;

#if !UE_BUILD_SHIPPING
	// Placeholder until active abilities exist (Phase 3): make the spent segment visible.
	if (SimState.DriftBoost.AbilityUseCount != AbilityUsesBefore)
	{
		UE_LOG(LogCosmicVehicle, Log, TEXT("ABILITY %s used the (not yet implemented) active ability: one boost segment spent, meter now %.3f"),
			*GetNameSafe(GetOwner()), SimState.DriftBoost.BoostCharge);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, AbilityMessageSeconds, FColor::Cyan, TEXT("Active ability used (placeholder): one boost segment spent"));
		}
	}
#endif

	LogStep(DeltaTime);
}

void UHoverMovementComponent::SetTestBot(float LateralOffset, float ThrottleScale)
{
	bTestBot = true;
	TestBotLateralOffset = LateralOffset;
	TestBotThrottleScale = ThrottleScale;
}

FVehicleInputState UHoverMovementComponent::GetAutopilotInput(const Chaos::FRigidBodyHandle_Internal& Body, float LateralOffset, float ThrottleScale, float DeltaTime, float MaxSpeed)
{
	FVehicleInputState Autopilot;
#if !UE_BUILD_SHIPPING
	if (!AutopilotTrack.IsValid() || ElapsedSimTime < AutopilotSettleEnd)
	{
		return Autopilot;
	}

	const FVector Location(Body.X());
	const FQuat Rotation(Body.R());
	const FTrackProjection Projection = AutopilotDistance < 0.f
		? AutopilotTrack->Project(Location)
		: AutopilotTrack->ProjectNear(Location, AutopilotDistance, AutopilotProjectionWindow, AutopilotReacquireDistance);
	AutopilotDistance = Projection.Distance;

	const float Speed = FVector(Body.V()).Size();
	const FTrackFrame AimFrame = AutopilotTrack->GetFrameAtDistance(
		Projection.Distance + AutopilotLookAheadBase + Speed * AutopilotLookAheadTime);
	const FVector AimPoint = AimFrame.Location + AimFrame.Right * LateralOffset;
	const FVector LocalAim = Rotation.UnrotateVector(AimPoint - Location);
	const float AngleToAim = FMath::RadiansToDegrees(FMath::Atan2(LocalAim.Y, LocalAim.X));

	// Stuck against a wall or another car: back off for a moment, turning the nose towards the aim point
	// (steering is inverted in reverse, like a car, so steer away from the aim).
	if (AutopilotReverseTimeRemaining > 0.f)
	{
		AutopilotReverseTimeRemaining -= DeltaTime;
		Autopilot.Brake = 1.f;
		Autopilot.Steer = AngleToAim >= 0.f ? -1.f : 1.f;
		return Autopilot;
	}

	AutopilotStuckTime = Speed < AutopilotStuckSpeed ? AutopilotStuckTime + DeltaTime : 0.f;
	if (AutopilotStuckTime > AutopilotStuckDelay)
	{
		AutopilotStuckTime = 0.f;
		AutopilotReverseTimeRemaining = AutopilotReverseDuration;
	}

	Autopilot.Steer = FMath::Clamp(AngleToAim / AutopilotFullSteerAngle, -1.f, 1.f);
	Autopilot.Throttle = (FMath::Abs(AngleToAim) > AutopilotSlowAngle ? AutopilotSlowThrottle : 1.f) * ThrottleScale;
	if (MaxSpeed > 0.f && Speed > MaxSpeed)
	{
		Autopilot.Throttle = 0.f;
	}
#endif
	return Autopilot;
}

bool UHoverMovementComponent::IsOwnerPlayerControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsPlayerControlled();
}

void UHoverMovementComponent::LogStep(float DeltaTime)
{
	ElapsedSimTime += DeltaTime;

#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	const float LogInterval = VehicleCVars::LogInterval.GetValueOnGameThread();
	if (LogInterval > 0.f && ElapsedSimTime >= NextLogTime)
	{
		NextLogTime = ElapsedSimTime + LogInterval;
		UE_LOG(LogCosmicVehicle, Log,
			TEXT("VEHICLE %s t=%.3f dt=%.5f height=%.2f kmh=%.2f lateral=%.1f yawRate=%.2f yaw=%.2f slip=%.2f x=%.0f y=%.0f z=%.1f grounded=%d drift=%d overslide=%.2f charge=%.3f boost=%d wall=%.0f closePass=%d lockout=%.2f"),
			*GetNameSafe(GetOwner()), ElapsedSimTime, SimState.DeltaTime, SimState.AverageHeight,
			SimState.ForwardSpeed * CmPerSecondToKmh, SimState.LateralSpeed, SimState.YawRate, SimState.Yaw, SimState.SlipAngle,
			SimState.Location.X, SimState.Location.Y, SimState.Location.Z,
			SimState.bGrounded ? 1 : 0,
			SimState.DriftBoost.bDrifting ? 1 : 0, SimState.DriftBoost.OverslideTimeRemaining, SimState.DriftBoost.BoostCharge, SimState.DriftBoost.bBoosting ? 1 : 0,
			SimState.WallClearance, SimState.DriftBoost.bClosePassCharging ? 1 : 0, SimState.DriftBoost.ClosePassLockoutRemaining);

		// Only while something is in the modifier stack, so runs without modifiers log exactly what they always did.
		if (Modifiers && !Modifiers->GetStack().GetModifiers().IsEmpty())
		{
			const FVehicleStatModifiers& Totals = Modifiers->GetStack().GetTotals();
			const FVehicleImpactModifiers& ImpactTotals = Modifiers->GetStack().GetImpactTotals();
			UE_LOG(LogCosmicVehicle, Log, TEXT("MODS %s t=%.3f count=%d maxSpeed=%.4f accel=%.4f steering=%.4f grip=%.4f ram=%.4f stability=%.4f sideRam=%.4f sideLoss=%.4f recovery=%.4f hitLoss=%.4f"),
				*GetNameSafe(GetOwner()), ElapsedSimTime, Modifiers->GetStack().GetModifiers().Num(),
				Totals.MaxSpeed, Totals.Acceleration, Totals.Steering, Totals.Grip, Totals.RamPower, Totals.Stability,
				ImpactTotals.SideRamPower, ImpactTotals.SideSpeedLoss, ImpactTotals.HitRecoveryTime, ImpactTotals.HitSpeedLoss);
		}
	}
#endif
}

void UHoverMovementComponent::RunTestHelpers()
{
#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	// Test helpers belong to the player's car only (a test dummy has this component too).
	if (!IsOwnerPlayerControlled())
	{
		return;
	}

	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();

	const float StartBoostCharge = VehicleCVars::StartBoostCharge.GetValueOnGameThread();
	if (StartBoostCharge >= 0.f && !bTestBoostChargeApplied)
	{
		bTestBoostChargeApplied = true;
		SimState.DriftBoost.BoostCharge = FMath::Clamp(StartBoostCharge, 0.f, FDriftBoostState::MaxBoostCharge);
	}

	const int32 PlayerCharacterIndex = VehicleCVars::PlayerCharacter.GetValueOnGameThread();
	if (PlayerCharacterIndex >= 0 && !bTestCharacterApplied)
	{
		bTestCharacterApplied = true;
		if (ARaceVehicle* Vehicle = Cast<ARaceVehicle>(GetOwner()))
		{
			// With -DPCVars the game mode already did this before the first physics step; do not apply it twice.
			const UCharacterDefinition* TestCharacter = Settings->FindCharacter(FString::FromInt(PlayerCharacterIndex));
			if (Vehicle->GetCharacter() != TestCharacter)
			{
				Vehicle->SetCharacter(TestCharacter);
			}
		}
	}

	// The dummy car (ch.Test.SpawnDummyAhead) is spawned by ARaceGameMode, before the first physics step.

	const float ScreenshotTime = VehicleCVars::ScreenshotTime.GetValueOnGameThread();
	if (ScreenshotTime > 0.f && !bTestScreenshotTaken && ElapsedSimTime >= ScreenshotTime)
	{
		bTestScreenshotTaken = true;
		FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/true);
	}

	const float AutoQuitTime = VehicleCVars::AutoQuitTime.GetValueOnGameThread();
	if (AutoQuitTime > 0.f && ElapsedSimTime >= AutoQuitTime)
	{
		FPlatformMisc::RequestExit(false);
	}
#endif
}

void UHoverMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	// Run here, in the normal game tick: spawning actors or quitting from inside the physics step is not allowed.
	RunTestHelpers();

	if (CosmicHotshots::VehicleCVars::DebugDraw.GetValueOnGameThread() != 0)
	{
		DrawDebug();
	}
#endif
}

void UHoverMovementComponent::DrawDebug() const
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FString Compressions;
	for (const FHoverPointState& Point : SimState.Points)
	{
		const FColor Color = !Point.bOverGround ? FColor::Red : (Point.Compression > 0.f ? FColor::Green : FColor::Yellow);
		DrawDebugLine(World, Point.WorldPosition, Point.TraceEnd, Color);
		DrawDebugPoint(World, Point.WorldPosition, DebugPointSize, Color);
		Compressions += FString::Printf(TEXT(" %.1f"), Point.Compression);
	}

	// Close-pass sweeps: grey = no wall in reach, green = wall in reach, red = touching / locked out.
	for (const FWallProbeState& Probe : SimState.WallProbes)
	{
		const bool bLocked = SimState.DriftBoost.ClosePassLockoutRemaining > 0.f;
		const FColor Color = Probe.Clearance < 0.f ? FColor::Silver : (bLocked ? FColor::Red : FColor::Green);
		DrawDebugLine(World, Probe.Start, Probe.End, Color);
	}

	if (GEngine)
	{
		const FString Text = FString::Printf(
			TEXT("%s\nSpeed: %.0f km/h   Sideways: %.0f cm/s   Slip angle: %.1f deg\nTurn rate: %.0f deg/s\nInput: throttle %.2f  brake %.2f  steer %.2f  drift %s  boost %d\nDrift: %s   Boost meter: %.0f%%   Boosting: %s\nWall gap: %.0f cm   Close pass: %s\nHeight: %.1f cm   Grounded: %s (%d points)\nCompression (cm):%s\nPhysics step: %.2f ms"),
			*GetNameSafe(GetOwner()),
			SimState.ForwardSpeed * CmPerSecondToKmh, SimState.LateralSpeed, SimState.SlipAngle,
			SimState.YawRate,
			Input.Throttle, Input.Brake, Input.Steer, Input.bDriftLeft ? TEXT("L") : (Input.bDriftRight ? TEXT("R") : TEXT("-")), Input.bBoost ? 1 : 0,
			SimState.DriftBoost.IsOversliding() ? TEXT("LOST CONTROL") : !SimState.DriftBoost.bDrifting ? TEXT("no") : (SimState.DriftBoost.DriftDirection > 0.f ? TEXT("RIGHT") : TEXT("LEFT")),
			SimState.DriftBoost.BoostCharge * 100.f, SimState.DriftBoost.bBoosting ? TEXT("yes") : TEXT("no"),
			SimState.WallClearance,
			SimState.DriftBoost.bClosePassCharging ? TEXT("CHARGING") : (SimState.DriftBoost.ClosePassLockoutRemaining > 0.f ? TEXT("locked (wall contact)") : TEXT("no")),
			SimState.AverageHeight,
			SimState.bGrounded ? TEXT("yes") : TEXT("no"), SimState.PointsOverGround,
			*Compressions,
			SimState.DeltaTime * 1000.f);

		// Stable key per vehicle so the text updates in place.
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::White, Text);
	}
#endif
}
