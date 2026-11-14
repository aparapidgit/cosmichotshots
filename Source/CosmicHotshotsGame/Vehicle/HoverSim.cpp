#include "Vehicle/HoverSim.h"

#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace
{
	// Below this speed (cm/s) the slip angle is reported as zero: direction of travel is meaningless when standing.
	constexpr double MinSpeedForSlipAngle = 50.0;

	/** Torque that gives the body the requested angular acceleration (world space), using its real inertia. */
	FVector TorqueForAngularAcceleration(const Chaos::FRigidBodyHandle_Internal& Body, const FQuat& Rotation, const FVector& AngularAcceleration)
	{
		const FQuat MassRotation = Rotation * FQuat(Body.RotationOfMass());
		const FVector Inertia(Body.I());
		const FVector LocalAcceleration = MassRotation.UnrotateVector(AngularAcceleration);
		return MassRotation.RotateVector(LocalAcceleration * Inertia);
	}
}

float FHoverSimParams::GetSteeringMultiplier(float SpeedRatio) const
{
	const float Position = FMath::Clamp(SpeedRatio / SteeringSampleRange, 0.f, 1.f) * (SteeringSamples - 1);
	const int32 Index = FMath::Min(FMath::FloorToInt32(Position), SteeringSamples - 2);
	return FMath::Lerp(SteeringBySpeed[Index], SteeringBySpeed[Index + 1], Position - Index);
}

