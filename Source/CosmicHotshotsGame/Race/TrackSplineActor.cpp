#include "Race/TrackSplineActor.h"

#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "Race/TrackSplineTable.h"

ATrackSplineActor::ATrackSplineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetClosedLoop(true);
	RootComponent = Spline;
}

ATrackSplineActor* ATrackSplineActor::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATrackSplineActor> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

TSharedPtr<const FTrackSplineTable> ATrackSplineActor::FindTable(const UWorld* World)
{
	ATrackSplineActor* Track = Find(World);
	return Track ? Track->GetTable() : nullptr;
}

TSharedPtr<const FTrackSplineTable> ATrackSplineActor::GetTable()
{
	if (Table.IsValid())
	{
		return Table;
	}

	TSharedRef<FTrackSplineTable> NewTable = MakeShared<FTrackSplineTable>();
	const float Length = Spline->GetSplineLength();
	const int32 Count = FMath::Max(FMath::CeilToInt32(Length / TableSampleSpacing), 3);

	TArray<FVector> Samples;
	Samples.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Even spacing; the closing point (Length) is the first one again, so it is left out.
		Samples.Add(Spline->GetLocationAtDistanceAlongSpline(Length * Index / Count, ESplineCoordinateSpace::World));
	}
	NewTable->Build(MoveTemp(Samples));
	NewTable->SetHalfWidth(TrackHalfWidth);

	Table = NewTable;
	return Table;
}
