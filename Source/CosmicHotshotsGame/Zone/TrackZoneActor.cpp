#include "Zone/TrackZoneActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Race/RaceDebug.h"
#include "Race/RaceGameState.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/VehicleTuningData.h"
#include "Zone/TrackZoneDefinition.h"
#include "Zone/TrackZoneTable.h"

namespace
{
	// Engine basic shapes are 100 cm across.
	constexpr float ZoneShapeSize = 100.f;
	/** Vector parameter of the project's placeholder material (UVehicleTuningData::PlaceholderMaterial), the one the cars use. */
	const FName ZoneColourParameter(TEXT("Colour"));

	/** A dynamic instance of the placeholder material on a mesh, set to a colour. Null when the project has no placeholder material. */
	UMaterialInstanceDynamic* MakeZoneMaterial(UStaticMeshComponent* Mesh, UObject* Outer, const FLinearColor& Colour)
	{
		const UVehicleTuningData* Tuning = GetDefault<UCosmicHotshotsSettings>()->DefaultVehicleTuning.LoadSynchronous();
		UMaterialInterface* Placeholder = Tuning ? Tuning->PlaceholderMaterial.LoadSynchronous() : nullptr;
		UMaterialInstanceDynamic* Material = Placeholder ? UMaterialInstanceDynamic::Create(Placeholder, Outer) : nullptr;
		if (Material)
		{
			Material->SetVectorParameterValue(ZoneColourParameter, Colour);
			Mesh->SetMaterial(0, Material);
		}
		return Material;
	}

	// Placeholder look (cm): thickness of the floor plate, how high it and the label sit above the road.
	constexpr float ZonePlateThickness = 4.f;
	constexpr float ZonePlateHeight = 6.f;
	constexpr float ZoneLabelHeight = 350.f;
	constexpr float ZoneLabelSize = 120.f;
	constexpr float ZoneMarkerHeight = 12.f;
	/** The meteor marker starts at this share of the impact radius and grows to all of it. */
	constexpr float ZoneMarkerStartShare = 0.2f;
	const FLinearColor ZoneMarkerColour(1.f, 0.1f, 0.02f, 1.f);

	// Debug drawing (ch.Debug.Zones).
	constexpr float ZoneDebugHeight = 150.f;
	constexpr float ZoneDebugThickness = 6.f;
	constexpr int32 ZoneDebugCircleSegments = 32;
}