void FHoverSim::Step(
	Chaos::FRigidBodyHandle_Internal& Body,
	const FHoverSimParams& Params,
	const FVehicleInputState& Input,
	bool bBoostPressed,
	FHoverTraceFunction Trace,
	float DeltaTime,
	FHoverSimState& OutState)
{
	const FVector Location = Body.X();
	const FQuat Rotation = Body.R();
	const FVector LinearVelocity = Body.V();
	const FVector AngularVelocity = Body.W();
	const double Mass = Body.M();
	const FVector CenterOfMass = Location + Rotation.RotateVector(Body.CenterOfMass());
	const FVector Forward = Rotation.GetForwardVector();
	const FVector Right = Rotation.GetRightVector();
	const FVector Up = Rotation.GetUpVector();

	const int32 NumPoints = Params.HoverPoints.Num();

	OutState.Points.SetNum(NumPoints, EAllowShrinking::No);
	OutState.PointsOverGround = 0;
	OutState.Location = Location;
	OutState.Velocity = LinearVelocity;
	OutState.DeltaTime = DeltaTime;

	// ------------------------------------------------------------------ Hover
	// Spring and damper from frequency and damping ratio, so the feel does not change with mass.
	// k = m * w^2, c = 2 * zeta * m * w, shared equally by all points.
	const double AngularFrequency = UE_DOUBLE_TWO_PI * Params.HoverFrequency;
	const double PointShare = NumPoints > 0 ? 1.0 / NumPoints : 0.0;
	const double Stiffness = Mass * FMath::Square(AngularFrequency) * PointShare;
	const double Damping = 2.0 * Params.HoverDampingRatio * Mass * AngularFrequency * PointShare;

	// The spring is longer than the ride height by the amount gravity compresses it at rest,
	// so the resting clearance is exactly RideHeight.
	const double RestCompression = AngularFrequency > UE_DOUBLE_SMALL_NUMBER
		? Params.GravityAcceleration / FMath::Square(AngularFrequency)
		: 0.0;
	const double SpringLength = Params.RideHeight + RestCompression;

	// The sweep starts above the point (inside the body) so it still works when the body is very low.
	const double StartLift = Params.TraceRadius;
	const double SweepLength = StartLift + SpringLength + Params.GroundedExtraReach - Params.TraceRadius;

	double HeightSum = 0.0;

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		FHoverPointState& Point = OutState.Points[Index];

		const FVector WorldPoint = Location + Rotation.RotateVector(Params.HoverPoints[Index]);
		const FVector Start = WorldPoint + Up * StartLift;
		const FVector End = Start - Up * SweepLength;

		Point.WorldPosition = WorldPoint;
		Point.TraceEnd = End;
		Point.Compression = 0.f;

		float Distance = 0.f;
		Point.bOverGround = Trace(Start, End, Params.TraceRadius, Distance);
		if (!Point.bOverGround)
		{
			Point.Height = 0.f;
			continue;
		}

		// Sphere centre travelled Distance from Start; the surface is one radius further.
		const double Height = Distance - StartLift + Params.TraceRadius;
		Point.Height = static_cast<float>(Height);
		HeightSum += Height;
		++OutState.PointsOverGround;

		const double Compression = SpringLength - Height;
		if (Compression <= 0.0)
		{
			continue;
		}
		Point.Compression = static_cast<float>(Compression);

		// Velocity of this point along the push direction (positive = moving away from the ground).
		const FVector PointVelocity = LinearVelocity + FVector::CrossProduct(AngularVelocity, WorldPoint - CenterOfMass);
		const double ClosingSpeed = FVector::DotProduct(PointVelocity, Up);

		// A hover thruster only pushes, it never pulls the car down.
		const double ForceMagnitude = FMath::Max(0.0, Stiffness * Compression - Damping * ClosingSpeed);
		const FVector Force = Up * ForceMagnitude;

		Body.AddForce(Force);
		Body.AddTorque(FVector::CrossProduct(WorldPoint - CenterOfMass, Force));
	}

	const bool bGrounded = OutState.PointsOverGround >= Params.MinGroundedPoints;
	OutState.bGrounded = bGrounded;
	OutState.AverageHeight = OutState.PointsOverGround > 0
		? static_cast<float>(HeightSum / OutState.PointsOverGround)
		: -1.f;

	// ---------------------------------------------------------------- Gravity
	// Custom gravity (engine gravity is disabled on the body). Stronger in the air.
	const double GravityScale = bGrounded ? 1.0 : Params.AirGravityMultiplier;
	Body.AddForce(FVector::DownVector * (Mass * Params.GravityAcceleration * GravityScale));

	// ------------------------------------------------------------------ Drive
	const double ForwardSpeed = FVector::DotProduct(LinearVelocity, Forward);
	const double LateralSpeed = FVector::DotProduct(LinearVelocity, Right);

	const double SlipAngle = LinearVelocity.SizeSquared() > FMath::Square(MinSpeedForSlipAngle)
		? FMath::RadiansToDegrees(FMath::Atan2(LateralSpeed, FMath::Abs(ForwardSpeed)))
		: 0.0;
	const double SteerInput = FMath::Clamp(Input.Steer, -1.f, 1.f);

	// --------------------------------------------------------- Control loss
	// A strong hit (wobble, spin-out) briefly weakens grip and steering. A drift cannot survive it.
	const bool bControlLoss = OutState.ControlLossTimeRemaining > 0.f;
	OutState.ControlLossTimeRemaining = FMath::Max(OutState.ControlLossTimeRemaining - DeltaTime, 0.f);
	const double ControlGripScale = bControlLoss ? Params.ControlLossGripMultiplier : 1.0;
	const double ControlSteerScale = bControlLoss ? Params.ControlLossSteerMultiplier : 1.0;

	// ---------------------------------------------------------- Drift state
	FDriftBoostState& DriftBoost = OutState.DriftBoost;

	if (bControlLoss)
	{
		DriftBoost.bDrifting = false;
		DriftBoost.DriftDirection = 0.f;
	}
	else if (!DriftBoost.bDrifting)
	{
		// A drift needs the button, the ground, enough speed and a steering direction to commit to.
		if (Input.bDrift && bGrounded && ForwardSpeed >= Params.DriftMinSpeed && FMath::Abs(SteerInput) >= Params.DriftMinSteer)
		{
			DriftBoost.bDrifting = true;
			DriftBoost.DriftDirection = SteerInput > 0.0 ? 1.f : -1.f;
		}
	}
	else if (!Input.bDrift || ForwardSpeed < Params.DriftEndSpeed)
	{
		DriftBoost.bDrifting = false;
		DriftBoost.DriftDirection = 0.f;
	}

	// Drifting fills the boost meter; a bigger slide fills it faster. No charge in the air.
	if (DriftBoost.bDrifting && bGrounded)
	{
		const double SlideAmount = FMath::Clamp(FMath::Abs(SlipAngle) / Params.DriftFullChargeSlipAngle, 0.0, 1.0);
		DriftBoost.BoostCharge += static_cast<float>(Params.DriftChargeRate * SlideAmount * DeltaTime);
	}
	if (Params.bInfiniteBoost)
	{
		DriftBoost.BoostCharge = FDriftBoostState::MaxBoostCharge;
	}
	DriftBoost.BoostCharge = FMath::Clamp(DriftBoost.BoostCharge, 0.f, FDriftBoostState::MaxBoostCharge);

	// ---------------------------------------------------------- Boost state
	double BoostKickAcceleration = 0.0;
	if (bBoostPressed && DriftBoost.BoostCharge >= Params.BoostCost)
	{
		DriftBoost.BoostCharge -= Params.BoostCost;
		DriftBoost.BoostTimeRemaining = Params.BoostDuration;
		++DriftBoost.BoostCount;

		// The kick: an instant speed gain, delivered as one step of acceleration.
		BoostKickAcceleration = DeltaTime > UE_SMALL_NUMBER ? Params.BoostImpulse / DeltaTime : 0.0;
	}

	const bool bBoosting = DriftBoost.IsBoosting();
	DriftBoost.BoostTimeRemaining = FMath::Max(DriftBoost.BoostTimeRemaining - DeltaTime, 0.f);

	// While boosting the car pushes at full throttle towards a higher top speed.
	const double MaxSpeed = bBoosting ? FMath::Max(Params.BoostMaxSpeed, Params.MaxSpeed) : Params.MaxSpeed;
	const double Acceleration = bBoosting ? Params.Acceleration * Params.BoostAccelMultiplier : Params.Acceleration;

	const double Throttle = bBoosting ? 1.0 : FMath::Clamp(Input.Throttle, 0.f, 1.f);
	const double Brake = FMath::Clamp(Input.Brake, 0.f, 1.f);

	double DriveAcceleration = 0.0;

	if (Throttle > 0.0)
	{
		const double SpeedRatio = FMath::Clamp(ForwardSpeed / MaxSpeed, 0.0, 1.0);
		DriveAcceleration += Acceleration * Throttle * (1.0 - FMath::Pow(SpeedRatio, static_cast<double>(Params.AccelFalloffExponent)));
	}

	if (Brake > 0.0)
	{
		if (ForwardSpeed > Params.BrakeToReverseSpeed)
		{
			DriveAcceleration -= Params.BrakeDeceleration * Brake;
		}
		else
		{
			const double ReverseRatio = FMath::Clamp(-ForwardSpeed / Params.ReverseMaxSpeed, 0.0, 1.0);
			DriveAcceleration -= Params.ReverseAcceleration * Brake * (1.0 - FMath::Pow(ReverseRatio, static_cast<double>(Params.AccelFalloffExponent)));
		}
	}

	if (!bGrounded)
	{
		DriveAcceleration *= Params.AirThrustMultiplier;
	}

	// Drag only acts on the ground: in the air the car keeps its momentum.
	if (bGrounded)
	{
		if (Throttle <= 0.0 && Brake <= 0.0)
		{
			DriveAcceleration -= ForwardSpeed * Params.CoastDrag;
		}
		if (ForwardSpeed > MaxSpeed)
		{
			DriveAcceleration -= (ForwardSpeed - MaxSpeed) * Params.OverspeedDrag;
		}
	}

	Body.AddForce(Forward * ((DriveAcceleration + BoostKickAcceleration) * Mass));

	// ------------------------------------------------------------------- Grip
	// Sideways velocity damping. This is what makes the hovercar hold a line instead of sliding.
	// Drifting lowers it, so the car slides.
	if (bGrounded)
	{
		const double Grip = (DriftBoost.bDrifting ? Params.DriftGrip : Params.LateralGrip) * ControlGripScale;
		Body.AddForce(Right * (-LateralSpeed * Grip * Mass));
	}

	// --------------------------------------------------------------- Steering
	// Drive the turn rate around the car's up axis towards the requested rate.
	const double YawRate = FVector::DotProduct(AngularVelocity, Up);
	const float SpeedRatioForSteering = static_cast<float>(FMath::Abs(ForwardSpeed) / Params.MaxSpeed);

	// A drift always turns the way it was started: the stick only widens or tightens the line.
	double EffectiveSteer = SteerInput;
	double YawRateScale = 1.0;
	if (DriftBoost.bDrifting)
	{
		const double IntoDrift = (SteerInput * DriftBoost.DriftDirection + 1.0) * 0.5; // 0 = against, 1 = fully into
		EffectiveSteer = DriftBoost.DriftDirection * FMath::Lerp(static_cast<double>(Params.DriftMinTurn), 1.0, IntoDrift);
		YawRateScale = Params.DriftYawMultiplier;
	}

	double TargetYawRate = EffectiveSteer * YawRateScale * ControlSteerScale * Params.MaxYawRate * Params.GetSteeringMultiplier(SpeedRatioForSteering);
	if (Params.bInvertSteeringInReverse && ForwardSpeed < -Params.BrakeToReverseSpeed)
	{
		TargetYawRate = -TargetYawRate;
	}
	if (!bGrounded)
	{
		TargetYawRate *= Params.AirSteeringMultiplier;
	}

	// During control loss the car also holds its heading less firmly, so a spin-out can actually spin.
	FVector AngularAcceleration = Up * ((TargetYawRate - YawRate) * Params.YawResponse * ControlSteerScale);

	// ------------------------------------------------------------ Air control
	// On the ground the hover points keep the car level with the surface. In the air nothing does,
	// so level pitch and roll towards the horizon to land flat.
	if (!bGrounded)
	{
		const FVector LevelError = FVector::CrossProduct(Up, FVector::UpVector);
		const FVector PitchRollVelocity = AngularVelocity - Up * YawRate;
		AngularAcceleration += LevelError * Params.AirLevelStrength - PitchRollVelocity * Params.AirLevelDamping;
	}

	Body.AddTorque(TorqueForAngularAcceleration(Body, Rotation, AngularAcceleration));

	// ------------------------------------------------------------------ State
	OutState.ForwardSpeed = static_cast<float>(ForwardSpeed);
	OutState.LateralSpeed = static_cast<float>(LateralSpeed);
	OutState.YawRate = static_cast<float>(FMath::RadiansToDegrees(YawRate));
	OutState.SlipAngle = static_cast<float>(SlipAngle);
	OutState.Yaw = static_cast<float>(Rotation.Rotator().Yaw);
}
