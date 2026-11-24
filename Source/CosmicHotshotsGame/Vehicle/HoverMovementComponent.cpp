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
#include "UnrealClient.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Vehicle/VehicleDebug.h"
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

	// Test helper: the dummy car is spawned once the player's car has settled.
	constexpr double TestDummySpawnTime = 0.5;

#if !UE_BUILD_SHIPPING
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

	FVehicleImpactSim::Apply(Body, BuildImpactParams(), ResolvedContacts, ElapsedSimTime, ImpactState, SimState, PendingImpacts);
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

	// Character (and later passive/item) multipliers on top of the base tuning.
	// Mass, RamPower and Stability are not driving values: the body and the impact component use them.
	if (Stats)
	{
		const FVehicleStatModifiers& Modifiers = Stats->GetModifiers();
		Params.MaxSpeed *= Modifiers.MaxSpeed;
		Params.BoostMaxSpeed *= Modifiers.MaxSpeed;
		Params.Acceleration *= Modifiers.Acceleration;
		Params.MaxYawRate *= Modifiers.Steering;
		Params.LateralGrip *= Modifiers.Grip;
		Params.DriftGrip *= Modifiers.Grip;
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
	if (ScriptedMode != 0 && IsOwnerPlayerControlled())
	{
		StepInput = GetScriptedTestInput(ScriptedMode, ElapsedSimTime);
		bBoostPressed = StepInput.bBoost && !bScriptedBoostWasHeld;
		bScriptedBoostWasHeld = StepInput.bBoost;
		bAbilityPressed = StepInput.bUseAbility && !bScriptedAbilityWasHeld;
		bScriptedAbilityWasHeld = StepInput.bUseAbility;
	}
#endif

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
			Vehicle->SetCharacter(Settings->FindCharacter(FString::FromInt(PlayerCharacterIndex)));
		}
	}

	const float DummyDistance = VehicleCVars::SpawnDummyAhead.GetValueOnGameThread();
	if (DummyDistance > 0.f && !bTestDummySpawned && ElapsedSimTime >= TestDummySpawnTime)
	{
		bTestDummySpawned = true;

		const AActor* Owner = GetOwner();
		const FVector Location = Owner->GetActorLocation()
			+ Owner->GetActorForwardVector() * DummyDistance
			+ Owner->GetActorRightVector() * VehicleCVars::DummyLateralOffset.GetValueOnGameThread();

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Dummy = GetWorld()->SpawnActor<AActor>(Owner->GetClass(), Location, Owner->GetActorRotation(), SpawnParameters);

		const int32 DummyCharacterIndex = VehicleCVars::DummyCharacter.GetValueOnGameThread();
		if (ARaceVehicle* DummyVehicle = Cast<ARaceVehicle>(Dummy); DummyVehicle && DummyCharacterIndex >= 0)
		{
			DummyVehicle->SetCharacter(Settings->FindCharacter(FString::FromInt(DummyCharacterIndex)));
		}
	}

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
