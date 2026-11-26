#include "Adventure/AdventureHubActors.h"

#include "Adventure/AdventureDefinitions.h"
#include "Adventure/AdventureHubTuningData.h"
#include "Adventure/AdventureSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "AdventureHub"

namespace
{
	/** The placeholder meshes are 100 cm across: scale = wanted size in cm times this. */
	constexpr float HubMeshUnitScale = 0.01f;

	/** Thickness of a portal's pad and door (cm). */
	constexpr float PortalPadHeight = 10.f;
	constexpr float PortalDoorThickness = 20.f;

	/** Gap between a label and the thing it belongs to (cm). */
	constexpr float HubLabelGap = 40.f;

	/** How far up a gate its label sits, as a share of the wall height. */
	constexpr float GateLabelHeightShare = 0.6f;

	constexpr float HubHalf = 0.5f;

	/** Gives a placeholder mesh component its mesh and a tintable copy of the placeholder material. */
	UMaterialInstanceDynamic* SetUpPlaceholder(UStaticMeshComponent* Component, UStaticMesh* Mesh, const UAdventureHubTuningData& Tuning)
	{
		Component->SetStaticMesh(Mesh);
		UMaterialInterface* Material = Tuning.PlaceholderMaterial.LoadSynchronous();
		if (!Material)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Component);
		Component->SetMaterial(0, Dynamic);
		return Dynamic;
	}
}

const UAdventureHubTuningData* UAdventureHubTuningData::Get()
{
	if (const UAdventureHubTuningData* ProjectTuning = GetDefault<UCosmicHotshotsSettings>()->AdventureHubTuning.LoadSynchronous())
	{
		return ProjectTuning;
	}
	return GetDefault<UAdventureHubTuningData>();
}

// --- layout

FTransform CosmicHotshots::HubLayout::GetPortalTransform(const UAdventureCampaignDefinition& Campaign, const UAdventureHubTuningData& Tuning, int32 LevelIndex)
{
	const int32 SectionIndex = FMath::Max(Campaign.GetSectionOfLevel(LevelIndex), 0);
	const int32 IndexInSection = LevelIndex - Campaign.GetFirstLevelOfSection(SectionIndex);
	const int32 NumInSection = Campaign.Sections.IsValidIndex(SectionIndex) ? Campaign.Sections[SectionIndex].Levels.Num() : 1;

	// Portals alternate between the two side walls and are spread evenly along the section.
	const int32 NumRows = FMath::Max((NumInSection + 1) / 2, 1);
	const int32 Row = IndexInSection / 2;
	const float Side = (IndexInSection % 2 == 0) ? -1.f : 1.f;

	const FVector Location(
		SectionIndex * Tuning.SectionLength + Tuning.SectionLength * (Row + 1) / (NumRows + 1),
		Side * (Tuning.SectionWidth * HubHalf - Tuning.PortalWallInset),
		0.f);
	// Facing away from its wall, into the section.
	return FTransform(FRotator(0.f, Side < 0.f ? 90.f : -90.f, 0.f), Location);
}

FVector CosmicHotshots::HubLayout::GetGateLocation(const UAdventureHubTuningData& Tuning, int32 SectionIndex)
{
	return FVector(SectionIndex * Tuning.SectionLength, 0.f, 0.f);
}

int32 CosmicHotshots::HubLayout::GetSectionAt(const UAdventureHubTuningData& Tuning, int32 NumSections, const FVector& Location)
{
	return FMath::Clamp(FMath::FloorToInt32(Location.X / FMath::Max(Tuning.SectionLength, 1.f)), 0, FMath::Max(NumSections - 1, 0));
}

FTransform CosmicHotshots::HubLayout::GetPlayerStart(const UAdventureHubTuningData& Tuning)
{
	return FTransform(FRotator::ZeroRotator, FVector(Tuning.PlayerStartInset, 0.f, 0.f));
}

// --- AAdventurePortal

AAdventurePortal::AAdventurePortal()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(RootComponent);
	// The player walks onto the pad: nothing of the portal is in the way.
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	Door->SetupAttachment(RootComponent);
	Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
}

void AAdventurePortal::SetLevelIndex(int32 InLevelIndex)
{
	LevelIndex = InLevelIndex;
	ShownState = INDEX_NONE;
}

