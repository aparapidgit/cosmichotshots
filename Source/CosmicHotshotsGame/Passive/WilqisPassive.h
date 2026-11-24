// Wilqis's passive: catching the next rival.
// A mild max-speed bonus while the gap to the nearest car AHEAD (measured in seconds along the
// track) is large enough. In 1st place there is no car ahead, so no bonus. The bonus switches
// on at a larger gap than it switches off at (hysteresis), so it never flickers.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "WilqisPassive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API UWilqisPassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Max speed multiplier while the bonus is on. Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wilqis", meta = (ClampMin = "1.0"))
	float MaxSpeedMultiplier = 1.04f;

	/** The bonus switches ON when the gap (seconds along the track) reaches this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wilqis", meta = (ClampMin = "0.0"))
	float ActivateGapSeconds = 1.5f;

	/** The bonus switches OFF when the gap shrinks to this. Keep it below ActivateGapSeconds (hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wilqis", meta = (ClampMin = "0.0"))
	float DeactivateGapSeconds = 0.8f;

	/** Seconds the gap must stay past a threshold before the bonus changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wilqis", meta = (ClampMin = "0.0"))
	float SwitchDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wilqis", meta = (ClampMin = "0.0"))
	float FadeIn = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wilqis", meta = (ClampMin = "0.0"))
	float FadeOut = 1.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UWilqisPassive : public UPassiveBase
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
