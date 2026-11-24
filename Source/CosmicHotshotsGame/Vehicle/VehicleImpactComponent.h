// Vehicle impacts: arcade response to hitting another car or a wall.
//
// The physics engine only separates the bodies (no bounce, no friction). This
// component adds the arcade part on top: knock-back scaled by the attacker's
// RamPower, the defender's Stability and both masses, a short wobble or
// spin-out for strong hits, and wall bounce with partial speed loss.
// Each car handles only what happens to ITSELF, so a collision needs no
// shared bookkeeping. It broadcasts OnImpact for camera shake, rumble, VFX
// and audio; it contains no presentation code.

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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleImpact, const FVehicleImpactInfo&, Impact);

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class COSMICHOTSHOTSGAME_API UVehicleImpactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleImpactComponent();

	void Initialize(UPrimitiveComponent* InBody, UHoverMovementComponent* InMovement, const UVehicleStatsComponent* InStats);

	/** Fired for every impact that passed the minimum strength, after the response was applied. */
	UPROPERTY(BlueprintAssignable, Category = "Impact")
	FOnVehicleImpact OnImpact;

	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	void HandleVehicleHit(class ARaceVehicle& OtherVehicle, const FVector& Normal, float ClosingSpeed, const FHitResult& Hit);
	void HandleWallHit(AActor* OtherActor, const FVector& Normal, float ClosingSpeed, const FHitResult& Hit);

	/** Applies wobble or spin-out for a received speed change and fills in the feedback part of the info. */
	void ApplyReaction(FVehicleImpactInfo& Info, bool bAllowSpinOut);

	bool IsOnCooldown(const AActor* OtherActor) const;
	void StartCooldown(const AActor* OtherActor);

	void FinishImpact(const FVehicleImpactInfo& Info);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<UHoverMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleTuningData> Tuning;

	/** Final RamPower, Stability and mass of this vehicle. */
	UPROPERTY(Transient)
	TObjectPtr<const UVehicleStatsComponent> Stats;

	/** World time until which hits with an actor are ignored. */
	TMap<TWeakObjectPtr<const AActor>, double> CooldownEndTimes;
};
