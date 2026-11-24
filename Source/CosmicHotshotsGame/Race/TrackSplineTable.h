// Track spline table: an immutable, evenly sampled copy of the track centre line.
//
// The race tracker and test helpers run inside the fixed physics step, where calling
// a live USplineComponent is not safe. This table is built once on the game thread
// (ATrackSplineActor) and then only read, from any thread. Distances are measured
// along the line from the start/finish line (distance 0) in cm and wrap at LapLength.

#pragma once

#include "CoreMinimal.h"

/** Where a world location sits relative to the track line. */
struct FTrackProjection
{
	/** Distance along the line from the start/finish line, 0..LapLength. */
	float Distance = 0.f;
	/** Sideways offset from the line in cm, positive = to the right of the driving direction. */
	float LateralOffset = 0.f;
	/** 3D distance from the location to the closest point of the line. */
	float DistanceToLine = 0.f;
	/** Driving direction of the track at that point (unit vector). */
	FVector Direction = FVector::ForwardVector;
	bool bValid = false;
};

/** Track line point at a distance. */
struct FTrackFrame
{
	FVector Location = FVector::ZeroVector;
	/** Driving direction (unit vector). */
	FVector Direction = FVector::ForwardVector;
	/** Horizontal unit vector to the right of the driving direction. */
	FVector Right = FVector::RightVector;
};

class COSMICHOTSHOTSGAME_API FTrackSplineTable
{
public:
	/**
	 * Builds the table from points of a closed loop, in driving order, the first point on the
	 * start/finish line. The last point must NOT repeat the first one.
	 */
	void Build(TArray<FVector> InPoints);

	bool IsValid() const { return Points.Num() >= MinPoints && LapLength > 0.f; }
	float GetLapLength() const { return LapLength; }

	/** Distance wrapped into 0..LapLength. */
	float WrapDistance(float Distance) const;

	/** Difference To - From wrapped into -LapLength/2..+LapLength/2 (shortest way around the loop). */
	float WrappedDelta(float From, float To) const;

	FTrackFrame GetFrameAtDistance(float Distance) const;

	/**
	 * How sharply the track turns at a distance, in radians per cm, smoothed over a short stretch.
	 * Positive = right-hand turn, negative = left-hand turn, 0 = straight. Corner radius = 1 / |curvature|.
	 */
	float GetCurvatureAtDistance(float Distance) const;

	/** Half of the drivable track width in cm (centre line to wall). */
	float GetHalfWidth() const { return HalfWidth; }
	void SetHalfWidth(float InHalfWidth) { HalfWidth = InHalfWidth; }

	/** Searches the whole loop. */
	FTrackProjection Project(const FVector& Location) const;

	/**
	 * Searches only HintDistance +- Window. Close parts of the track (S-kink next to the chicane)
	 * can never be mistaken for each other this way. When the result lies farther than
	 * ReacquireDistance from the line (teleport, lost track), the whole loop is searched instead.
	 */
	FTrackProjection ProjectNear(const FVector& Location, float HintDistance, float Window, float ReacquireDistance) const;

private:
	static constexpr int32 MinPoints = 3;

	/** Closest point on segment Index (Points[Index] -> Points[Index + 1], wrapping). */
	FTrackProjection ProjectOnSegment(int32 Index, const FVector& Location) const;
	int32 SegmentAtDistance(float Distance) const;

	TArray<FVector> Points;
	/** Distance of each point from the start; one extra entry = LapLength (closing segment). */
	TArray<float> Distances;
	/** Unit direction of each segment. */
	TArray<FVector> Directions;
	/** Signed curvature at each point (radians per cm, positive = right turn). */
	TArray<float> Curvatures;
	float LapLength = 0.f;
	float HalfWidth = 1500.f;
};
