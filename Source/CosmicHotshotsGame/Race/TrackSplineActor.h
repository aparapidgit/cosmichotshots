// Track spline: the centre line of a race track, placed once per track map.
//
// Spline point 0 lies on the start/finish line and the points follow the driving
// direction around a closed loop. All race progress (ranking, gaps, laps) is measured
// along this line, never in straight lines. The physics step reads the line through
// an immutable FTrackSplineTable, built here on the game thread on first use.
// On the test track this actor is placed by Scripts/place_track_spline.py.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrackSplineActor.generated.h"

class FTrackSplineTable;
class USplineComponent;

UCLASS()
class COSMICHOTSHOTSGAME_API ATrackSplineActor : public AActor
{
	GENERATED_BODY()

public:
	ATrackSplineActor();

	/** First track spline in the world, or null. Game thread only. */
	static ATrackSplineActor* Find(const UWorld* World);

	/** Table of the first track spline in the world (built on first call), or null. Game thread only. */
	static TSharedPtr<const FTrackSplineTable> FindTable(const UWorld* World);

	/** Immutable sampled copy of the spline; built on the first call. Game thread only. Safe to read from any thread afterwards. */
	TSharedPtr<const FTrackSplineTable> GetTable();

	USplineComponent* GetSpline() const { return Spline; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<USplineComponent> Spline;

	/** Spacing of the samples in the table (cm). Smaller = more precise projection, more memory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "10.0"))
	float TableSampleSpacing = 200.f;

private:
	TSharedPtr<const FTrackSplineTable> Table;
};
