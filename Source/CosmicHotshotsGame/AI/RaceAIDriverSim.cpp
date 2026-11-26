#include "AI/RaceAIDriverSim.h"

#include "Math/RandomStream.h"
#include "Race/TrackSplineTable.h"

namespace
{
	/** The car drops onto the track and settles before the driver touches anything (s). */
	constexpr double DriverSettleTime = 1.0;

	// Track projection (same idea as the race tracker): search near the last position, whole lap when lost.
	constexpr float LineProjectionWindow = 3000.f;
	constexpr float LineReacquireDistance = 2500.f;

	// Speed planning: look this far ahead for corners, in steps (cm).
	constexpr float BrakeLookaheadMargin = 2000.f;
	constexpr float BrakeLookaheadMax = 30000.f;
	constexpr float BrakeLookaheadStep = 500.f;
	/** Curvature below this is a straight (1 / cm). */
	constexpr float StraightCurvature = 0.00002f;

	// Aim point (debug display, and which way to turn when backing off): this far ahead on the line (cm, plus speed x time).
	constexpr float AimLookaheadBase = 1200.f;
	constexpr float AimLookaheadTime = 0.45f;
	/** Travelling this far off the wanted direction (degrees): ease off instead of accelerating the wrong way. */
	constexpr float OffLineAngle = 35.f;
	constexpr float OffLineThrottle = 0.4f;

	// Steering. The driver turns as fast as the track turns a moment ahead (speed x time, averaged over
	// a stretch in cm so one kinked sample does not jerk the wheel), and adds a correction: the sideways
	// speed it wants is the distance to its line x gain (1/s), limited to an angle (degrees) off the track
	// direction; the turn rate added is the error in travel direction x gain (1/s).
	constexpr float SteerPreviewTime = 0.25f;
	constexpr float SteerCurvatureSpan = 800.f;
	constexpr float LineErrorGain = 1.2f;
	constexpr float LineMaxApproachAngle = 10.f;
	constexpr float CourseGain = 3.f;
	/** Below this speed (cm/s) the nose, not the direction of travel, says where the car is heading. */
	constexpr float MinCourseSpeed = 500.f;

	// Racing line: the driver positions itself for the average turn of the track from here to this far ahead
	// (cm, plus speed x time), sampled in steps (cm).
	constexpr float LineLookaheadBase = 1500.f;
	constexpr float LineLookaheadTime = 2.f;
	constexpr float LineLookaheadStep = 1000.f;
	// Wall hugging: only where the wall is smooth; a kink is given room.
	// A wall is smooth where the track's turn changes little from one sample (step, cm) to the next, from a
	// little behind the car to speed x time ahead. A jump (a kinked wall, or the start or end of a corner)
	// is given room. Walls of corners sharper than the absolute limit are not hugged at all.
	constexpr float WallHugMaxCurvatureStep = 0.000025f;
	constexpr float WallHugMaxCurvature = 0.00012f;
	constexpr float WallHugLookBehind = 1000.f;
	constexpr float WallHugLookAheadTime = 1.f;
	constexpr float WallHugSampleStep = 1000.f;
	/** The last stretch (cm) towards the wall is closed slowly (cm/s), so the car does not overshoot into it. */
	constexpr float WallHugApproachDistance = 300.f;
	constexpr float WallHugApproachSpeed = 120.f;
	/** Where the wall has a kink the driver stays this much (cm) further away from it than its hugging line. */
	constexpr float WallKinkRoom = 200.f;
	/** A line change slows down on arrival with this deceleration (cm/s^2), never below the minimum speed (cm/s). */
	constexpr float LineChangeDeceleration = 500.f;
	constexpr float LineChangeMinSpeed = 40.f;
	/** In a corner that needs braking the driver cuts to the inside once the track under it turns at least this share of the corner's sharpest turn. */
	constexpr float TightCornerInsideShare = 0.5f;
	/** On the way into such a corner the driver sits this share of the usable half width to the inside of the middle. */
	constexpr float TightCornerEntryShare = 0.2f;
	/** The driver changes sides once the track ahead calls for the other side with at least this share of a full commitment. */
	constexpr float LineSideKeepShare = 0.5f;
	/** Along the wall a steady error in the gap is trimmed out: cm of aim shift per second per cm of error, up to a limit (cm). */
	constexpr float WallHugTrimRate = 0.6f;
	constexpr float WallHugTrimMax = 120.f;

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
	/** An overtaking side is kept for this long (s) after the car to pass is no longer in the way, so the driver does not dither. */
	constexpr float PassCommitTime = 1.f;

