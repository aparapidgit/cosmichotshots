// Glick's passive: fighting for space.
// In SIDE contact with another car Glick pushes the rival away harder and is pushed less
// himself. It only changes side contacts (UVehicleTuningData::SideContactMaxAngle): a nose or
// tail hit is unchanged. The two multipliers sit in his modifier stack for the whole race and
// the impact simulation reads them (FVehicleImpactSim).

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "GlickPassive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API UGlickPassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** Knock-back Glick GIVES in side contact (1.4 = +40 %). Placeholder, tune by playtesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glick", meta = (ClampMin = "1.0"))
	float SideRamPowerMultiplier = 1.4f;

	/** Knock-back Glick RECEIVES in side contact (0.5 = half). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glick", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SideSpeedLossMultiplier = 0.5f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API UGlickPassive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

private:
	bool bApplied = false;
};
