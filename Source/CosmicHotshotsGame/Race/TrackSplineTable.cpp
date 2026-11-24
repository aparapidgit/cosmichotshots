#include "Race/TrackSplineTable.h"

#include "Algo/BinarySearch.h"

namespace
{
	FVector HorizontalRight(const FVector& Direction)
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal2D();
		return Right.IsNearlyZero() ? FVector::RightVector : Right;
	}

	/** Heading change is measured between points this far before and after (cm). */
	constexpr float CurvatureSmoothingDistance = 1000.f;
}

void FTrackSplineTable::Build(TArray<FVector> InPoints)
{
	Points = MoveTemp(InPoints);
	Distances.Reset(Points.Num() + 1);
	Directions.Reset(Points.Num());
	LapLength = 0.f;

	if (Points.Num() < MinPoints)
	{
		return;
	}

	float Accumulated = 0.f;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector& From = Points[Index];
		const FVector& To = Points[(Index + 1) % Points.Num()];
		Distances.Add(Accumulated);
		Directions.Add((To - From).GetSafeNormal());
		Accumulated += FVector::Dist(From, To);
	}
	Distances.Add(Accumulated);
	LapLength = Accumulated;

	// Curvature: change of heading over a short stretch around each point, so sample noise does not show.
	const int32 Num = Points.Num();
	Curvatures.SetNumUninitialized(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const int32 Span = FMath::Clamp(FMath::RoundToInt32(CurvatureSmoothingDistance * Num / FMath::Max(LapLength, 1.f)), 1, Num / 2 - 1);
		const FVector& Before = Directions[(Index - Span + Num) % Num];
		const FVector& After = Directions[(Index + Span) % Num];
		const float HeadingChange = FMath::FindDeltaAngleRadians(FMath::Atan2(Before.Y, Before.X), FMath::Atan2(After.Y, After.X));
		const float Stretch = 2.f * Span * LapLength / Num;
		Curvatures[Index] = Stretch > 0.f ? HeadingChange / Stretch : 0.f;
	}
}

float FTrackSplineTable::GetCurvatureAtDistance(float Distance) const
{
	return IsValid() ? Curvatures[SegmentAtDistance(Distance)] : 0.f;
}

float FTrackSplineTable::WrapDistance(float Distance) const
{
	if (LapLength <= 0.f)
	{
		return 0.f;
	}
	const float Wrapped = FMath::Fmod(Distance, LapLength);
	return Wrapped < 0.f ? Wrapped + LapLength : Wrapped;
}

float FTrackSplineTable::WrappedDelta(float From, float To) const
{
	float Delta = WrapDistance(To - From);
	if (Delta > LapLength * 0.5f)
	{
		Delta -= LapLength;
	}
	return Delta;
}

int32 FTrackSplineTable::SegmentAtDistance(float Distance) const
{
	const float Wrapped = WrapDistance(Distance);
	// Last entry whose start distance is <= Wrapped.
	const int32 Upper = Algo::UpperBound(Distances, Wrapped);
	return FMath::Clamp(Upper - 1, 0, Points.Num() - 1);
}

FTrackFrame FTrackSplineTable::GetFrameAtDistance(float Distance) const
{
	FTrackFrame Frame;
	if (!IsValid())
	{
		return Frame;
	}

	const float Wrapped = WrapDistance(Distance);
	const int32 Segment = SegmentAtDistance(Wrapped);
	const float SegmentLength = Distances[Segment + 1] - Distances[Segment];
	const float Alpha = SegmentLength > 0.f ? (Wrapped - Distances[Segment]) / SegmentLength : 0.f;

	Frame.Location = FMath::Lerp(Points[Segment], Points[(Segment + 1) % Points.Num()], Alpha);
	Frame.Direction = Directions[Segment];
	Frame.Right = HorizontalRight(Frame.Direction);
	return Frame;
}

FTrackProjection FTrackSplineTable::ProjectOnSegment(int32 Index, const FVector& Location) const
{
	const FVector& From = Points[Index];
	const FVector& To = Points[(Index + 1) % Points.Num()];
	const FVector Closest = FMath::ClosestPointOnSegment(Location, From, To);
	const float SegmentLength = Distances[Index + 1] - Distances[Index];

	FTrackProjection Result;
	Result.bValid = true;
	Result.Direction = Directions[Index];
	Result.Distance = WrapDistance(Distances[Index] + FMath::Min(FVector::Dist(From, Closest), SegmentLength));
	Result.DistanceToLine = FVector::Dist(Location, Closest);
	Result.LateralOffset = FVector::DotProduct(Location - Closest, HorizontalRight(Result.Direction));
	return Result;
}

FTrackProjection FTrackSplineTable::Project(const FVector& Location) const
{
	FTrackProjection Best;
	if (!IsValid())
	{
		return Best;
	}

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FTrackProjection Candidate = ProjectOnSegment(Index, Location);
		if (!Best.bValid || Candidate.DistanceToLine < Best.DistanceToLine)
		{
			Best = Candidate;
		}
	}
	return Best;
}

FTrackProjection FTrackSplineTable::ProjectNear(const FVector& Location, float HintDistance, float Window, float ReacquireDistance) const
{
	if (!IsValid())
	{
		return FTrackProjection();
	}

	const int32 First = SegmentAtDistance(HintDistance - Window);
	const int32 Last = SegmentAtDistance(HintDistance + Window);
	// Number of segments from First to Last going forward around the loop.
	const int32 Count = FMath::Min(((Last - First + Points.Num()) % Points.Num()) + 1, Points.Num());

	FTrackProjection Best;
	for (int32 Step = 0; Step < Count; ++Step)
	{
		const FTrackProjection Candidate = ProjectOnSegment((First + Step) % Points.Num(), Location);
		if (!Best.bValid || Candidate.DistanceToLine < Best.DistanceToLine)
		{
			Best = Candidate;
		}
	}

	if (!Best.bValid || Best.DistanceToLine > ReacquireDistance)
	{
		const FTrackProjection Full = Project(Location);
		if (Full.bValid && (!Best.bValid || Full.DistanceToLine < Best.DistanceToLine))
		{
			return Full;
		}
	}
	return Best;
}