ATrackZone::ATrackZone()
{
	PrimaryActorTick.bCanEverTick = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// Zones never block or overlap anything: cars are tested against them by position in the physics step.
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(RootComponent);
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plate->SetMobility(EComponentMobility::Movable);
	Plate->SetCastShadow(false);

	MeteorMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeteorMarker"));
	MeteorMarker->SetupAttachment(RootComponent);
	MeteorMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeteorMarker->SetMobility(EComponentMobility::Movable);
	MeteorMarker->SetCastShadow(false);
	MeteorMarker->SetVisibility(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(ZoneLabelSize);
	// Faces the cars coming towards the zone.
	Label->SetRelativeLocationAndRotation(FVector(0.0, 0.0, ZoneLabelHeight), FRotator(0.0, 180.0, 0.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Plate->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		MeteorMarker->SetStaticMesh(CylinderMesh.Object);
	}
}

void ATrackZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateLook();
}

void ATrackZone::SetDefinition(const UTrackZoneDefinition* InDefinition)
{
	Definition = InDefinition;
	UpdateLook();
}

void ATrackZone::SetTrackPlacement(int32 InSeed, float InHalfWidth, const FVector& InTrackDirection)
{
	Seed = InSeed;
	PlacedHalfWidth = InHalfWidth;
	TrackDirection = InTrackDirection.GetSafeNormal2D();
	bPlaced = true;
	UpdateLook();
}

void ATrackZone::UpdateLook()
{
	if (!Definition)
	{
		return;
	}

	// Round zones show their whole reach; box zones their length and width.
	const bool bRound = Definition->Type == ETrackZoneType::Magnetonium || Definition->Type == ETrackZoneType::DustVortex;
	const float HalfWidth = bPlaced ? PlacedHalfWidth : FMath::Max(Definition->HalfWidth, Definition->HalfLength);
	const FVector HalfSize = bRound ? FVector(Definition->Radius, Definition->Radius, 0.f) : FVector(Definition->HalfLength, HalfWidth, 0.f);
	Plate->SetRelativeLocation(FVector(0.0, 0.0, ZonePlateHeight));
	Plate->SetRelativeScale3D(FVector(2.f * HalfSize.X / ZoneShapeSize, 2.f * HalfSize.Y / ZoneShapeSize, ZonePlateThickness / ZoneShapeSize));

	if (!PlateMaterial)
	{
		PlateMaterial = MakeZoneMaterial(Plate, this, Definition->Colour);
	}
	else
	{
		PlateMaterial->SetVectorParameterValue(ZoneColourParameter, Definition->Colour);
	}
	Label->SetText(Definition->DisplayName);
	Label->SetTextRenderColor(Definition->Colour.ToFColor(true));
}

void ATrackZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (!Definition || !World)
	{
		return;
	}

	// Meteor Cluster: show where the next meteor lands, growing until it does. The spot comes from the
	// same function the cars use to decide who is hit.
	const ARaceGameState* RaceState = World->GetGameState<ARaceGameState>();
	const bool bMeteors = Definition->Type == ETrackZoneType::MeteorCluster && bPlaced && RaceState;
	bool bShowMarker = false;
	if (bMeteors)
	{
		const float RaceTime = RaceState->GetRaceTime();
		const int32 NextImpact = FMath::FloorToInt32(RaceTime / Definition->MeteorPeriod) + 1;
		const float TimeToImpact = NextImpact * Definition->MeteorPeriod - RaceTime;
		if (TimeToImpact <= Definition->MeteorWarning)
		{
			FTrackZoneEntry Entry;
			Entry.Definition = Definition;
			Entry.HalfWidth = PlacedHalfWidth;
			Entry.Seed = Seed;
			float Along = 0.f;
			float Sideways = 0.f;
			FTrackZoneTable::GetMeteorSpot(Entry, NextImpact, Along, Sideways);

			const FVector Right(-TrackDirection.Y, TrackDirection.X, 0.0);
			const float Growth = Definition->MeteorWarning > UE_SMALL_NUMBER ? 1.f - TimeToImpact / Definition->MeteorWarning : 1.f;
			const float Radius = Definition->MeteorRadius * FMath::Lerp(ZoneMarkerStartShare, 1.f, Growth);
			MeteorMarker->SetWorldLocation(GetActorLocation() + TrackDirection * Along + Right * Sideways + FVector(0.0, 0.0, ZoneMarkerHeight));
			MeteorMarker->SetWorldScale3D(FVector(2.f * Radius / ZoneShapeSize, 2.f * Radius / ZoneShapeSize, ZonePlateThickness / ZoneShapeSize));
			if (!MarkerMaterial)
			{
				MarkerMaterial = MakeZoneMaterial(MeteorMarker, this, ZoneMarkerColour);
			}
			bShowMarker = true;
		}
	}
	MeteorMarker->SetVisibility(bShowMarker);

#if !UE_BUILD_SHIPPING
	if (CosmicHotshots::RaceCVars::DebugZones.GetValueOnGameThread() != 0)
	{
		const FColor Colour = Definition->Colour.ToFColor(true);
		const FVector Centre = GetActorLocation() + FVector(0.0, 0.0, ZoneDebugHeight);
		const bool bRound = Definition->Type == ETrackZoneType::Magnetonium || Definition->Type == ETrackZoneType::DustVortex;
		if (bRound)
		{
			DrawDebugCircle(World, Centre, Definition->Radius, ZoneDebugCircleSegments, Colour, false, -1.f, 0, ZoneDebugThickness, FVector::ForwardVector, FVector::RightVector, false);
		}
		else
		{
			const float HalfWidth = bPlaced ? PlacedHalfWidth : Definition->HalfWidth;
			DrawDebugBox(World, Centre, FVector(Definition->HalfLength, HalfWidth, ZoneDebugHeight), GetActorQuat(), Colour, false, -1.f, 0, ZoneDebugThickness);
		}
		DrawDebugString(World, Centre, FString::Printf(TEXT("%s (%s)"), *Definition->DisplayName.ToString(), *GetName()), nullptr, Colour, 0.f);
	}
#endif
}