	// Cars alongside: a car within this distance along the track (cm) and this far sideways (cm) is beside
	// this one; the driver keeps its centre at least the clearance (cm) away from it.
	constexpr float AlongsideLength = 550.f;
	constexpr float AlongsideWidth = 700.f;
	constexpr float AlongsideClearance = 300.f;
	/** Squeezed between a car and a wall, the driver keeps this gap (cm) to the wall and eases off by this much speed (cm/s) to drop back. */
	constexpr float AlongsideWallGap = 40.f;
	constexpr float AlongsideYieldSpeed = 250.f;

	// Defending: a car behind within this distance (cm, plus its closing speed x time) that closes in by at
	// least this much (cm/s) is an attacker. The defending line is held this long (s) after the threat is gone.
	constexpr float DefendRangeBase = 1500.f;
	constexpr float DefendRangeTime = 1.5f;
	constexpr float DefendMinClosingSpeed = 150.f;
	constexpr float DefendHoldTime = 1.f;
	/** An attacker already this close sideways (cm) is covered; no move is needed. */
	constexpr float DefendCoveredWidth = 150.f;

	// Stuck: below this speed (cm/s) for the delay (s), reverse for the duration (s).
	constexpr float DriverStuckSpeed = 300.f;
	constexpr float DriverStuckDelay = 1.f;
	constexpr float DriverReverseDuration = 1.2f;

	// Item boxes: look this far ahead (cm, plus speed x time).
	constexpr float BoxSeekBase = 2000.f;
	constexpr float BoxSeekTime = 2.5f;
	/** A car ahead within this sideways distance (cm) of a box is expected to collect it. */
	constexpr float BoxClaimWidth = 350.f;

	// Drift.
	/** The corner the drift button reacts to lies speed x this time ahead (s). */
	constexpr float DriftLookaheadTime = 0.25f;
	/** A running drift is kept while the corner is at least this share of the starting sharpness. */
	constexpr float DriftKeepCurvatureShare = 0.75f;
	/** Degrees of slide past the driver's limit that push the steering fully against the drift. */
	constexpr float DriftSlipCorrectionBand = 5.f;
	/** A drift starts only where the corner needs this much more turn than a drift's minimum turn rate. */
	constexpr float DriftTightnessMargin = 1.4f;
	/** This close (degrees) to the car's critical angle the drift button is let go. */
	constexpr float DriftCriticalMargin = 3.f;

	// Mistakes: one roll per stretch of track of this length (cm), keyed by driver seed, lap and stretch.
	constexpr float MistakeStretchLength = 20000.f;
	constexpr int32 MistakeStretchesPerLapKey = 1000;
	constexpr int32 MistakeKinds = 3;
	/** No mistakes in the first seconds of the race (s of the car's own simulation time). */
	constexpr double MistakeFreeStartTime = 6.0;

	// Boost.
	/** The track this far ahead (speed x time, in steps of cm) must be gentle enough for the driver to boost. */
	constexpr float BoostLookaheadTime = 2.f;
	constexpr float BoostLookaheadStep = 1000.f;
	/** Boost only with this much speed (cm/s) in hand before the next hazard would need braking. */
	constexpr float BoostHazardMargin = 300.f;
	/** Travelling within this angle (degrees) of the wanted direction before boosting. */
	constexpr float BoostMaxAimAngle = 8.f;
	/** A running boost is kept until the meter is this low. */
	constexpr float BoostStopCharge = 0.02f;
}

float FRaceAIDriverParams::GetYawRateAtSpeed(float Speed) const
{
	if (SteeringBySpeed.Num() < 2 || TopSpeed <= UE_KINDA_SMALL_NUMBER)
	{
		return FullYawRate;
	}
	// The same lookup the car's own physics does (FHoverSimParams::GetSteeringMultiplier).
	const float Position = FMath::Clamp(FMath::Abs(Speed) / TopSpeed / SteeringSampleRange, 0.f, 1.f) * (SteeringBySpeed.Num() - 1);
	const int32 Index = FMath::Min(FMath::FloorToInt32(Position), SteeringBySpeed.Num() - 2);
	return FullYawRate * FMath::Lerp(SteeringBySpeed[Index], SteeringBySpeed[Index + 1], Position - Index);
}

