// Race vehicle: one rigid body hovercar. It does not know whether a human or an
// AI drives it; drivers only call SetInput().

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/VehicleInputState.h"
#include "RaceVehicle.generated.h"

class UHoverMovementComponent;
class UPhysicalMaterial;
class URaceCameraComponent;
class UStaticMeshComponent;
class UVehicleHullComponent;
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

	UHoverMovementComponent* GetHoverMovement() const { return HoverMovement; }
	URaceCameraComponent* GetRaceCamera() const { return RaceCamera; }

	/** This vehicle's tuning asset, else the project default (Project Settings > Cosmic Hotshots), else the class defaults. */
	const UVehicleTuningData* GetActiveTuning() const;

	virtual void PostInitializeComponents() override;

protected:
	/** Sizes the collision hull and the grey-box mesh from the tuning asset. */
	void ApplyBodyShape(const UVehicleTuningData& ActiveTuning);

	/** Applies mass, damping and surface material from the tuning asset. Runtime only. */
	void ApplyBodyPhysics(const UVehicleTuningData& ActiveTuning);

	/** Simulated root: the one rigid body of the vehicle, with a rounded two-capsule hull. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleHullComponent> Body;

	/** Grey-box visual (engine cube). No collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UHoverMovementComponent> HoverMovement;

	/** Third-person chase camera of this vehicle's driver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<URaceCameraComponent> RaceCamera;

	/** Tuning asset for this vehicle. When empty, the project default tuning is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<const UVehicleTuningData> Tuning;

	/** Runtime surface material of the body (friction and restitution come from tuning). */
	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> BodySurfaceMaterial;
};
