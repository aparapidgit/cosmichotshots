// Vehicle impacts: feedback side.
//
// The arcade response to hitting another car or a wall (knock-back by RamPower,
// Stability and mass, wobble or spin-out, wall bounce with partial speed loss,
// boost loss) is applied inside the fixed physics step by FVehicleImpactSim, so it
// is the same at every frame rate. This component only collects what those steps
// did and broadcasts OnImpact once per impact for camera shake, rumble, VFX and
// audio. It contains no physics and no presentation code.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleImpactComponent.generated.h"

class UHoverMovementComponent;
class UPrimitiveComponent;
class UVehicleStatsComponent;
class UVehicleTuningData;

UENUM(BlueprintType)
enum class EImpactReaction : uint8
{
	/** Bounce and feedback only. */
	None,
	/** Brief loss of grip and steering. */
	Wobble,
	/** Short spin with loss of control. */
	SpinOut,
};

/** What one impact did to this vehicle. */
USTRUCT(BlueprintType)
struct FVehicleImpactInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	TObjectPtr<AActor> OtherActor;

	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	bool bOtherIsVehicle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	FVector ImpactPoint = FVector::ZeroVector;

	/** Unit vector pointing from the thing that was hit towards this vehicle. */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	FVector Normal = FVector::ZeroVector;

	/** Speed at which the two closed in on each other along the normal (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float ClosingSpeed = 0.f;

	/** Speed change the arcade response gave this vehicle (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float SpeedChange = 0.f;

	/** 0..1 hit strength for feedback (camera shake, rumble, VFX, audio volume). 1 = spin-out strength. */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float Strength = 0.f;

	/** Share of the full boost meter (0..1) this hit took away. Only wall and object hits cost boost. */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float BoostLost = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	EImpactReaction Reaction = EImpactReaction::None;

	/** Simulation time of the physics step that applied the response (seconds since the vehicle started). */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float SimTime = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleImpact, const FVehicleImpactInfo&, Impact);

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class COSMICHOTSHOTSGAME_API UVehicleImpactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleImpactComponent();

	/** The movement component whose physics steps apply the impacts. */
	void Initialize(UHoverMovementComponent* InMovement);

	/** Fired once per impact that passed the minimum strength, after the physics step applied the response. */
	UPROPERTY(BlueprintAssignable, Category = "Impact")
	FOnVehicleImpact OnImpact;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Log, debug draw and broadcast of one impact. */
	void FinishImpact(const FVehicleImpactInfo& Info);

	UPROPERTY(Transient)
	TObjectPtr<UHoverMovementComponent> Movement;

	/** Reused every frame. */
	TArray<FVehicleImpactInfo> Impacts;
};