namespace
{
	/**
	 * Fastest speed (cm/s) at which the car can still turn a corner of this curvature with the given share of
	 * its steering lock. The lock's turn rate depends on the speed itself, so it is found in a few rounds.
	 */
	float SteeringLimitedSpeed(const FRaceAIDriverParams& Params, float Curvature, float TopSpeed)
	{
		constexpr int32 Rounds = 4;
		float Speed = TopSpeed;
		for (int32 Round = 0; Round < Rounds; ++Round)
		{
			Speed = FMath::Min(Params.CornerSteeringShare * Params.GetYawRateAtSpeed(Speed) / Curvature, TopSpeed);
		}
		return Speed;
	}
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
		: Track.ProjectNear(Input.Location, State.TrackDistance, LineProjectionWindow, LineReacquireDistance);
	State.TrackDistance = Projection.Distance;
	State.LateralOffset = Projection.LateralOffset;

	if (Input.ElapsedSimTime < DriverSettleTime)
	{
		State.Mode = ERaceAIDriverMode::Waiting;
		State.TargetLateral = Projection.LateralOffset;
		return Out;
	}

	const float Distance = State.TrackDistance;
	const float Speed = static_cast<float>(Input.Velocity.Size());
	const float ForwardSpeed = static_cast<float>(FVector::DotProduct(Input.Velocity, Input.Rotation.GetForwardVector()));

	// --- Mistakes: on entering a new stretch of track, roll once whether this driver slips up on it.
	// The roll depends only on the driver's seed, the lap and the stretch, never on time or frame rate.
	const int32 Stretch = Input.Lap * MistakeStretchesPerLapKey + FMath::FloorToInt32(Distance / MistakeStretchLength);
	if (Stretch != State.MistakeStretch)
	{
		State.MistakeStretch = Stretch;
		State.Mistake = ERaceAIMistake::None;
		FRandomStream Roll(static_cast<int32>(HashCombineFast(::GetTypeHash(Params.MistakeSeed), ::GetTypeHash(Stretch))));
		if (Input.ElapsedSimTime > MistakeFreeStartTime && Roll.FRand() < Params.MistakeChance)
		{
			State.Mistake = static_cast<ERaceAIMistake>(1 + Roll.RandRange(0, MistakeKinds - 1));
			State.MistakeLiftRemaining = State.Mistake == ERaceAIMistake::Lift ? Params.MistakeLiftTime : 0.f;
			++State.MistakeCount;
		}
	}
	State.MistakeLiftRemaining = FMath::Max(State.MistakeLiftRemaining - Input.DeltaTime, 0.f);

	// --- Speed. The cruise speed is what the driver aims for on a straight; a running boost raises it to
	// the boost's top speed (otherwise the driver would brake against its own boost). The hazard speed is
	// the fastest speed from which every corner ahead can still be reached by braking: only that, and a
	// car in the way, is worth braking for.
	const float CruiseSpeed = Params.TopSpeed * Params.TopSpeedFactor;
	const bool bBoostRunning = State.bBoostHeld && Input.bBoosting;
	const float TopTarget = bBoostRunning ? FMath::Max(Params.BoostTopSpeed, CruiseSpeed) : CruiseSpeed;
	const float BrakeLookahead = FMath::Min(Speed * Speed / (2.f * Params.PlannedBraking) + BrakeLookaheadMargin, BrakeLookaheadMax);
	float HazardSpeed = TNumericLimits<float>::Max();
	for (float Ahead = 0.f; Ahead <= BrakeLookahead; Ahead += BrakeLookaheadStep)
	{
		const float Curvature = FMath::Abs(Track.GetCurvatureAtDistance(Distance + Ahead));
		if (Curvature < StraightCurvature)
		{
			continue;
		}
		// Two limits on the corner's speed: the sideways acceleration the driver dares, and how tightly the
		// car can steer at speed (what makes a hairpin a braking corner).
		const float CornerSpeed = FMath::Min(FMath::Sqrt(Params.CornerAcceleration / Curvature), SteeringLimitedSpeed(Params, Curvature, TopTarget));
		HazardSpeed = FMath::Min(HazardSpeed, FMath::Sqrt(CornerSpeed * CornerSpeed + 2.f * Params.PlannedBraking * Ahead));
	}

