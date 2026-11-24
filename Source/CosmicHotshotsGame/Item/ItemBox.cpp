#include "Item/ItemBox.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Engine cube is 100 cm per side.
	constexpr float CubeMeshSize = 100.f;

	// Vector parameter of the engine's basic shape material.
	const FName BoxColourParameter(TEXT("Color"));
}

AItemBox::AItemBox()
{
	PrimaryActorTick.bCanEverTick = true;

	// The mesh bobs and spins under a root that stays where the box was placed.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	// Collecting is decided from positions in the physics step; the box never blocks or overlaps anything.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AItemBox::BeginPlay()
{
	Super::BeginPlay();

	Mesh->SetRelativeScale3D(FVector(BoxSize / CubeMeshSize));
	BoxMaterial = Mesh->CreateDynamicMaterialInstance(0);
	if (BoxMaterial)
	{
		BoxMaterial->SetVectorParameterValue(BoxColourParameter, BoxColour);
	}
}

void AItemBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bActive)
	{
		return;
	}

	// Visual only: the gameplay position of the box is where the actor was placed.
	VisualTime += DeltaSeconds;
	Mesh->AddLocalRotation(FRotator(0.0, SpinRate * DeltaSeconds, 0.0));
	Mesh->SetRelativeLocation(FVector(0.0, 0.0, BobHeight * FMath::Sin(2.f * UE_PI * BobFrequency * VisualTime)));
}

void AItemBox::SetBoxActive(bool bInActive)
{
	bActive = bInActive;
	Mesh->SetVisibility(bInActive);
}

AItemBoxCluster::AItemBoxCluster()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	BoxClass = AItemBox::StaticClass();
}

void AItemBoxCluster::BeginPlay()
{
	Super::BeginPlay();

	UClass* Class = BoxClass ? BoxClass.Get() : AItemBox::StaticClass();
	for (const float Offset : LateralOffsets)
	{
		GetWorld()->SpawnActor<AItemBox>(Class, GetActorLocation() + GetActorRightVector() * Offset, GetActorRotation());
	}
}
