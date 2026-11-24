#include "Race/RaceTrackerSim.h"

#include "Race/TrackSplineTable.h"

namespace
{
	/** Extra laps of passing-time history beyond the race length (grid behind the line, driving on after the finish). */
	constexpr int32 ExtraHistoryLaps = 2;

	/** Passing times start one lap behind the start line, so grid positions have a bucket too. */
	double GetBucketOrigin(float LapLength)
	{
		return -static_cast<double>(LapLength);
	}

	int32 GetNumCheckpoints(const FRaceTrackerParams& Params)
	{
		return FMath::Max(Params.CheckpointDistances.Num(), 1);
	}

	/** Total progress at which gate Gate is passed. */
	double GetGateProgress(const FRaceTrackerParams& Params, float LapLength, int32 Gate)
	{
		const int32 NumCheckpoints = GetNumCheckpoints(Params);
		const int32 Lap = Gate / NumCheckpoints;
		const int32 Checkpoint = Gate % NumCheckpoints;
		const double InLap = Params.CheckpointDistances.IsValidIndex(Checkpoint) ? Params.CheckpointDistances[Checkpoint] : 0.0;
		return static_cast<double>(Lap) * LapLength + InLap;
	}

	/** Race time at which progress reached Target during the last step (linear between the two samples). */
	double InterpolateCrossingTime(const FRacerSimState& Racer, double Target, double RaceTime, double StepSeconds)
	{
		const double Moved = Racer.Progress - Racer.PrevProgress;
		const double Alpha = Moved > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp((Target - Racer.PrevProgress) / Moved, 0.0, 1.0) : 1.0;
		return RaceTime - StepSeconds + Alpha * StepSeconds;
	}

	void RecordBuckets(FRacerSimState& Racer, const FRaceTrackerParams& Params, float LapLength, double RaceTime, double StepSeconds)
	{
		const double Origin = GetBucketOrigin(LapLength);
		while (Racer.BucketTimes.IsValidIndex(Racer.NextBucket)
			&& Racer.Progress >= Origin + Racer.NextBucket * static_cast<double>(Params.GapBucketSize))
		{
			const double Target = Origin + Racer.NextBucket * static_cast<double>(Params.GapBucketSize);
			Racer.BucketTimes[Racer.NextBucket] = InterpolateCrossingTime(Racer, Target, RaceTime, StepSeconds);
			++Racer.NextBucket;
		}
	}

	/** First step with a location: place the car on the track. Gates already behind it count as passed, silently. */
	void InitialiseRacer(FRacerSimState& Racer, const FTrackSplineTable& Track, const FRaceTrackerParams& Params, const FVector& Location)
	{
		const float LapLength = Track.GetLapLength();
		const FTrackProjection Projection = Track.Project(Location);

		Racer.bInitialised = true;
		Racer.TrackDistance = Projection.Distance;
		Racer.LateralOffset = Projection.LateralOffset;
		// The back half of the lap counts as "behind the start line" (the grid).
		Racer.Progress = Projection.Distance > LapLength * 0.5f ? Projection.Distance - LapLength : Projection.Distance;
		Racer.PrevProgress = Racer.Progress;

		while (Racer.Progress >= GetGateProgress(Params, LapLength, Racer.GatesPassed))
		{
			++Racer.GatesPassed;
		}
		Racer.bStarted = Racer.GatesPassed > 0;
		Racer.bLapTimeValid = false;

		const double Origin = GetBucketOrigin(LapLength);
		const int32 NumBuckets = FMath::CeilToInt32((Params.NumLaps + ExtraHistoryLaps) * LapLength / Params.GapBucketSize) + 1;
		Racer.BucketTimes.Init(-1.0, NumBuckets);
		Racer.FirstBucket = FMath::Clamp(FMath::CeilToInt32((Racer.Progress - Origin) / Params.GapBucketSize), 0, NumBuckets);
		Racer.NextBucket = Racer.FirstBucket;
	}

