// Track zone: one zone placed on a track. Drop the actor on the road, pick its type (Definition)
// and turn it to face along the track (an Arrows Pad: the way it should push).
//
// The actor is only the PLACE and the look (placeholder: a flat coloured plate with the zone's
// name; a Meteor Cluster also shows where the next meteor lands). What a zone does to a car is
// decided in the car's fixed physics step (UVehicleZoneComponent), from the zone table the race
// builds out of these actors.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrackZoneActor.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTrackZoneDefinition;

UCLASS()
class COSMICHOTSHOTSGAME_API ATrackZone : public AActor
{
	GENERATED_BODY()

public:
	ATrackZone();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	void SetDefinition(const UTrackZoneDefinition* InDefinition);
	const UTrackZoneDefinition* GetDefinition() const { return Definition; }

	/** The race tells the zone where it sits on the track (for the meteor marker): its seed, resolved half width and the track's direction there. */
	void SetTrackPlacement(int32 InSeed, float InHalfWidth, const FVector& InTrackDirection);

protected:
	/** The type of this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<const UTrackZoneDefinition> Definition;

private:
	void UpdateLook();

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UTextRenderComponent> Label;

	/** Meteor Cluster: grows on the spot of the next impact. */
	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UStaticMeshComponent> MeteorMarker;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlateMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

	int32 Seed = 0;
	float PlacedHalfWidth = 0.f;
	FVector TrackDirection = FVector::ForwardVector;
	bool bPlaced = false;
};
