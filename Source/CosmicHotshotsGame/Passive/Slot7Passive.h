// Slot 7's passive (working name): drift master / clean line.
// Every well-executed drift raises a chain level, and each level gives a small max-speed bonus.
// A drift counts when it lasted long enough, reached a real slip angle, and ended without the
// car losing control (overslide) and without a collision during it. A wall or car hit resets the
// chain at once. Developer decisions: a counted drift lasts at least 2 seconds, and the chain
// runs out when no new counted drift follows in time (the bonus then decays gradually).
// A drift that does not count neither raises nor breaks the chain.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "Slot7Passive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API USlot7PassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Max speed gained per chain level (0.015 = +1.5 % per counted drift). Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.0"))
	float MaxSpeedPerLevel = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "1"))
	int32 MaxLevel = 5;

	/** A drift counts only when it lasted at least this long (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.0"))
	float MinDriftDuration = 2.f;

	/** A drift counts only when it reached at least this slip angle (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.0"))
	float MinSlipAngle = 10.f;

	/** Seconds after the latest counted drift before the chain runs out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.1"))
	float ChainTimeout = 15.f;

	/** Seconds for the bonus to decay once the chain has run out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.0"))
	float DecayTime = 2.f;

	/** Seconds for the bonus to disappear after a collision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.0"))
	float HitFadeOut = 0.3f;

	/** Seconds for a new or raised bonus to reach full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot7", meta = (ClampMin = "0.0"))
	float FadeIn = 0.3f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API USlot7Passive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

private:
	int32 Level = 0;
	/** Seconds left before the chain runs out. */
	float TimeRemaining = 0.f;

	// The drift in progress.
	bool bWasDrifting = false;
	float DriftTime = 0.f;
	float MaxSlipAngle = 0.f;
	/** The drift in progress lost control or had a collision: it will not count. */
	bool bDriftSpoiled = false;

	/** Impact count seen in the previous step; negative = not seen yet. */
	int32 LastImpactCount = -1;
};