	// --- Line: towards the inside of the corner ahead (right-hand turn = positive curvature = right side).
	const float LineLookahead = LineLookaheadBase + Speed * LineLookaheadTime;
	float CurvatureSum = 0.f;
	float SharpestSigned = 0.f;
	int32 CurvatureSamples = 0;
	for (float Ahead = 0.f; Ahead <= LineLookahead; Ahead += LineLookaheadStep, ++CurvatureSamples)
	{
		const float Curvature = Track.GetCurvatureAtDistance(Distance + Ahead);
		CurvatureSum += Curvature;
		if (FMath::Abs(Curvature) > FMath::Abs(SharpestSigned))
		{
			SharpestSigned = Curvature;
		}
	}
	// A driver that hugs walls stays committed to the side of the last corner (also down the straight that
	// follows) until the track ahead clearly calls for the other side, or for more of this one.
	const float InsideNow = FMath::Clamp(CurvatureSum / (CurvatureSamples * Params.ApexFullCurvature), -1.f, 1.f);
	const bool bOtherSide = InsideNow * State.LineSide < 0.f && FMath::Abs(InsideNow) >= LineSideKeepShare;
	if (Params.WallHugClearance <= 0.f || bOtherSide || InsideNow * State.LineSide >= FMath::Square(State.LineSide))
	{
		State.LineSide = InsideNow;
	}
	const float InsideAmount = State.LineSide;
	float DesiredLateral = InsideAmount * Params.ApexOffset + Params.PersonalOffset;

	// Fully committed to the inside: a driver that dares goes right up to the wall, with more room where the wall has a kink.
	const float HugSide = InsideAmount > 0.f ? 1.f : -1.f;
	const float HugLateral = HugSide * FMath::Max(Params.TrackHalfWidth - Params.BodyHalfWidth - Params.WallHugClearance, 0.f);
	const bool bAtWall = Params.WallHugClearance > 0.f && FMath::Abs(InsideAmount) >= 1.f;
	bool bSmoothWall = true;
	if (bAtWall)
	{
		float PreviousCurvature = Track.GetCurvatureAtDistance(Distance - WallHugLookBehind);
		for (float Ahead = -WallHugLookBehind + WallHugSampleStep; Ahead <= Speed * WallHugLookAheadTime && bSmoothWall; Ahead += WallHugSampleStep)
		{
			const float Curvature = Track.GetCurvatureAtDistance(Distance + Ahead);
			bSmoothWall = FMath::Abs(Curvature - PreviousCurvature) <= WallHugMaxCurvatureStep && FMath::Abs(Curvature) <= WallHugMaxCurvature;
			PreviousCurvature = Curvature;
		}
	}
	bool bHug = bAtWall && bSmoothWall && State.Mistake != ERaceAIMistake::Wide;
	if (bAtWall)
	{
		DesiredLateral = bHug ? HugLateral : HugLateral - HugSide * WallKinkRoom;
	}
	// A corner too tight to steer round at cruise speed pushes the car wide whatever the driver does, so it
	// needs room on the OUTSIDE: the driver comes in a little inside of the middle and aims for the inside
	// once it is in the corner. (Coming in from the outside ends on the outside wall.)
	const float HereCurvature = Track.GetCurvatureAtDistance(Distance);
	const bool bTightAhead = FMath::Abs(SharpestSigned) > StraightCurvature
		&& SteeringLimitedSpeed(Params, FMath::Abs(SharpestSigned), CruiseSpeed) < CruiseSpeed;
	if (bTightAhead)
	{
		const float CornerSide = SharpestSigned > 0.f ? 1.f : -1.f;
		const bool bInCorner = HereCurvature * CornerSide >= FMath::Abs(SharpestSigned) * TightCornerInsideShare;
		const float InsideOffset = FMath::Min(Params.ApexOffset, Params.UsableHalfWidth);
		DesiredLateral = CornerSide * (bInCorner ? InsideOffset : Params.UsableHalfWidth * TightCornerEntryShare);
		State.LineSide = CornerSide;
		bHug = false;
	}
	if (State.Mistake == ERaceAIMistake::Wide)
	{
		// Mistake: misses the inside and runs wide.
		DesiredLateral -= HugSide * Params.MistakeWideOffset;
	}

	// --- Item boxes: with a row of boxes coming up, line up for the box nearest to this line.
	const float BoxRange = BoxSeekBase + Speed * BoxSeekTime;
	float BestBoxSideways = Params.ItemBoxMaxSidestep;
	float BoxLateral = DesiredLateral;
	for (const FRaceAIBoxSpot& Box : Input.Boxes)
	{
		const float Ahead = Track.WrappedDelta(Distance, Box.TrackDistance);
		const float Sideways = FMath::Abs(Box.LateralOffset - DesiredLateral);
		if (!Box.bActive || Ahead <= 0.f || Ahead >= BoxRange || Sideways >= BestBoxSideways)
		{
			continue;
		}

		// A car between here and the box that is lined up with it will take it first: pick another box of the row.
		bool bClaimed = false;
		for (const FRaceAIOtherCar& Other : Input.Others)
		{
			const float OtherAhead = Track.WrappedDelta(Distance, Other.TrackDistance);
			bClaimed |= OtherAhead > 0.f && OtherAhead < Ahead && FMath::Abs(Other.LateralOffset - Box.LateralOffset) < BoxClaimWidth;
		}
		if (!bClaimed)
		{
			BestBoxSideways = Sideways;
			BoxLateral = Box.LateralOffset;
			bHug = false;
		}
	}
	DesiredLateral = BoxLateral;

