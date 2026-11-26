// On-foot input config: Enhanced Input assets for walking around the Adventure hub.
// The hub player controller reads this asset, so input assets are never hard-coded.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OnFootInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UOnFootInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Key and button bindings for walking. Added to the local player while they are in the hub. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 MappingPriority = 0;

	/** Axis2D: X = right, Y = forward (relative to the camera). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Move;

	/** Axis2D from a stick: X = turn right, Y = look up. A rate (held = keeps turning). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Look;

	/** Axis2D from the mouse: X = turn right, Y = look up. A movement (per frame). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> LookMouse;

	/** Hold to run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Run;

	/** Press to enter the portal the character stands at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Interact;
};
