// System input config: Enhanced Input assets for actions that are not driving (pause; later the HUD toggle).
// Kept apart from UDrivingInputConfig so driving bindings stay untouched when system actions are added.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SystemInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API USystemInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Key and button bindings of the system actions. Added to the local player next to the driving bindings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Above the driving bindings, so a system key is never swallowed by a driving action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 MappingPriority = 1;

	/** Press to open the pause menu during a race. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Pause;
};
