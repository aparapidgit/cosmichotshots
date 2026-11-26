#include "Adventure/AdventureHubGameMode.h"

#include "CosmicHotshotsGame.h"
#include "Adventure/AdventureDefinitions.h"
#include "Adventure/AdventureHubActors.h"
#include "Adventure/AdventureHubTuningData.h"
#include "Adventure/AdventureSubsystem.h"
#include "Adventure/HubCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** The placeholder cube is 100 cm: scale = wanted size in cm times this. */
	constexpr float HubBlockUnitScale = 0.01f;

	constexpr float HubGmHalf = 0.5f;

	/** Save slot used when the hub map is opened on its own (dev: Play in the editor on the hub map). */
	constexpr int32 HubDevSaveSlot = 0;

	/** The player is spawned this far above the floor, so the capsule never starts inside it (cm). */
	constexpr float HubSpawnHeight = 110.f;
}

AAdventureHubGameMode::AAdventureHubGameMode()
{
	DefaultPawnClass = AHubCharacter::StaticClass();
	PlayerControllerClass = AHubPlayerController::StaticClass();
	HUDClass = nullptr;
}

void AAdventureHubGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	if (!Campaign)
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("HUB: no Adventure campaign (Project Settings > Game > Cosmic Hotshots > Adventure Campaign): the hub stays empty."));
		return;
	}

	// The hub opened without the Adventure menu (dev): carry on with the first slot, or start one.
	if (!Adventure->HasActiveGame() && !Adventure->LoadGame(HubDevSaveSlot))
	{
		Adventure->StartNewGame(HubDevSaveSlot);
	}

	// A hub map with its own portals is a designed hub: nothing to build.
	if (TActorIterator<AAdventurePortal>(GetWorld()))
	{
		UE_LOG(LogCosmicRace, Log, TEXT("HUB: the map has its own portals, the placeholder hub is not built."));
		return;
	}
	BuildHub(*Campaign, *UAdventureHubTuningData::Get());
}

void AAdventureHubGameMode::SpawnBlock(const UAdventureHubTuningData& Tuning, const FVector& Centre, const FVector& Size, const FLinearColor& Colour)
{
	UStaticMesh* Cube = Tuning.CubeMesh.LoadSynchronous();
	AStaticMeshActor* Block = Cube ? GetWorld()->SpawnActor<AStaticMeshActor>(Centre, FRotator::ZeroRotator) : nullptr;
	if (!Block)
	{
		return;
	}
	UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
	// Spawned at runtime, so not static geometry.
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(Cube);
	Block->SetActorScale3D(Size * HubBlockUnitScale);
	if (UMaterialInterface* Material = Tuning.PlaceholderMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Mesh);
		Dynamic->SetVectorParameterValue(Tuning.ColourParameter, Colour);
		Mesh->SetMaterial(0, Dynamic);
	}
}