	void PassGates(FRacerSimState& Racer, FRaceTrackerState& State, const FRaceTrackerParams& Params, float LapLength,
		double StepSeconds, TArray<FRaceEvent>& OutEvents)
	{
		const int32 NumCheckpoints = GetNumCheckpoints(Params);
		while (!Racer.bFinished)
		{
			const double GateProgress = GetGateProgress(Params, LapLength, Racer.GatesPassed);
			if (Racer.Progress < GateProgress)
			{
				break;
			}

			const double CrossTime = InterpolateCrossingTime(Racer, GateProgress, State.RaceTime, StepSeconds);
			const int32 Lap = Racer.GatesPassed / NumCheckpoints;
			const bool bLine = (Racer.GatesPassed % NumCheckpoints) == 0;
			++Racer.GatesPassed;

			if (!bLine)
			{
				continue;
			}

			if (Lap == 0)
			{
				// Crossing the line from the grid starts lap 1.
				Racer.bStarted = true;
			}
			else
			{
				Racer.LapsCompleted = Lap;
				FRaceEvent& Event = OutEvents.AddDefaulted_GetRef();
				Event.Type = ERaceEventType::LapCompleted;
				Event.Racer = Racer.Id;
				Event.Time = CrossTime;
				Event.Lap = Lap;
				if (Racer.bLapTimeValid)
				{
					const float LapTime = static_cast<float>(CrossTime - Racer.LapStartTime);
					Racer.LapTimes.Add(LapTime);
					Racer.BestLapTime = Racer.BestLapTime > 0.f ? FMath::Min(Racer.BestLapTime, LapTime) : LapTime;
					Event.LapTime = LapTime;
				}

				if (Lap >= Params.NumLaps)
				{
					Racer.bFinished = true;
					Racer.FinishTime = CrossTime;
					Racer.FinishPlace = ++State.FinishedCount;

					FRaceEvent& Finish = OutEvents.AddDefaulted_GetRef();
					Finish.Type = ERaceEventType::RacerFinished;
					Finish.Racer = Racer.Id;
					Finish.Time = CrossTime;
					Finish.Lap = Lap;
					Finish.Place = Racer.FinishPlace;
				}
			}
			Racer.LapStartTime = CrossTime;
			Racer.bLapTimeValid = true;
		}
	}

	void UpdateRanking(FRaceTrackerState& State)
	{
		State.RankOrder.Reset(State.Racers.Num());
		for (const FRacerSimState& Racer : State.Racers)
		{
			State.RankOrder.Add(Racer.Id);
		}

		const TArray<FRacerSimState>& Racers = State.Racers;
		State.RankOrder.Sort([&Racers](int32 IdA, int32 IdB)
		{
			const FRacerSimState& A = Racers[IdA];
			const FRacerSimState& B = Racers[IdB];
			if (A.bFinished != B.bFinished)
			{
				return A.bFinished;
			}
			if (A.bFinished)
			{
				return A.FinishPlace < B.FinishPlace;
			}
			if (A.bInitialised != B.bInitialised)
			{
				return A.bInitialised;
			}
			if (A.Progress != B.Progress)
			{
				return A.Progress > B.Progress;
			}
			return A.Id < B.Id;
		});

		for (int32 Position = 0; Position < State.RankOrder.Num(); ++Position)
		{
			State.Racers[State.RankOrder[Position]].Rank = Position + 1;
		}
	}

	void UpdateGaps(FRaceTrackerState& State, const FRaceTrackerParams& Params, float LapLength)
	{
		for (int32 Position = 0; Position < State.RankOrder.Num(); ++Position)
		{
			FRacerSimState& Racer = State.Racers[State.RankOrder[Position]];
			Racer.GapToCarAhead = FRaceGap();
			if (Position > 0 && Racer.bInitialised)
			{
				Racer.GapToCarAhead = FRaceTrackerSim::ComputeGap(State, Params, LapLength, State.RankOrder[Position - 1], Racer.Id);
			}
		}

		State.LeaderToSecondGap = State.RankOrder.Num() >= 2 ? State.Racers[State.RankOrder[1]].GapToCarAhead : FRaceGap();
	}

