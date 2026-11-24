// Killjoke's passive: chasing the runaway leader.
// When the leader is far ahead of 2nd place (gap in seconds along the track), Killjoke gets a
// max-speed bonus until the gap shrinks. The leader never gets it: in 1st place Killjoke has no
// bonus. In 2nd place or lower the bonus follows the leader-to-second gap, with hysteresis.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "KilljokePassive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API UKilljokePassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Max speed multiplier while the bonus is on. Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killjoke", meta = (ClampMin = "1.0"))
	float MaxSpeedMultiplier = 1.05f;

	/** The bonus switches ON when the gap (seconds along the track) reaches this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killjoke", meta = (ClampMin = "0.0"))
	float ActivateGapSeconds = 3.0f;

	/** The bonus switches OFF when the gap shrinks to this. Keep it below ActivateGapSeconds (hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killjoke", meta = (ClampMin = "0.0"))
	float DeactivateGapSeconds = 1.5f;

	/** Seconds the gap must stay past a threshold before the bonus changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killjoke", meta = (ClampMin = "0.0"))
	float SwitchDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killjoke", meta = (ClampMin = "0.0"))
	float FadeIn = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killjoke", meta = (ClampMin = "0.0"))
	float FadeOut = 1.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UKilljokePassive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

private:
	FPassiveSwitch Switch;
	/** Gap the passive last looked at, in seconds; negative = none. */
	float LastGapSeconds = -1.f;
};
