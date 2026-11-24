// Freya's passive: aggressive overtaking.
// After a CLOSE overtake (she passed within the close-overtake distance sideways, see
// URaceTuningData::CloseOvertakeLateral), the overtaken car briefly loses a little max speed.
// The effect is a modifier on the OTHER car's stack, so it starts in that car's next physics
// step. Hitting the same car again while it is still slowed just restarts the effect.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "FreyaPassive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API UFreyaPassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Max speed multiplier put on the overtaken car (0.93 = -7 %). Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freya", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float VictimMaxSpeedMultiplier = 0.93f;

	/** Seconds the overtaken car stays slowed at full effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freya", meta = (ClampMin = "0.0"))
	float Duration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freya", meta = (ClampMin = "0.0"))
	float FadeIn = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freya", meta = (ClampMin = "0.0"))
	float FadeOut = 0.5f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UFreyaPassive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

private:
	/** Close overtakes that slowed a rival so far. */
	int32 TriggerCount = 0;
	/** Seconds since the last trigger (for the debug display); negative = never. */
	float TimeSinceTrigger = -1.f;
};
