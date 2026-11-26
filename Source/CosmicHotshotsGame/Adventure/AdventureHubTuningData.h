// Adventure hub tuning: every number and placeholder asset of the grey-box hub lives here.
//
// The placeholder hub is BUILT from this data and from the campaign when the hub map starts (see
// AAdventureHubGameMode): one open room per section in a row, closed off from the next by a wall with a
// gate, portals along the side walls. More sections or portals in the campaign need no map change.
// Units: centimetres, seconds, degrees.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AdventureHubTuningData.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UAdventureHubTuningData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The project's hub tuning (Project Settings > Game > Cosmic Hotshots > Adventure Hub Tuning), else the class defaults. */
	static const UAdventureHubTuningData* Get();

	// ------------------------------------------------------------- Layout

	/** Size of one section along the row of sections (the direction the gates lead). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "500.0"))
	float SectionLength = 4000.f;

	/** Size of one section across. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "500.0"))
	float SectionWidth = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "50.0"))
	float WallHeight = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "10.0"))
	float WallThickness = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "10.0"))
	float FloorThickness = 100.f;

	/** Width of the opening between two sections that the gate closes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "100.0"))
	float GateWidth = 800.f;

	/** How far from the side wall a portal stands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	float PortalWallInset = 500.f;

	/** Where the player appears in the first section, measured from its entrance wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	float PlayerStartInset = 500.f;

	/** Coming back from a race, the player appears this far in front of that level's portal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	float ReturnDistanceFromPortal = 450.f;

	// ------------------------------------------------------------ Portals

	/** The player can enter a portal (and reads its prompt) within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals", meta = (ClampMin = "50.0"))
	float InteractRadius = 350.f;

	/** The "gate closed" hint shows within this distance of a closed gate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals", meta = (ClampMin = "50.0"))
	float GateHintRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals", meta = (ClampMin = "10.0"))
	float PortalPadRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals", meta = (ClampMin = "10.0"))
	float PortalDoorWidth = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals", meta = (ClampMin = "10.0"))
	float PortalDoorHeight = 320.f;

	/** Size of the text above portals and on gates (world units). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals", meta = (ClampMin = "5.0"))
	float LabelSize = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals")
	FLinearColor LockedColour = FLinearColor(0.12f, 0.12f, 0.14f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals")
	FLinearColor OpenColour = FLinearColor(0.1f, 0.5f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals")
	FLinearColor WonColour = FLinearColor(0.15f, 0.8f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portals")
	FLinearColor GateColour = FLinearColor(0.75f, 0.15f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FLinearColor FloorColour = FLinearColor(0.35f, 0.35f, 0.37f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FLinearColor WallColour = FLinearColor(0.22f, 0.22f, 0.25f, 1.f);

	// ------------------------------------------------------------ On foot

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "10.0"))
	float WalkSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "10.0"))
	float RunSpeed = 850.f;

	/** How fast the character turns to face where it walks (degrees per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "10.0"))
	float TurnRate = 540.f;

	/** Camera distance behind the character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "50.0"))
	float CameraDistance = 550.f;

	/** The camera orbits a point this high above the character's centre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot")
	float CameraPivotHeight = 60.f;

	/** Camera pitch when the hub starts (negative = looking down). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot")
	float CameraStartPitch = -18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot")
	float CameraMinPitch = -70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot")
	float CameraMaxPitch = 30.f;

	/** How fast the right stick turns the camera at full deflection (degrees per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "1.0"))
	float StickLookRate = 150.f;

	/** Degrees of camera turn per unit of mouse movement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "0.01"))
	float MouseLookScale = 0.25f;

	/** How long a cinematic placeholder screen stays up before it ends by itself (the player can skip it sooner). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematics", meta = (ClampMin = "0.5"))
	float CinematicPlaceholderDuration = 4.f;

	/** How long the "level unlocked" / "gate open" message stays on screen after a race. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Foot", meta = (ClampMin = "0.0"))
	float MessageDuration = 6.f;

	// ------------------------------------------------- Placeholder assets

	/** A 100 cm cube: floors, walls, gates and portal doors are scaled copies of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeholders")
	TSoftObjectPtr<UStaticMesh> CubeMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));

	/** A cylinder 100 cm across and 100 cm high: portal pads and the on-foot body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeholders")
	TSoftObjectPtr<UStaticMesh> CylinderMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));

	/** Material of every placeholder; it must have the vector parameter named in ColourParameter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeholders")
	TSoftObjectPtr<UMaterialInterface> PlaceholderMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeholders")
	FName ColourParameter = TEXT("Color");
};
