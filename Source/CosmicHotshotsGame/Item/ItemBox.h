// Item box: a floating box on the track that gives an item when a car drives through it.
//
// The box itself has no gameplay logic and no collision. UItemSystemComponent decides in the
// fixed physics step who collected it and when it comes back; this actor only shows that:
// it floats, turns and bobs, and hides while it is waiting to respawn. Placeholder look: a
// coloured cube. AItemBoxCluster places a row of boxes across the track.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemBox.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

UCLASS()
class COSMICHOTSHOTSGAME_API AItemBox : public AActor
{
	GENERATED_BODY()

public:
	AItemBox();

	virtual void Tick(float DeltaSeconds) override;

	/** Shown and collectable, or hidden while it waits to respawn. Called by the item system. */
	void SetBoxActive(bool bInActive);
	bool IsBoxActive() const { return bActive; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Box")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Edge length of the placeholder cube (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box", meta = (ClampMin = "10.0"))
	float BoxSize = 200.f;

	/** Colour of the placeholder cube: bright, so the box reads from far away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box")
	FLinearColor BoxColour = FLinearColor(1.f, 0.75f, 0.05f, 1.f);

	/** Degrees per second the box turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box")
	float SpinRate = 60.f;

	/** How far (cm) and how fast (cycles per second) the box bobs up and down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box", meta = (ClampMin = "0.0"))
	float BobHeight = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box", meta = (ClampMin = "0.0"))
	float BobFrequency = 0.5f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoxMaterial;

	bool bActive = true;
	float VisualTime = 0.f;
};

/** A row of item boxes across the track (three by default), for quick placement in a level. */
UCLASS()
class COSMICHOTSHOTSGAME_API AItemBoxCluster : public AActor
{
	GENERATED_BODY()

public:
	AItemBoxCluster();

protected:
	virtual void BeginPlay() override;

	/** Sideways positions of the boxes (cm along the actor's right axis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box")
	TArray<float> LateralOffsets = { -900.f, 0.f, 900.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Box")
	TSubclassOf<AItemBox> BoxClass;
};
