#include "Vehicle/RaceVehicle.h"

#include "CosmicHotshotsGame.h"
#include "Camera/RaceCameraComponent.h"
#include "Character/CharacterDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/HoverMovementComponent.h"
#include "Vehicle/VehicleHullComponent.h"
#include "Vehicle/VehicleImpactComponent.h"
#include "Vehicle/VehicleStatsComponent.h"
#include "Vehicle/VehicleTuningData.h"

namespace
{
	const FName VehicleCollisionProfile(TEXT("Vehicle"));

	// Engine cube is 100 cm per side.
	constexpr double GreyBoxMeshSize = 100.0;

	// Vector parameter the placeholder material must expose (see UVehicleTuningData::PlaceholderMaterial).
	const FName GreyBoxColourParameter(TEXT("Colour"));
}

ARaceVehicle::ARaceVehicle()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UVehicleHullComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(VehicleCollisionProfile);
	Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(false); // gravity is applied by the hover simulation
	// No hit events: impacts are read from the physics step itself (UVehicleContactSubsystem).
	Body->SetNotifyRigidBodyCollision(false);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Body);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GreyBoxMesh = CubeMesh.Object;
		VisualMesh->SetStaticMesh(GreyBoxMesh);
	}

	Stats = CreateDefaultSubobject<UVehicleStatsComponent>(TEXT("Stats"));

	HoverMovement = CreateDefaultSubobject<UHoverMovementComponent>(TEXT("HoverMovement"));

	Impact = CreateDefaultSubobject<UVehicleImpactComponent>(TEXT("Impact"));

	RaceCamera = CreateDefaultSubobject<URaceCameraComponent>(TEXT("RaceCamera"));
	RaceCamera->SetupAttachment(Body);
}

float ARaceVehicle::GetBodyMass() const
{
	return Body->GetMass();
}

void ARaceVehicle::HandleImpact(const FVehicleImpactInfo& ImpactInfo)
{
	RaceCamera->AddImpactShake(ImpactInfo.Strength);
}

const UCharacterDefinition* ARaceVehicle::GetCharacter() const
{
	if (Character)
	{
		return Character;
	}
	return GetDefault<UCosmicHotshotsSettings>()->DefaultCharacter.LoadSynchronous();
}

const UVehicleTuningData* ARaceVehicle::GetActiveTuning() const
{
	if (Tuning)
	{
		return Tuning;
	}

	const UCharacterDefinition* ActiveCharacter = GetCharacter();
	if (ActiveCharacter && ActiveCharacter->BaseTuning)
	{
		return ActiveCharacter->BaseTuning;
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

	HoverMovement->SetUpdatedBody(Body);
	Impact->OnImpact.AddUniqueDynamic(this, &ARaceVehicle::HandleImpact);

	ApplyCharacter();
}

void ARaceVehicle::SetCharacter(const UCharacterDefinition* InCharacter)
{
	Character = InCharacter;
	ApplyCharacter();
}

void ARaceVehicle::ApplyCharacter()
{
	const UCharacterDefinition* ActiveCharacter = GetCharacter();
	const UVehicleTuningData* ActiveTuning = GetActiveTuning();

	Stats->SetBase(ActiveTuning, ActiveCharacter ? ActiveCharacter->StatModifiers : FVehicleStatModifiers());

	ApplyBodyShape(*ActiveTuning);
	ApplyBodyPhysics(*ActiveTuning);
	ApplyAppearance(ActiveCharacter);

	HoverMovement->SetTuning(ActiveTuning);
	HoverMovement->SetStats(Stats);
	Impact->Initialize(HoverMovement);
}

void ARaceVehicle::ApplyBodyShape(const UVehicleTuningData& ActiveTuning)
{
	Body->SetHullSize(ActiveTuning.BodySize);
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
	Body->SetMassOverrideInKg(NAME_None, Stats->GetMassKg(), true);
}

void ARaceVehicle::ApplyAppearance(const UCharacterDefinition* ActiveCharacter)
{
	UStaticMesh* CharacterMesh = ActiveCharacter ? ActiveCharacter->HovercarMesh.LoadSynchronous() : nullptr;

	if (CharacterMesh)
	{
		// A real hovercar mesh is authored at its final size.
		VisualMesh->SetStaticMesh(CharacterMesh);
		VisualMesh->SetRelativeScale3D(FVector::OneVector);
		return;
	}

	// Grey-box placeholder: a box of the body's size, tinted with the character colour.
	VisualMesh->SetStaticMesh(GreyBoxMesh);
	VisualMesh->SetRelativeScale3D(Stats->GetTuning()->BodySize / GreyBoxMeshSize);

	const UVehicleTuningData* ActiveTuning = Stats->GetTuning();
	UMaterialInterface* PlaceholderMaterial = ActiveTuning->PlaceholderMaterial.LoadSynchronous();
	if (!PlaceholderMaterial)
	{
		return;
	}

	if (!GreyBoxMaterial || GreyBoxMaterial->Parent != PlaceholderMaterial)
	{
		GreyBoxMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial, this);
	}
	VisualMesh->SetMaterial(0, GreyBoxMaterial);
	GreyBoxMaterial->SetVectorParameterValue(GreyBoxColourParameter, ActiveCharacter ? ActiveCharacter->Colour : ActiveTuning->PlaceholderColour);
}

void ARaceVehicle::SetInput(const FVehicleInputState& InInput)
{
	HoverMovement->SetInput(InInput);
}
