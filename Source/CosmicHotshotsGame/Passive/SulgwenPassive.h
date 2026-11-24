// Sulgwen's passive: chain overtaking.
// The first overtake gives a short bonus to max speed AND acceleration. Every further
// overtake before the timer runs out raises the chain level (stronger bonuses) and restarts
// the timer. When the timer runs out the chain is over and the bonuses decay gradually.
// Overtakes come from the race tracker, which already ignores jitter, lapping and a repeat
// overtake of the same rival within its pair cooldown.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "SulgwenPassive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API USulgwenPassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Max speed gained per chain level (0.03 = +3 % per overtake). Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "0.0"))
	float MaxSpeedPerLevel = 0.03f;

	/** Acceleration gained per chain level (0.05 = +5 % per overtake). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "0.0"))
	float AccelerationPerLevel = 0.05f;

	/** Highest chain level; caps the total bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "1"))
	int32 MaxLevel = 4;

	/** Seconds the bonus lasts after the latest overtake. Another overtake within it continues the chain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "0.1"))
	float BonusDuration = 4.f;

	/** Seconds for the bonuses to decay to base values once the timer has run out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "0.0"))
	float DecayTime = 2.f;

	/** Seconds for a new or raised bonus to reach full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "0.0"))
	float FadeIn = 0.3f;

	/**
	 * Seconds before overtaking the SAME rival counts for the chain again. 0 = only the race
	 * tracker's own pair cooldown applies (URaceTuningData::OvertakePairCooldown).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sulgwen", meta = (ClampMin = "0.0"))
	float RivalCooldown = 0.f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API USulgwenPassive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

	int32 GetChainLevel() const { return Level; }
	float GetTimeRemaining() const { return TimeRemaining; }

private:
	/** Current chain level, 0 = no chain. */
	int32 Level = 0;
	/** Seconds left to continue the chain. */
	float TimeRemaining = 0.f;
	/** Race time of the last counted overtake of each rival (by driver id). */
	TMap<int32, double> LastOvertakeOfRival;
};
