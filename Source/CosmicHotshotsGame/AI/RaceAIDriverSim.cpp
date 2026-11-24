#include "AI/RaceAIDriverSim.h"

#include "Race/TrackSplineTable.h"

namespace
{
	/** The car drops onto the track and settles before the driver touches anything (s). */
	constexpr double SettleTime = 1.0;

	// Track projection (same idea as the race tracker): search near the last position, whole lap when lost.
	constexpr float ProjectionWindow = 3000.f;
	constexpr float ReacquireDistance = 2500.f;

	// Speed planning: look this far ahead for corners, in steps (cm).
	constexpr float BrakeLookaheadMargin = 2000.f;
	constexpr float BrakeLookaheadMax = 30000.f;
	constexpr float BrakeLookaheadStep = 500.f;
	/** Curvature below this is a straight (1 / cm). */
	constexpr float StraightCurvature = 0.00002f;

	// Steering: aim at a point this far ahead on the line (cm, plus speed x time).
	constexpr float AimLookaheadBase = 1200.f;
	constexpr float AimLookaheadTime = 0.45f;
	/** Angle to the aim point (degrees) that gives full steering lock. */
	constexpr float FullSteerAngle = 20.f;
	/** Pointing this far away from the aim point (degrees): ease off instead of accelerating the wrong way. */
	constexpr float OffLineAngle = 35.f;
	constexpr float OffLineThrottle = 0.4f;

	// Racing line: the corner the driver positions itself for lies this far ahead (cm, plus speed x time).
	constexpr float LineLookaheadBase = 1500.f;
	constexpr float LineLookaheadTime = 0.6f;

	// Throttle and brake: full brake when this much too fast, full throttle when this much too slow (cm/s).
	constexpr float BrakeBand = 400.f;
	constexpr float ThrottleBand = 150.f;
	/** Below this forward speed the brake is not used (it would turn into reverse). */
	constexpr float MinBrakingSpeed = 300.f;

	// Other cars: react to a slower car this far ahead (cm, plus speed x time) that is within the width (cm) of the line.
	constexpr float AvoidAheadBase = 1500.f;
	constexpr float AvoidAheadTime = 0.8f;
	constexpr float AvoidWidth = 330.f;
	/** Pass with the car centres this far apart sideways (cm). */
	constexpr float AvoidClearance = 420.f;
	/** A car must be at least this much slower (cm/s) before the driver bothers to go around it. */
	constexpr float AvoidMinClosingSpeed = 100.f;
	/** Closer than this (cm) behind a car it cannot get around yet: match its speed. */
	constexpr float TailgateDistance = 900.f;

	// Stuck: below this speed (cm/s) for the delay (s), reverse for the duration (s).
	constexpr float StuckSpeed = 300.f;
	constexpr float StuckDelay = 1.f;
	constexpr float ReverseDuration = 1.2f;

	// Drift and boost.
	/** The corner the drift button reacts to lies speed x this time ahead (s). */
	constexpr float DriftLookaheadTime = 0.25f;
	/** A boost needs this much straight track ahead (cm), checked in steps. */
	constexpr float BoostStraightLength = 12000.f;
	constexpr float BoostStraightStep = 1000.f;
	/** Nose within this angle of the aim point (degrees) before boosting. */
	constexpr float BoostMaxAimAngle = 8.f;
	/** A running boost is kept until the meter is this low. */
	constexpr float BoostStopCharge = 0.02f;
}

