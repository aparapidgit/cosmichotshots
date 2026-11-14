#include "Vehicle/HoverMovementComponent.h"

#include "CosmicHotshotsGame.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Vehicle/VehicleDebug.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	const FName HoverTraceTag(TEXT("HoverTrace"));

	constexpr float KmhToCmPerSecond = 100000.f / 3600.f;
	constexpr float CmPerSecondToKmh = 1.f / KmhToCmPerSecond;

	// Debug display only.
	constexpr float DebugPointSize = 8.f;

#if !UE_BUILD_SHIPPING
	/**
	 * Fixed input sequence for the headless frame-rate test (ch.Test.ScriptedInput 1).
	 * Driven by simulation time, so every frame rate gets exactly the same input.
	 */
	FVehicleInputState GetScriptedDriftBoostInput(double SimTime);

	FVehicleInputState GetScriptedTestInput(int32 Mode, double SimTime)
	{
		constexpr int32 DriftBoostMode = 2;
		if (Mode == DriftBoostMode)
		{
			return GetScriptedDriftBoostInput(SimTime);
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
		constexpr double BoostPressEnd = 5.4;
		constexpr double ThrottleEnd = 6.0;
		constexpr float DriftStartSteer = -0.3f;
		constexpr float DriftWidenSteer = 1.f;
		constexpr float StraightenSteer = 0.6f;

		FVehicleInputState Scripted;
		Scripted.Throttle = (SimTime >= SettleEnd && SimTime < ThrottleEnd) ? 1.f : 0.f;
		Scripted.bDrift = SimTime >= DriftStart && SimTime < DriftEnd;
		Scripted.bBoost = SimTime >= BoostPressStart && SimTime < BoostPressEnd;

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

void UHoverMovementComponent::SetInput(const FVehicleInputState& InInput)
{
	if (InInput.bBoost && !Input.bBoost)
	{
		bBoostPressedLatch = true;
	}
	Input = InInput;
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
	Params.DriftMinSteer = Tuning->DriftMinSteer;
	Params.DriftChargeRate = Tuning->DriftChargeRate;
	Params.DriftFullChargeSlipAngle = Tuning->DriftFullChargeSlipAngle;

	Params.BoostCost = Tuning->BoostCost;
	Params.BoostDuration = Tuning->BoostDuration;
	Params.BoostImpulse = Tuning->BoostImpulse;
	Params.BoostMaxSpeed = Tuning->BoostMaxSpeedKmh * KmhToCmPerSecond;
	Params.BoostAccelMultiplier = Tuning->BoostAccelMultiplier;
#if !UE_BUILD_SHIPPING
	Params.bInfiniteBoost = VehicleCVars::InfiniteBoost.GetValueOnGameThread() != 0;
#endif

	Params.AirLevelStrength = Tuning->AirLevelStrength;
	Params.AirLevelDamping = Tuning->AirLevelDamping;

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
#if !UE_BUILD_SHIPPING
	const int32 ScriptedMode = CosmicHotshots::VehicleCVars::ScriptedInput.GetValueOnGameThread();
	if (ScriptedMode != 0)
	{
		StepInput = GetScriptedTestInput(ScriptedMode, ElapsedSimTime);
		bBoostPressed = StepInput.bBoost && !bScriptedBoostWasHeld;
		bScriptedBoostWasHeld = StepInput.bBoost;
	}
#endif

	FHoverSim::Step(*Body, BuildSimParams(), StepInput, bBoostPressed, Trace, DeltaTime, SimState);
	bBoostPressedLatch = false;

	LogAndTestHooks(DeltaTime);
}

void UHoverMovementComponent::LogAndTestHooks(float DeltaTime)
{
	ElapsedSimTime += DeltaTime;

#if !UE_BUILD_SHIPPING
	using namespace CosmicHotshots;

	const float LogInterval = VehicleCVars::LogInterval.GetValueOnGameThread();
	if (LogInterval > 0.f && ElapsedSimTime >= NextLogTime)
	{
		NextLogTime = ElapsedSimTime + LogInterval;
		UE_LOG(LogCosmicVehicle, Log,
			TEXT("VEHICLE %s t=%.3f dt=%.5f height=%.2f kmh=%.2f lateral=%.1f yawRate=%.2f yaw=%.2f slip=%.2f x=%.0f y=%.0f z=%.1f grounded=%d drift=%d charge=%.3f boost=%.2f"),
			*GetNameSafe(GetOwner()), ElapsedSimTime, SimState.DeltaTime, SimState.AverageHeight,
			SimState.ForwardSpeed * CmPerSecondToKmh, SimState.LateralSpeed, SimState.YawRate, SimState.Yaw, SimState.SlipAngle,
			SimState.Location.X, SimState.Location.Y, SimState.Location.Z,
			SimState.bGrounded ? 1 : 0,
			SimState.DriftBoost.bDrifting ? 1 : 0, SimState.DriftBoost.BoostCharge, SimState.DriftBoost.BoostTimeRemaining);
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

	if (GEngine)
	{
		const FString Text = FString::Printf(
			TEXT("%s\nSpeed: %.0f km/h   Sideways: %.0f cm/s   Slip angle: %.1f deg\nTurn rate: %.0f deg/s\nInput: throttle %.2f  brake %.2f  steer %.2f  drift %d  boost %d\nDrift: %s   Boost meter: %.0f%%   Boost time left: %.2f s\nHeight: %.1f cm   Grounded: %s (%d points)\nCompression (cm):%s\nPhysics step: %.2f ms"),
			*GetNameSafe(GetOwner()),
			SimState.ForwardSpeed * CmPerSecondToKmh, SimState.LateralSpeed, SimState.SlipAngle,
			SimState.YawRate,
			Input.Throttle, Input.Brake, Input.Steer, Input.bDrift ? 1 : 0, Input.bBoost ? 1 : 0,
			!SimState.DriftBoost.bDrifting ? TEXT("no") : (SimState.DriftBoost.DriftDirection > 0.f ? TEXT("RIGHT") : TEXT("LEFT")),
			SimState.DriftBoost.BoostCharge * 100.f, SimState.DriftBoost.BoostTimeRemaining,
			SimState.AverageHeight,
			SimState.bGrounded ? TEXT("yes") : TEXT("no"), SimState.PointsOverGround,
			*Compressions,
			SimState.DeltaTime * 1000.f);

		// Stable key per vehicle so the text updates in place.
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::White, Text);
	}
#endif
}