void AAdventurePortal::BeginPlay()
{
	Super::BeginPlay();

	// The portal stands on the floor at its actor location and faces along its forward axis.
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	const float PadDiameter = Tuning.PortalPadRadius * 2.f;

	ColourMaterial = SetUpPlaceholder(Pad, Tuning.CylinderMesh.LoadSynchronous(), Tuning);
	Pad->SetRelativeScale3D(FVector(PadDiameter, PadDiameter, PortalPadHeight) * HubMeshUnitScale);
	Pad->SetRelativeLocation(FVector(0.f, 0.f, PortalPadHeight * HubHalf));

	// The door stands at the back edge of the pad and shares the pad's colour.
	Door->SetStaticMesh(Tuning.CubeMesh.LoadSynchronous());
	if (ColourMaterial)
	{
		Door->SetMaterial(0, ColourMaterial);
	}
	Door->SetRelativeScale3D(FVector(PortalDoorThickness, Tuning.PortalDoorWidth, Tuning.PortalDoorHeight) * HubMeshUnitScale);
	Door->SetRelativeLocation(FVector(-Tuning.PortalPadRadius, 0.f, Tuning.PortalDoorHeight * HubHalf));

	Label->SetWorldSize(Tuning.LabelSize);
	Label->SetRelativeLocation(FVector(-Tuning.PortalPadRadius + PortalDoorThickness, 0.f, Tuning.PortalDoorHeight + HubLabelGap));

	Refresh();
}

EAdventurePortalState AAdventurePortal::GetState() const
{
	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	if (Adventure && Adventure->IsLevelWon(LevelIndex))
	{
		return EAdventurePortalState::Won;
	}
	return Adventure && Adventure->IsLevelUnlocked(LevelIndex) ? EAdventurePortalState::Open : EAdventurePortalState::Locked;
}

void AAdventurePortal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Refresh();
}

void AAdventurePortal::Refresh()
{
	const EAdventurePortalState State = GetState();
	if (ShownState == static_cast<int32>(State))
	{
		return;
	}
	ShownState = static_cast<int32>(State);

	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	const FLinearColor Colour = State == EAdventurePortalState::Won ? Tuning.WonColour
		: State == EAdventurePortalState::Open ? Tuning.OpenColour : Tuning.LockedColour;
	if (ColourMaterial)
	{
		ColourMaterial->SetVectorParameterValue(Tuning.ColourParameter, Colour);
	}

	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	const UAdventureLevelDefinition* Level = Campaign ? Campaign->GetLevel(LevelIndex) : nullptr;
	const FText StateText = State == EAdventurePortalState::Won ? LOCTEXT("PortalWon", "WON")
		: State == EAdventurePortalState::Open ? LOCTEXT("PortalOpen", "OPEN") : LOCTEXT("PortalLocked", "LOCKED");
	Label->SetText(FText::Format(LOCTEXT("PortalLabel", "{0}<br>{1}"), Level ? Level->DisplayName : LOCTEXT("PortalNoLevel", "?"), StateText));
	Label->SetTextRenderColor(State == EAdventurePortalState::Locked ? FColor(150, 150, 150) : FColor::White);
}

// --- AAdventureGate

AAdventureGate::AAdventureGate()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	Block->SetupAttachment(RootComponent);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
}

void AAdventureGate::SetSectionIndex(int32 InSectionIndex)
{
	SectionIndex = InSectionIndex;
	ShownOpen = INDEX_NONE;
}

void AAdventureGate::BeginPlay()
{
	Super::BeginPlay();

	// The gate fills the opening in the wall: centred on its actor location, standing on the floor.
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();
	if (UMaterialInstanceDynamic* Material = SetUpPlaceholder(Block, Tuning.CubeMesh.LoadSynchronous(), Tuning))
	{
		Material->SetVectorParameterValue(Tuning.ColourParameter, Tuning.GateColour);
	}
	Block->SetRelativeScale3D(FVector(Tuning.WallThickness, Tuning.GateWidth, Tuning.WallHeight) * HubMeshUnitScale);
	Block->SetRelativeLocation(FVector(0.f, 0.f, Tuning.WallHeight * HubHalf));

	// The label faces the section the player comes from.
	Label->SetWorldSize(Tuning.LabelSize);
	Label->SetRelativeLocation(FVector(-(Tuning.WallThickness * HubHalf + 1.f), 0.f, Tuning.WallHeight * GateLabelHeightShare));
	Label->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));

	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	if (Campaign && Campaign->Sections.IsValidIndex(SectionIndex) && Campaign->Sections.IsValidIndex(SectionIndex - 1))
	{
		Label->SetText(FText::Format(LOCTEXT("GateLabel", "{0}<br>Win every level of {1}"),
			Campaign->Sections[SectionIndex].DisplayName, Campaign->Sections[SectionIndex - 1].DisplayName));
	}

	Refresh();
}

bool AAdventureGate::IsOpen() const
{
	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	return Adventure && Adventure->IsSectionOpen(SectionIndex);
}

void AAdventureGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Refresh();
}

void AAdventureGate::Refresh()
{
	const bool bOpen = IsOpen();
	if (ShownOpen == (bOpen ? 1 : 0))
	{
		return;
	}
	ShownOpen = bOpen ? 1 : 0;

	// An open gate is simply gone: nothing to see and nothing in the way.
	SetActorHiddenInGame(bOpen);
	SetActorEnableCollision(!bOpen);
}

#undef LOCTEXT_NAMESPACE