	void DetectOvertakes(FRaceTrackerState& State, const FRaceTrackerParams& Params, double StepSeconds, TArray<FRaceEvent>& OutEvents)
	{
		const int32 Num = State.Racers.Num();
		for (int32 IdA = 0; IdA < Num; ++IdA)
		{
			for (int32 IdB = IdA + 1; IdB < Num; ++IdB)
			{
				const FRacerSimState& A = State.Racers[IdA];
				const FRacerSimState& B = State.Racers[IdB];
				if (!A.bActive || !B.bActive || !A.bInitialised || !B.bInitialised)
				{
					continue;
				}

				FRacePairState& Pair = State.Pairs[IdA * Num + IdB];
				const double Difference = A.Progress - B.Progress;

				// The moment the cars are level: remember how far apart they were sideways.
				const int8 LevelSign = Difference >= 0.0 ? 1 : -1;
				if (Pair.LevelSign != 0 && LevelSign != Pair.LevelSign)
				{
					Pair.LateralAtLevel = FMath::Abs(A.LateralOffset - B.LateralOffset);
				}
				Pair.LevelSign = LevelSign;

				// Margin as hysteresis: inside it the previous order stays.
				if (Difference > Params.OvertakeMargin)
				{
					Pair.RawOrder = 1;
				}
				else if (Difference < -Params.OvertakeMargin)
				{
					Pair.RawOrder = -1;
				}
				else if (Pair.RawOrder == 0)
				{
					Pair.RawOrder = LevelSign;
				}

				if (Pair.ConfirmedOrder == 0)
				{
					// First look at this pair: just remember the order.
					Pair.ConfirmedOrder = Pair.RawOrder;
					continue;
				}

				if (Pair.RawOrder == Pair.ConfirmedOrder)
				{
					Pair.PendingTime = 0.f;
					continue;
				}

				Pair.PendingTime += static_cast<float>(StepSeconds);
				if (Pair.PendingTime < Params.OvertakeConfirmTime)
				{
					continue;
				}

				Pair.ConfirmedOrder = Pair.RawOrder;
				Pair.PendingTime = 0.f;

				const bool bAOvertook = Pair.RawOrder > 0;
				const FRacerSimState& Overtaker = bAOvertook ? A : B;
				const FRacerSimState& Overtaken = bAOvertook ? B : A;
				const int32 Direction = bAOvertook ? 0 : 1;

				const float LateralAtLevel = Pair.LateralAtLevel >= 0.f ? Pair.LateralAtLevel : FMath::Abs(A.LateralOffset - B.LateralOffset);
				Pair.LateralAtLevel = -1.f;

				// No overtakes on the grid before the start line, after the finish, or within the pair cooldown.
				const bool bCounts = Overtaker.bStarted && Overtaken.bStarted
					&& !Overtaker.bFinished && !Overtaken.bFinished && !State.bRaceFinished
					&& State.RaceTime - Pair.LastOvertakeTime[Direction] >= Params.OvertakePairCooldown;
				if (!bCounts)
				{
					continue;
				}
				Pair.LastOvertakeTime[Direction] = State.RaceTime;

				FRaceEvent& Event = OutEvents.AddDefaulted_GetRef();
				Event.Type = ERaceEventType::Overtake;
				Event.Racer = Overtaker.Id;
				Event.Other = Overtaken.Id;
				Event.Time = State.RaceTime;
				Event.Closeness.LateralDistance = LateralAtLevel;
				Event.Closeness.GapAtConfirm = static_cast<float>(FMath::Abs(Difference));
				// Close = passed near the other car sideways, whatever the speed difference (developer decision).
				Event.Closeness.bClose = LateralAtLevel <= Params.CloseOvertakeLateral;
				Event.Closeness.Closeness = Params.CloseOvertakeLateral > 0.f
					? FMath::Clamp(1.f - LateralAtLevel / Params.CloseOvertakeLateral, 0.f, 1.f)
					: 0.f;
			}
		}
	}

	void CheckRaceFinished(FRaceTrackerState& State, const FRaceTrackerParams& Params, TArray<FRaceEvent>& OutEvents)
	{
		if (State.bRaceFinished || State.Racers.IsEmpty())
		{
			return;
		}

		// The race ends when everybody has finished, or a while after the first human crossed the line
		// (the first driver, in a race without humans), so slower cars still get a real time and place.
		bool bAnyHuman = false;
		bool bAllFinished = true;
		for (const FRacerSimState& Racer : State.Racers)
		{
			bAnyHuman |= Racer.bHuman;
			bAllFinished &= Racer.bFinished || !Racer.bInitialised;
		}
		if (State.FirstFinishTime < 0.0)
		{
			for (const FRacerSimState& Racer : State.Racers)
			{
				if (Racer.bFinished && (Racer.bHuman || !bAnyHuman) && (State.FirstFinishTime < 0.0 || Racer.FinishTime < State.FirstFinishTime))
				{
					State.FirstFinishTime = Racer.FinishTime;
				}
			}
		}

		const bool bTimedOut = State.FirstFinishTime >= 0.0 && State.RaceTime - State.FirstFinishTime >= Params.FinishTimeout;
		if (bAllFinished || bTimedOut)
		{
			State.bRaceFinished = true;
			FRaceEvent& Event = OutEvents.AddDefaulted_GetRef();
			Event.Type = ERaceEventType::RaceFinished;
			Event.Time = State.RaceTime;
		}
	}
}

