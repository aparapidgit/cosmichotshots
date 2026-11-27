// Race vehicle: one rigid body hovercar. It does not know whether a human or an
// AI drives it; drivers only call SetInput().

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/VehicleImpactComponent.h"
#include "Vehicle/VehicleInputState.h"
#include "RaceVehicle.generated.h"

class UCharacterDefinition;
class UHoverMovementComponent;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class URaceCameraComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVehicleHullComponent;
class UVehicleModifierComponent;
class UVehicleAbilityComponent;
class UVehiclePassiveComponent;
class UVehicleZoneComponent;
class UVehicleStatsComponent;
class UVehicleTuningData;

UCLASS()
class COSMICHOTSHOTSGAME_API ARaceVehicle : public APawn
{
	GENERATED_BODY()

public:
	ARaceVehicle();

	/** Driver intent for this frame. Called by a PlayerController or an AIController. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetInput(const FVehicleInputState& InInput);

	/** Makes this vehicle the given character's hovercar: tuning, stats, mass and look. Can be called at any time. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetCharacter(const UCharacterDefinition* InCharacter);

	/** This vehicle's character, else the project default character. Can be null (plain base tuning). */
	const UCharacterDefinition* GetCharacter() const;

	UHoverMovementComponent* GetHoverMovement() const { return HoverMovement; }
	URaceCameraComponent* GetRaceCamera() const { return RaceCamera; }
	UVehicleImpactComponent* GetImpact() const { return Impact; }
	UVehicleStatsComponent* GetStats() const { return Stats; }
	UVehicleModifierComponent* GetModifiers() const { return Modifiers; }
	UVehiclePassiveComponent* GetPassive() const { return Passive; }
	UVehicleZoneComponent* GetZones() const { return Zones; }
	UVehicleAbilityComponent* GetAbility() const { return Ability; }

	/** Mass of the vehicle's rigid body in kg. */
	float GetBodyMass() const;

	/**
	 * Base tuning in use: this vehicle's own tuning asset, else the character's, else the
	 * project default (Project Settings > Cosmic Hotshots), else the class defaults.
	 * For final values (with character modifiers) ask GetStats().
	 */
	const UVehicleTuningData* GetActiveTuning() const;

	virtual void PostInitializeComponents() override;

protected:
	/** Resolves character and tuning and applies them to every part of the vehicle. */
	void ApplyCharacter();

	/** Sizes the collision hull from the tuning asset. */
	void ApplyBodyShape(const UVehicleTuningData& ActiveTuning);

	/** Applies mass, damping and surface material. Runtime only. */
	void ApplyBodyPhysics(const UVehicleTuningData& ActiveTuning);

	/** Hovercar mesh of the character, or the tinted grey-box placeholder. */
	void ApplyAppearance(const UCharacterDefinition* ActiveCharacter);

	/** Forwards impacts to this vehicle's own presentation (camera shake). */
	UFUNCTION()
	void HandleImpact(const FVehicleImpactInfo& ImpactInfo);

	/** Simulated root: the one rigid body of the vehicle, with a rounded two-capsule hull. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleHullComponent> Body;

	/** Visual: the character's hovercar mesh, or a grey box. No collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Final values of this vehicle (base tuning x character modifiers). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleStatsComponent> Stats;

	/** Temporary stat bonuses and penalties (passives, items, abilities). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleModifierComponent> Modifiers;

	/** Runs the character's passive. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehiclePassiveComponent> Passive;

	/** Runs the character's active ability. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleAbilityComponent> Ability;

	/** What the track's zones do to this car. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleZoneComponent> Zones;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UHoverMovementComponent> HoverMovement;

	/** Arcade response to hitting other cars and walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleImpactComponent> Impact;

	/** Third-person chase camera of this vehicle's driver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<URaceCameraComponent> RaceCamera;

	/** Character driving this vehicle. When empty, the project default character is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<const UCharacterDefinition> Character;

	/** Tuning override for this one vehicle. When empty, the character's or the project default tuning is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<const UVehicleTuningData> Tuning;

	/** Runtime surface material of the body (friction and restitution come from tuning). */
	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> BodySurfaceMaterial;

	/** Engine cube used while a character has no hovercar mesh. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> GreyBoxMesh;

	/** Tintable material instance of the grey box. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GreyBoxMaterial;
};