	State.Mode = Speed > HazardSpeed ? ERaceAIDriverMode::Braking : ERaceAIDriverMode::Driving;

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
		// Pick a side once and stay with it: the inside of the corner ahead if this driver is aggressive
		// enough and there is room, otherwise the side with more room.
		const float RoomRight = Params.UsableHalfWidth - Blocker->LateralOffset;
		const float RoomLeft = Params.UsableHalfWidth + Blocker->LateralOffset;
		if (State.PassSide == 0.f)
		{
			const float InsideRoom = HugSide > 0.f ? RoomRight : RoomLeft;
			const bool bTakeInside = FMath::Abs(InsideAmount) >= Params.InsidePassShare && Params.InsidePassShare < 1.f && InsideRoom >= AvoidClearance;
			State.PassSide = bTakeInside ? HugSide : (RoomRight >= RoomLeft ? 1.f : -1.f);
		}
		if ((State.PassSide > 0.f ? RoomRight : RoomLeft) < AvoidClearance)
		{
			// No room on the chosen side after all (the car ahead moved over).
			State.PassSide = -State.PassSide;
		}
		State.PassHoldRemaining = PassCommitTime;
		DesiredLateral = Blocker->LateralOffset + State.PassSide * AvoidClearance;
		State.Mode = ERaceAIDriverMode::Avoiding;
		bHug = false;

