// Character definition: one playable character and their hovercar.
// Menus, HUD and vehicles read characters from these assets; nothing is hard-coded.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Vehicle/VehicleStatModifiers.h"
#include "CharacterDefinition.generated.h"

class UActiveAbilityDefinition;
class UPassiveDefinition;
class UStaticMesh;
class UTexture2D;
class UVehicleTuningData;

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FText DisplayName;

	/** Character colour: tints the placeholder hovercar now, used by HUD and menus later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FLinearColor Colour = FLinearColor::White;

	/** Icon for standings, character select and results. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** Hovercar mesh. Empty = grey-box placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hovercar")
	TSoftObjectPtr<UStaticMesh> HovercarMesh;

	/** Base tuning for this hovercar. Empty = the project default tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hovercar")
	TObjectPtr<const UVehicleTuningData> BaseTuning;

	/** Small permanent stat differences on top of the base tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hovercar")
	FVehicleStatModifiers StatModifiers;

	/** Passive ability of this character (tunables and on/off switch). Empty = no passive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive")
	TObjectPtr<const UPassiveDefinition> PassiveDefinition;

	/** Active ability of this character (fired with the ability button for one Energy Core segment). Empty = none designed yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Active ability")
	TObjectPtr<const UActiveAbilityDefinition> ActiveAbility;
};