int32 FRaceTrackerSim::AddRacer(FRaceTrackerState& State, bool bHuman)
{
	const int32 OldNum = State.Racers.Num();
	FRacerSimState& Racer = State.Racers.AddDefaulted_GetRef();
	Racer.Id = OldNum;
	Racer.bHuman = bHuman;

	// Re-index the pair table for the new size, keeping what is known about existing pairs.
	const int32 NewNum = OldNum + 1;
	TArray<FRacePairState> NewPairs;
	NewPairs.SetNum(NewNum * NewNum);
	for (int32 IdA = 0; IdA < OldNum; ++IdA)
	{
		for (int32 IdB = IdA + 1; IdB < OldNum; ++IdB)
		{
			NewPairs[IdA * NewNum + IdB] = State.Pairs[IdA * OldNum + IdB];
		}
	}
	State.Pairs = MoveTemp(NewPairs);
	return Racer.Id;
}

void FRaceTrackerSim::Step(FRaceTrackerState& State, const FTrackSplineTable& Track, const FRaceTrackerParams& Params,
	TConstArrayView<TOptional<FVector>> Locations, double StepSeconds, TArray<FRaceEvent>& OutEvents)
{
	State.RaceTime = State.StepCount * StepSeconds;
	const float LapLength = Track.GetLapLength();

	for (FRacerSimState& Racer : State.Racers)
	{
		const bool bHasLocation = Locations.IsValidIndex(Racer.Id) && Locations[Racer.Id].IsSet();
		Racer.bActive = bHasLocation;
		if (!bHasLocation)
		{
			Racer.ProgressSpeed = 0.f;
			continue;
		}

		const FVector& Location = Locations[Racer.Id].GetValue();
		if (!Racer.bInitialised)
		{
			InitialiseRacer(Racer, Track, Params, Location);
			continue;
		}

		const FTrackProjection Projection = Track.ProjectNear(Location, Racer.TrackDistance, Params.ProjectionWindow, Params.ReacquireDistance);
		const float Delta = Track.WrappedDelta(Racer.TrackDistance, Projection.Distance);
		Racer.PrevProgress = Racer.Progress;
		Racer.Progress += Delta;
		Racer.TrackDistance = Projection.Distance;
		Racer.LateralOffset = Projection.LateralOffset;
		Racer.ProgressSpeed = StepSeconds > 0.0 ? static_cast<float>(Delta / StepSeconds) : 0.f;

		RecordBuckets(Racer, Params, LapLength, State.RaceTime, StepSeconds);
		PassGates(Racer, State, Params, LapLength, StepSeconds, OutEvents);
	}

	UpdateRanking(State);
	UpdateGaps(State, Params, LapLength);
	DetectOvertakes(State, Params, StepSeconds, OutEvents);
	CheckRaceFinished(State, Params, OutEvents);

	++State.StepCount;
}

FRaceGap FRaceTrackerSim::ComputeGap(const FRaceTrackerState& State, const FRaceTrackerParams& Params, float LapLength, int32 Ahead, int32 Behind)
{
	FRaceGap Gap;
	if (!State.Racers.IsValidIndex(Ahead) || !State.Racers.IsValidIndex(Behind))
	{
		return Gap;
	}

	const FRacerSimState& A = State.Racers[Ahead];
	const FRacerSimState& B = State.Racers[Behind];
	if (!A.bInitialised || !B.bInitialised)
	{
		return Gap;
	}

	Gap.bValid = true;
	Gap.DistanceCm = static_cast<float>(A.Progress - B.Progress);

	// When was the car ahead where the car behind is now?
	const double BucketPosition = (B.Progress - GetBucketOrigin(LapLength)) / Params.GapBucketSize;
	const int32 Bucket = FMath::FloorToInt32(BucketPosition);
	if (Bucket >= A.FirstBucket && Bucket + 1 < A.NextBucket)
	{
		const double Alpha = BucketPosition - Bucket;
		const double PassedAt = FMath::Lerp(A.BucketTimes[Bucket], A.BucketTimes[Bucket + 1], Alpha);
		Gap.TimeSeconds = static_cast<float>(State.RaceTime - PassedAt);
	}
	else
	{
		// No passing time recorded there (joined later, or not moving yet): estimate from the speed behind.
		Gap.TimeSeconds = Gap.DistanceCm / FMath::Max(B.ProgressSpeed, Params.MinGapSpeed);
	}
	return Gap;
}

int32 FRaceTrackerSim::GetCurrentLap(const FRacerSimState& Racer, int32 NumLaps)
{
	return FMath::Clamp(Racer.LapsCompleted + 1, 1, FMath::Max(NumLaps, 1));
}

int32 FRaceTrackerSim::GetNextCheckpoint(const FRacerSimState& Racer, const FRaceTrackerParams& Params)
{
	return Racer.GatesPassed % GetNumCheckpoints(Params);
}