FVehicleInputState FRaceAIDriverSim::Step(const FTrackSplineTable& Track, const FRaceAIDriverParams& Params,
	const FRaceAIDriverInput& Input, FRaceAIDriverState& State)
{
	FVehicleInputState Out;
	if (!Track.IsValid())
	{
		return Out;
	}

	const FTrackProjection Projection = State.TrackDistance < 0.f
		? Track.Project(Input.Location)
		: Track.ProjectNear(Input.Location, State.TrackDistance, ProjectionWindow, ReacquireDistance);
	State.TrackDistance = Projection.Distance;
	State.LateralOffset = Projection.LateralOffset;

	if (Input.ElapsedSimTime < SettleTime)
	{
		State.Mode = ERaceAIDriverMode::Waiting;
		State.TargetLateral = Projection.LateralOffset;
		return Out;
	}

	const float Distance = State.TrackDistance;
	const float Speed = static_cast<float>(Input.Velocity.Size());
	const float ForwardSpeed = static_cast<float>(FVector::DotProduct(Input.Velocity, Input.Rotation.GetForwardVector()));

	// --- Speed: the fastest speed from which every corner ahead can still be reached by braking.
	const float TopTarget = Params.TopSpeed * Params.TopSpeedFactor;
	const float BrakeLookahead = FMath::Min(Speed * Speed / (2.f * Params.PlannedBraking) + BrakeLookaheadMargin, BrakeLookaheadMax);
	float TargetSpeed = TopTarget;
	for (float Ahead = 0.f; Ahead <= BrakeLookahead; Ahead += BrakeLookaheadStep)
	{
		const float Curvature = FMath::Abs(Track.GetCurvatureAtDistance(Distance + Ahead));
		if (Curvature < StraightCurvature)
		{
			continue;
		}
		const float CornerSpeed = FMath::Min(FMath::Sqrt(Params.CornerAcceleration / Curvature), TopTarget);
		TargetSpeed = FMath::Min(TargetSpeed, FMath::Sqrt(CornerSpeed * CornerSpeed + 2.f * Params.PlannedBraking * Ahead));
	}

	// --- Line: towards the inside of the corner ahead (right-hand turn = positive curvature = right side).
	const float LineCurvature = Track.GetCurvatureAtDistance(Distance + LineLookaheadBase + Speed * LineLookaheadTime);
	float DesiredLateral = FMath::Clamp(LineCurvature / Params.ApexFullCurvature, -1.f, 1.f) * Params.ApexOffset + Params.PersonalOffset;
	State.Mode = Speed > TargetSpeed ? ERaceAIDriverMode::Braking : ERaceAIDriverMode::Driving;

	// --- Other cars: the nearest slower car ahead that sits on this line.
	const float AvoidRange = AvoidAheadBase + Speed * AvoidAheadTime;
	const FRaceAIOtherCar* Blocker = nullptr;
	float BlockerAhead = AvoidRange;
	for (const FRaceAIOtherCar& Other : Input.Others)
	{
		const float Ahead = Track.WrappedDelta(Distance, Other.TrackDistance);
		if (Ahead <= 0.f || Ahead >= BlockerAhead || Other.Speed > Speed - AvoidMinClosingSpeed)
		{
			continue;
		}
		if (FMath::Abs(Other.LateralOffset - DesiredLateral) < AvoidWidth || FMath::Abs(Other.LateralOffset - State.LateralOffset) < AvoidWidth)
		{
			Blocker = &Other;
			BlockerAhead = Ahead;
		}
	}
	if (Blocker)
	{
		// Go around on the side with more room.
		const float RoomRight = Params.UsableHalfWidth - Blocker->LateralOffset;
		const float RoomLeft = Params.UsableHalfWidth + Blocker->LateralOffset;
		DesiredLateral = Blocker->LateralOffset + (RoomRight >= RoomLeft ? AvoidClearance : -AvoidClearance);
		State.Mode = ERaceAIDriverMode::Avoiding;

		// Right behind it and not beside it yet: do not ram it.
		if (BlockerAhead < TailgateDistance && FMath::Abs(Blocker->LateralOffset - State.LateralOffset) < AvoidWidth)
		{
			TargetSpeed = FMath::Min(TargetSpeed, Blocker->Speed);
		}
	}
	DesiredLateral = FMath::Clamp(DesiredLateral, -Params.UsableHalfWidth, Params.UsableHalfWidth);
	State.TargetLateral = FMath::FInterpConstantTo(State.TargetLateral, DesiredLateral, Input.DeltaTime, Params.LineChangeSpeed);
	State.TargetSpeed = TargetSpeed;

	// --- Aim point and the angle to it.
	const FTrackFrame AimFrame = Track.GetFrameAtDistance(Distance + AimLookaheadBase + Speed * AimLookaheadTime);
	State.AimPoint = AimFrame.Location + AimFrame.Right * State.TargetLateral;
	const FVector LocalAim = Input.Rotation.UnrotateVector(State.AimPoint - Input.Location);
	const float AngleToAim = FMath::RadiansToDegrees(FMath::Atan2(LocalAim.Y, LocalAim.X));

	// --- Stuck against a wall or a car: back off, turning the nose towards the aim point
	// (steering is inverted in reverse, like a car, so steer away from the aim).
	if (State.ReverseTimeRemaining > 0.f)
	{
		State.ReverseTimeRemaining -= Input.DeltaTime;
		State.Mode = ERaceAIDriverMode::Reversing;
		State.Steer = AngleToAim >= 0.f ? -1.f : 1.f;
		State.bBoostHeld = false;
		Out.Brake = 1.f;
		Out.Steer = State.Steer;
		return Out;
	}
	State.StuckTime = Speed < StuckSpeed ? State.StuckTime + Input.DeltaTime : 0.f;
	if (State.StuckTime > StuckDelay)
	{
		State.StuckTime = 0.f;
		State.ReverseTimeRemaining = ReverseDuration;
	}

	// --- Steering, moved at a limited rate.
	const float WantedSteer = FMath::Clamp(AngleToAim / FullSteerAngle, -1.f, 1.f);
	State.Steer = FMath::FInterpConstantTo(State.Steer, WantedSteer, Input.DeltaTime, Params.SteeringSpeed);
	Out.Steer = State.Steer;

	// --- Throttle and brake.
	const float TooFast = Speed - TargetSpeed;
	if (TooFast > 0.f)
	{
		Out.Brake = ForwardSpeed > MinBrakingSpeed ? FMath::Clamp(TooFast / BrakeBand, 0.f, 1.f) : 0.f;
	}
	else
	{
		Out.Throttle = FMath::Clamp(-TooFast / ThrottleBand, 0.f, 1.f);
		if (FMath::Abs(AngleToAim) > OffLineAngle)
		{
			Out.Throttle = FMath::Min(Out.Throttle, OffLineThrottle);
		}
	}

	// --- Drift through sharp corners.
	if (Params.bUseDrift && Speed >= Params.DriftMinSpeed)
	{
		const float DriftCurvature = Track.GetCurvatureAtDistance(Distance + Speed * DriftLookaheadTime);
		if (FMath::Abs(DriftCurvature) >= Params.DriftMinCurvature)
		{
			Out.bDriftRight = DriftCurvature > 0.f;
			Out.bDriftLeft = DriftCurvature < 0.f;
		}
	}

	// --- Boost on a straight, pointing the right way, with nothing to brake for.
	bool bBoost = false;
	if (Params.bUseBoost && TooFast <= 0.f && TargetSpeed >= TopTarget && FMath::Abs(AngleToAim) < BoostMaxAimAngle)
	{
		bool bStraight = true;
		for (float Ahead = 0.f; Ahead <= BoostStraightLength && bStraight; Ahead += BoostStraightStep)
		{
			bStraight = FMath::Abs(Track.GetCurvatureAtDistance(Distance + Ahead)) < StraightCurvature;
		}
		bBoost = bStraight && (State.bBoostHeld ? Input.BoostCharge > BoostStopCharge : Input.BoostCharge >= Params.BoostMinCharge);
	}
	State.bBoostHeld = bBoost;
	Out.bBoost = bBoost;

	return Out;
}