void AAdventureHubGameMode::BuildHub(const UAdventureCampaignDefinition& Campaign, const UAdventureHubTuningData& Tuning)
{
	const int32 NumSections = Campaign.Sections.Num();
	const float Length = Tuning.SectionLength;
	const float Width = Tuning.SectionWidth;
	const float Height = Tuning.WallHeight;
	const float Thick = Tuning.WallThickness;

	// Sections stand in a row along +X; the floor's top is at Z = 0.
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		const float CentreX = SectionIndex * Length + Length * HubGmHalf;
		SpawnBlock(Tuning, FVector(CentreX, 0.f, -Tuning.FloorThickness * HubGmHalf), FVector(Length, Width, Tuning.FloorThickness), Tuning.FloorColour);

		// The two side walls.
		for (const float Side : { -1.f, 1.f })
		{
			SpawnBlock(Tuning, FVector(CentreX, Side * (Width + Thick) * HubGmHalf, Height * HubGmHalf), FVector(Length, Thick, Height), Tuning.WallColour);
		}

		if (SectionIndex == 0)
		{
			continue;
		}

		// The wall to the section before, with the gate's opening in the middle.
		const FVector GateLocation = CosmicHotshots::HubLayout::GetGateLocation(Tuning, SectionIndex);
		const float PieceWidth = (Width - Tuning.GateWidth) * HubGmHalf;
		for (const float Side : { -1.f, 1.f })
		{
			SpawnBlock(Tuning, GateLocation + FVector(0.f, Side * (Tuning.GateWidth + PieceWidth) * HubGmHalf, Height * HubGmHalf),
				FVector(Thick, PieceWidth, Height), Tuning.WallColour);
		}
		if (AAdventureGate* Gate = GetWorld()->SpawnActorDeferred<AAdventureGate>(AAdventureGate::StaticClass(), FTransform(GateLocation)))
		{
			Gate->SetSectionIndex(SectionIndex);
			Gate->FinishSpawning(FTransform(GateLocation));
		}
	}

	// The two end walls close the row.
	for (const float EndX : { -Thick * HubGmHalf, NumSections * Length + Thick * HubGmHalf })
	{
		SpawnBlock(Tuning, FVector(EndX, 0.f, Height * HubGmHalf), FVector(Thick, Width + 2.f * Thick, Height), Tuning.WallColour);
	}

	// One portal per level.
	const int32 NumLevels = Campaign.GetNumLevels();
	for (int32 LevelIndex = 0; LevelIndex < NumLevels; ++LevelIndex)
	{
		const FTransform PortalTransform = CosmicHotshots::HubLayout::GetPortalTransform(Campaign, Tuning, LevelIndex);
		if (AAdventurePortal* Portal = GetWorld()->SpawnActorDeferred<AAdventurePortal>(AAdventurePortal::StaticClass(), PortalTransform))
		{
			Portal->SetLevelIndex(LevelIndex);
			Portal->FinishSpawning(PortalTransform);
		}
	}

	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	int32 NumOpenLevels = 0;
	int32 NumOpenSections = 0;
	for (int32 LevelIndex = 0; LevelIndex < NumLevels; ++LevelIndex)
	{
		NumOpenLevels += Adventure->IsLevelUnlocked(LevelIndex) ? 1 : 0;
	}
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		NumOpenSections += Adventure->IsSectionOpen(SectionIndex) ? 1 : 0;
	}
	UE_LOG(LogCosmicRace, Log, TEXT("HUB built: sections=%d portals=%d gates=%d; won=%d unlocked=%d sectionsOpen=%d"),
		NumSections, NumLevels, FMath::Max(NumSections - 1, 0), Adventure->GetNumLevelsWon(), NumOpenLevels, NumOpenSections);
}

APawn* AAdventureHubGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	const UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(this);
	const UAdventureCampaignDefinition* Campaign = Adventure ? Adventure->GetCampaign() : nullptr;
	if (!Campaign)
	{
		return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	}
	const UAdventureHubTuningData& Tuning = *UAdventureHubTuningData::Get();

	// Back from a race: in front of that level's portal, looking away from it. Otherwise the hub entrance.
	FTransform Start = CosmicHotshots::HubLayout::GetPlayerStart(Tuning);
	if (Campaign->GetLevel(Adventure->GetCurrentLevel()))
	{
		const FTransform Portal = CosmicHotshots::HubLayout::GetPortalTransform(*Campaign, Tuning, Adventure->GetCurrentLevel());
		Start = FTransform(Portal.GetRotation(), Portal.GetLocation() + Portal.GetRotation().GetForwardVector() * Tuning.ReturnDistanceFromPortal);
	}
	Start.AddToTranslation(FVector(0.f, 0.f, HubSpawnHeight));

	APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Start);
	if (NewPlayer)
	{
		// The camera starts looking the way the character faces.
		NewPlayer->SetControlRotation(Start.Rotator());
	}
	return Pawn;
}
