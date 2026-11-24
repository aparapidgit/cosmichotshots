// Spark's passive: comeback from behind.
// While Spark is in LAST place (and has crossed the start line), a slightly higher max speed.
// The bonus ends when Spark is no longer last. Short delays in both directions keep it from
// flickering while two cars swap the last place back and forth.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "SparkPassive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API USparkPassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Max speed multiplier while last (1.06 = +6 %). Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spark", meta = (ClampMin = "1.0"))
	float MaxSpeedMultiplier = 1.06f;

	/** Seconds Spark must be last before the bonus starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spark", meta = (ClampMin = "0.0"))
	float ActivateDelay = 0.5f;

	/** Seconds Spark must be out of last place before the bonus ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spark", meta = (ClampMin = "0.0"))
	float DeactivateDelay = 0.5f;

	/** Seconds for the bonus to grow to full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spark", meta = (ClampMin = "0.0"))
	float FadeIn = 0.5f;

	/** Seconds for the bonus to fade away once it ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spark", meta = (ClampMin = "0.0"))
	float FadeOut = 1.f;

	/** Fewer drivers than this in the race: no bonus (being last of one means nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spark", meta = (ClampMin = "2"))
	int32 MinRacers = 2;
};

UCLASS()
class COSMICHOTSHOTSGAME_API USparkPassive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

	bool IsBonusActive() const { return bActive; }

private:
	bool bActive = false;
	/** Seconds the last-place condition has differed from bActive. */
	float PendingTime = 0.f;
};