		// Right behind it and not beside it yet: do not ram it.
		if (BlockerAhead < TailgateDistance && FMath::Abs(Blocker->LateralOffset - State.LateralOffset) < AvoidWidth)
		{
			HazardSpeed = FMath::Min(HazardSpeed, Blocker->Speed);
		}
	}
	if (!Blocker)
	{
		State.PassHoldRemaining = FMath::Max(State.PassHoldRemaining - Input.DeltaTime, 0.f);
		if (State.PassHoldRemaining <= 0.f)
		{
			State.PassSide = 0.f;
		}
	}

	// --- Defending: one move towards the side of a faster car closing in from behind, then hold that line.
	// The move is made once per attack; the driver never weaves.
	bool bThreat = false;
	if (Params.DefendShift > 0.f && !Blocker)
	{
		const FRaceAIOtherCar* Attacker = nullptr;
		float AttackerBehind = TNumericLimits<float>::Max();
		for (const FRaceAIOtherCar& Other : Input.Others)
		{
			// Only the inside is worth covering: an attacker on the outside has the longer way round anyway,
			// and a driver already on the inside line stays there.
			const float Behind = -Track.WrappedDelta(Distance, Other.TrackDistance);
			const float Closing = Other.Speed - Speed;
			const bool bOnInside = (Other.LateralOffset - State.LateralOffset) * HugSide > DefendCoveredWidth;
			if (bOnInside && Behind > AlongsideLength && Behind < AttackerBehind && Closing >= DefendMinClosingSpeed && Behind < DefendRangeBase + Closing * DefendRangeTime)
			{
				Attacker = &Other;
				AttackerBehind = Behind;
			}
		}
		if (Attacker)
		{
			bThreat = true;
			if (!State.bDefending)
			{
				State.bDefending = true;
				State.DefendLateral = State.LateralOffset + FMath::Clamp(Attacker->LateralOffset - State.LateralOffset, -Params.DefendShift, Params.DefendShift);
			}
		}
	}
	State.DefendHoldRemaining = bThreat ? DefendHoldTime : FMath::Max(State.DefendHoldRemaining - Input.DeltaTime, 0.f);
	if (State.DefendHoldRemaining <= 0.f || Blocker)
	{
		State.bDefending = false;
	}
	if (State.bDefending)
	{
		DesiredLateral = State.DefendLateral;
		State.Mode = ERaceAIDriverMode::Defending;
		bHug = false;
	}

	// --- Cars alongside: stay on this side of them (they do the same if they are computer drivers).
	float LeftLimit = -TNumericLimits<float>::Max();
	float RightLimit = TNumericLimits<float>::Max();
	for (const FRaceAIOtherCar& Other : Input.Others)
	{
		const float Sideways = Other.LateralOffset - State.LateralOffset;
		if (FMath::Abs(Track.WrappedDelta(Distance, Other.TrackDistance)) < AlongsideLength && FMath::Abs(Sideways) < AlongsideWidth)
		{
			if (Sideways >= 0.f)
			{
				RightLimit = FMath::Min(RightLimit, Other.LateralOffset - AlongsideClearance);
			}
			else
			{
				LeftLimit = FMath::Max(LeftLimit, Other.LateralOffset + AlongsideClearance);
			}
		}
	}
	const bool bCarAlongside = LeftLimit > -TNumericLimits<float>::Max() || RightLimit < TNumericLimits<float>::Max();
	// The walls are a limit too: with a car on one side and the wall on the other and no room for both,
	// the driver holds the wall side and drops back instead of being squeezed into either.
	const float WallLimit = FMath::Max(Params.TrackHalfWidth - Params.BodyHalfWidth - AlongsideWallGap, 0.f);
	if (bCarAlongside)
	{
		if (LeftLimit > WallLimit || RightLimit < -WallLimit || LeftLimit > RightLimit)
		{
			HazardSpeed = FMath::Min(HazardSpeed, FMath::Max(Speed - AlongsideYieldSpeed, 0.f));
		}
		LeftLimit = FMath::Min(LeftLimit, WallLimit);
		RightLimit = FMath::Max(RightLimit, -WallLimit);
		// Between two cars: the middle of the gap.
		const float Limited = LeftLimit <= RightLimit ? FMath::Clamp(DesiredLateral, LeftLimit, RightLimit) : (LeftLimit + RightLimit) * 0.5f;
		bHug = bHug && Limited == DesiredLateral;
		DesiredLateral = Limited;
	}
	const float TargetSpeed = FMath::Min(TopTarget, HazardSpeed);
	if (!bHug && !(bAtWall && State.Mode != ERaceAIDriverMode::Avoiding))
	{
		DesiredLateral = FMath::Clamp(DesiredLateral, -Params.UsableHalfWidth, Params.UsableHalfWidth);
	}
	State.bHuggingWall = bHug;
	const bool bFinalApproach = bHug && FMath::Abs(DesiredLateral - State.TargetLateral) < WallHugApproachDistance;
	const float PreviousTargetLateral = State.TargetLateral;
	// The line moves sideways at the driver's line-change speed and slows down as it arrives (a line that
	// stopped dead would leave the car, still moving sideways, to overshoot it by metres).
	const float LineRemaining = FMath::Abs(DesiredLateral - State.TargetLateral);
	const float ArrivalSpeed = FMath::Max(FMath::Sqrt(2.f * LineChangeDeceleration * LineRemaining), LineChangeMinSpeed);
	const float LineSpeed = FMath::Min(bFinalApproach ? FMath::Min(WallHugApproachSpeed, Params.LineChangeSpeed) : Params.LineChangeSpeed, ArrivalSpeed);
	State.TargetLateral = FMath::FInterpConstantTo(State.TargetLateral, DesiredLateral, Input.DeltaTime, LineSpeed);
	if (bCarAlongside && LeftLimit <= RightLimit)
	{
		// Never plan a line through the car alongside, not even on the way to the wanted one.
		State.TargetLateral = FMath::Clamp(State.TargetLateral, LeftLimit, RightLimit);
	}
	const float TargetLateralSpeed = Input.DeltaTime > UE_SMALL_NUMBER ? (State.TargetLateral - PreviousTargetLateral) / Input.DeltaTime : 0.f;

	// How far the car is from its line. Right along the wall the measured gap to it is what counts
	// (too wide = move towards the wall).
	float LineError = State.TargetLateral - State.LateralOffset;
	const float HugGap = HugSide > 0.f ? Input.WallClearanceRight : Input.WallClearanceLeft;
	if (bHug && HugGap >= 0.f && State.TargetLateral == DesiredLateral)
	{
		const float GapError = HugSide * (HugGap - Params.WallHugClearance);
		State.WallHugTrim = FMath::Clamp(State.WallHugTrim + GapError * WallHugTrimRate * Input.DeltaTime, -WallHugTrimMax, WallHugTrimMax);
		LineError = GapError + State.WallHugTrim;
	}
	else
	{
		State.WallHugTrim = 0.f;
	}
	State.LineError = LineError;
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
		State.DriftHeld = 0.f;
		State.PassSide = 0.f;
		State.bDefending = false;
		Out.Brake = 1.f;
		Out.Steer = State.Steer;
		return Out;
	}
	State.StuckTime = Speed < DriverStuckSpeed ? State.StuckTime + Input.DeltaTime : 0.f;
	if (State.StuckTime > DriverStuckDelay)
	{
		State.StuckTime = 0.f;
		State.ReverseTimeRemaining = DriverReverseDuration;
	}

	// --- Where the car is going relative to the track, and where the driver wants it to go. A sliding
	// car's nose points into the corner while it travels wide of it, so the driver judges by the direction
	// of TRAVEL (course), not by where the nose points.
	const FTrackFrame HereFrame = Track.GetFrameAtDistance(Distance);
	const FVector Heading = Speed > MinCourseSpeed ? Input.Velocity.GetSafeNormal2D() : Input.Rotation.GetForwardVector().GetSafeNormal2D();
	const float CourseAngle = FMath::Atan2(static_cast<float>(FVector::DotProduct(Heading, HereFrame.Right)), static_cast<float>(FVector::DotProduct(Heading, HereFrame.Direction)));
	const float MaxApproachSpeed = FMath::Max(Speed, MinCourseSpeed) * FMath::Sin(FMath::DegreesToRadians(LineMaxApproachAngle));
	const float WantedLateralSpeed = FMath::Clamp(LineError * LineErrorGain, -MaxApproachSpeed, MaxApproachSpeed) + TargetLateralSpeed;
	const float WantedCourseAngle = FMath::Asin(FMath::Clamp(WantedLateralSpeed / FMath::Max(Speed, MinCourseSpeed), -0.5f, 0.5f));
	const float CourseError = WantedCourseAngle - CourseAngle;
	const float CourseErrorDegrees = FMath::RadiansToDegrees(CourseError);

	// The turn the track makes just ahead, on this car's own line (the inside line turns tighter).
	const float PreviewDistance = Distance + Speed * SteerPreviewTime;
	const float TrackCurvature = (Track.GetCurvatureAtDistance(PreviewDistance - SteerCurvatureSpan) + Track.GetCurvatureAtDistance(PreviewDistance)
		+ Track.GetCurvatureAtDistance(PreviewDistance + SteerCurvatureSpan)) / 3.f;
	const float LineCurvature = TrackCurvature / FMath::Max(1.f - TrackCurvature * State.LateralOffset, 0.5f);
	const float WantedYawRate = Speed * LineCurvature + CourseGain * CourseError;

	// --- Drift: hold the button of the corner's side through corners sharp enough for this driver.
	const FVector LocalVelocity = Input.Rotation.UnrotateVector(Input.Velocity);
	const float SlipAngle = Speed > MinBrakingSpeed ? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)) : 0.f;
	if (Params.bUseDrift && Speed >= Params.DriftMinSpeed)
	{
		const float DriftCurvature = Track.GetCurvatureAtDistance(Distance + Speed * DriftLookaheadTime);
		const float DriftSide = DriftCurvature > 0.f ? 1.f : -1.f;
		const bool bTooMuchSlide = FMath::Abs(SlipAngle) > Params.DriftCriticalSlipAngle - DriftCriticalMargin;
		// The corner must last: a drift through a quick chicane only throws the car at the next wall.
		const float LaterCurvature = Track.GetCurvatureAtDistance(Distance + Speed * Params.DriftMinCornerTime);
		const bool bCornerLasts = LaterCurvature * DriftSide >= Params.DriftMinCurvature * DriftKeepCurvatureShare;
		// A drifting car turns by itself, at least at the drift's minimum rate: in a corner gentler than that
		// it would spiral into the inside wall. Drift only where the corner needs more turn than that.
		const float DriftMinYawRate = Params.DriftMinTurn * Params.DriftYawMultiplier * Params.GetYawRateAtSpeed(ForwardSpeed);
		const bool bTightEnough = Speed * FMath::Abs(DriftCurvature) >= DriftMinYawRate * DriftTightnessMargin;
		// A drift settles at a slide angle of about (turn rate / drift grip). Where that is more than the driver
		// dares, the corner is too fast for a drift: the car would slide into the outside wall.
		const float SettledSlip = FMath::RadiansToDegrees(Speed * FMath::Abs(DriftCurvature) / FMath::Max(Params.DriftGrip, UE_KINDA_SMALL_NUMBER));
		const bool bSlowEnough = SettledSlip <= Params.DriftMaxSlipAngle;
		if (State.DriftHeld == 0.f)
		{
			if (FMath::Abs(DriftCurvature) >= Params.DriftMinCurvature && bCornerLasts && bTightEnough && bSlowEnough && !bTooMuchSlide)
			{
				State.DriftHeld = DriftSide;
			}
		}
		else if (DriftSide != State.DriftHeld || FMath::Abs(DriftCurvature) < Params.DriftMinCurvature * DriftKeepCurvatureShare || bTooMuchSlide
			|| Speed * FMath::Abs(DriftCurvature) < DriftMinYawRate)
		{
			State.DriftHeld = 0.f;
		}
	}
	else
	{
		State.DriftHeld = 0.f;
	}
	Out.bDriftRight = State.DriftHeld > 0.f;
	Out.bDriftLeft = State.DriftHeld < 0.f;

	// --- Steering: the lock that gives the wanted turn rate with this car at this speed, moved at a limited rate.
	const float YawAuthority = FMath::Max(Params.GetYawRateAtSpeed(ForwardSpeed), UE_KINDA_SMALL_NUMBER);
	float WantedSteer = FMath::Clamp(WantedYawRate / YawAuthority, -1.f, 1.f);
	const bool bDrifting = State.DriftHeld != 0.f && Input.DriftDirection == State.DriftHeld;
	if (bDrifting)
	{
		// A drifting car turns the drift's way by itself; the steering only sets how tightly, between the
		// drift's minimum turn (steering fully against it) and its full turn (fully into it).
		const float DriftTurn = FMath::Max(WantedYawRate * State.DriftHeld, 0.f) / (YawAuthority * FMath::Max(Params.DriftYawMultiplier, UE_KINDA_SMALL_NUMBER));
		const float IntoDrift = (DriftTurn - Params.DriftMinTurn) / FMath::Max(1.f - Params.DriftMinTurn, UE_KINDA_SMALL_NUMBER);
		// Sliding more than the driver dares: steer against the drift to straighten up.
		const float SlideExcess = FMath::Max(FMath::Abs(SlipAngle) - Params.DriftMaxSlipAngle, 0.f) / DriftSlipCorrectionBand;
		WantedSteer = FMath::Clamp(State.DriftHeld * (2.f * FMath::Clamp(IntoDrift, 0.f, 1.f) - 1.f - SlideExcess), -1.f, 1.f);
	}
	State.Steer = FMath::FInterpConstantTo(State.Steer, WantedSteer, Input.DeltaTime, Params.SteeringSpeed);
	Out.Steer = State.Steer;

	// --- Throttle and brake. Brake only for a hazard; merely faster than the cruise speed (a boost just
	// ended) means lifting and letting drag do the rest. A target at or above the car's own top speed is
	// driven flat out: easing off near the top would only cost pace.
	const float TooFast = Speed - TargetSpeed;
	if (Speed > HazardSpeed)
	{
		Out.Brake = ForwardSpeed > MinBrakingSpeed ? FMath::Clamp((Speed - HazardSpeed) / BrakeBand, 0.f, 1.f) : 0.f;
	}
	else if (State.MistakeLiftRemaining > 0.f)
	{
		// Mistake: lifts off the throttle for a moment.
	}
	else if (TargetSpeed >= Params.TopSpeed || TooFast <= 0.f)
	{
		Out.Throttle = TargetSpeed >= Params.TopSpeed ? 1.f : FMath::Clamp(-TooFast / ThrottleBand, 0.f, 1.f);
		if (FMath::Abs(CourseErrorDegrees) > OffLineAngle)
		{
			Out.Throttle = FMath::Min(Out.Throttle, OffLineThrottle);
		}
	}

	// --- Boost: going the right way, nothing to brake for soon, and the track ahead gentle enough for this driver.
	bool bBoost = false;
	const bool bMistakeBlocksBoost = State.Mistake == ERaceAIMistake::NoBoost || State.MistakeLiftRemaining > 0.f;
	if (Params.bUseBoost && !bMistakeBlocksBoost && Speed + BoostHazardMargin <= HazardSpeed && FMath::Abs(CourseErrorDegrees) < BoostMaxAimAngle)
	{
		bool bGentle = true;
		const float BoostLookahead = Speed * BoostLookaheadTime;
		for (float Ahead = 0.f; Ahead <= BoostLookahead && bGentle; Ahead += BoostLookaheadStep)
		{
			bGentle = FMath::Abs(Track.GetCurvatureAtDistance(Distance + Ahead)) <= Params.BoostMaxCurvature;
		}
		// A held button whose boost is not running (meter ran dry) is released: only a new press starts a boost.
		// Overtaking is what the saved boost is for: start it with less in the meter.
		const float StartCharge = State.PassSide != 0.f ? FMath::Min(Params.BoostMinCharge, Params.OvertakeBoostCharge) : Params.BoostMinCharge;
		bBoost = bGentle && (State.bBoostHeld ? Input.bBoosting && Input.BoostCharge > BoostStopCharge : Input.BoostCharge >= StartCharge);
	}
	State.bBoostHeld = bBoost;
	Out.bBoost = bBoost;

	return Out;
}
