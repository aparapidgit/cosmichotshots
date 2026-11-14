#include "Vehicle/RaceVehicle.h"

#include "CosmicHotshotsGame.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/VehicleHullComponent.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	const FName VehicleCollisionProfile(TEXT("Vehicle"));

	// Engine cube is 100 cm per side.
	constexpr double GreyBoxMeshSize = 100.0;

	// TEMPORARY camera placement, removed with the temporary camera.
	constexpr float TemporaryCameraDistance = 900.f;
	const FVector TemporaryCameraOffset(0.0, 0.0, 250.0);
	const FRotator TemporaryCameraPitch(-12.0, 0.0, 0.0);
	constexpr float TemporaryCameraLagSpeed = 8.f;
}

ARaceVehicle::ARaceVehicle()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UVehicleHullComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(VehicleCollisionProfile);
	Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(false); // gravity is applied by the hover simulation
	Body->SetNotifyRigidBodyCollision(true);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Body);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeMesh.Object);
	}

	HoverMovement = CreateDefaultSubobject<UHoverMovementComponent>(TEXT("HoverMovement"));

	TemporaryCameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TemporaryCameraArm"));
	TemporaryCameraArm->SetupAttachment(Body);
	TemporaryCameraArm->TargetArmLength = TemporaryCameraDistance;
	TemporaryCameraArm->SocketOffset = TemporaryCameraOffset;
	TemporaryCameraArm->SetRelativeRotation(TemporaryCameraPitch);
	TemporaryCameraArm->bDoCollisionTest = false;
	TemporaryCameraArm->bEnableCameraLag = true;
	TemporaryCameraArm->CameraLagSpeed = TemporaryCameraLagSpeed;

	TemporaryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TemporaryCamera"));
	TemporaryCamera->SetupAttachment(TemporaryCameraArm, USpringArmComponent::SocketName);
}

const UVehicleTuningData* ARaceVehicle::GetActiveTuning() const
{
	if (Tuning)
	{
		return Tuning;
	}
	if (const UVehicleTuningData* ProjectDefault = GetDefault<UCosmicHotshotsSettings>()->DefaultVehicleTuning.LoadSynchronous())
	{
		return ProjectDefault;
	}
	return GetDefault<UVehicleTuningData>();
}

void ARaceVehicle::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	const UVehicleTuningData* ActiveTuning = GetActiveTuning();
	ApplyBodyShape(*ActiveTuning);
	ApplyBodyPhysics(*ActiveTuning);

	HoverMovement->SetUpdatedBody(Body);
	HoverMovement->SetTuning(ActiveTuning);
}

void ARaceVehicle::ApplyBodyShape(const UVehicleTuningData& ActiveTuning)
{
	Body->SetHullSize(ActiveTuning.BodySize);
	VisualMesh->SetRelativeScale3D(ActiveTuning.BodySize / GreyBoxMeshSize);
}

void ARaceVehicle::ApplyBodyPhysics(const UVehicleTuningData& ActiveTuning)
{
	if (!BodySurfaceMaterial)
	{
		BodySurfaceMaterial = NewObject<UPhysicalMaterial>(this, TEXT("BodySurfaceMaterial"));
	}
	BodySurfaceMaterial->Friction = ActiveTuning.BodyFriction;
	BodySurfaceMaterial->StaticFriction = ActiveTuning.BodyFriction;
	BodySurfaceMaterial->Restitution = ActiveTuning.BodyRestitution;
	BodySurfaceMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	BodySurfaceMaterial->bOverrideFrictionCombineMode = true;
	BodySurfaceMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
	BodySurfaceMaterial->bOverrideRestitutionCombineMode = true;
	Body->SetPhysMaterialOverride(BodySurfaceMaterial);

	Body->SetAngularDamping(ActiveTuning.AngularDamping);
	Body->SetLinearDamping(0.f);
	Body->SetMassOverrideInKg(NAME_None, ActiveTuning.MassKg, true);
}

void ARaceVehicle::SetInput(const FVehicleInputState& InInput)
{
	HoverMovement->SetInput(InInput);
}
