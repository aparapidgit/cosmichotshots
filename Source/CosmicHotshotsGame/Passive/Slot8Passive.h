// Slot 8's passive (working name): defending the lead.
// Developer decision:
//  - 4 or more drivers in the race: while in the TOP 3, recovery from hits is shorter and less
//    speed is lost in hits.
//  - fewer than 4 drivers: while EXACTLY 2nd, recovery is a little shorter and less speed is lost.
//  - otherwise nothing.
// Kept mild because most other passives already help trailing cars. The place must hold for a
// short delay before the bonus changes, so it does not flicker while positions swap.

#pragma once

#include "CoreMinimal.h"
#include "Passive/PassiveBase.h"
#include "Slot8Passive.generated.h"

UCLASS()
class COSMICHOTSHOTSGAME_API USlot8PassiveDefinition : public UPassiveDefinition
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UPassiveBase> GetPassiveClass() const override;

	/** From this many drivers on, the top-places rule applies; below it, the small-field rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8", meta = (ClampMin = "2"))
	int32 MinRacersForTopRule = 4;

	/** Top-places rule: the bonus is on while the place is this or better. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8|Top places", meta = (ClampMin = "1"))
	int32 TopPlaces = 3;

	/** Top-places rule: length of wobble / spin-out (0.6 = 40 % shorter). Placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8|Top places", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TopRecoveryTimeMultiplier = 0.6f;

	/** Top-places rule: speed lost in hits (0.7 = 30 % less). Placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8|Top places", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TopSpeedLossMultiplier = 0.7f;

	/** Small-field rule: the bonus is on while the place is exactly this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8|Small field", meta = (ClampMin = "1"))
	int32 SmallFieldPlace = 2;

	/** Small-field rule: length of wobble / spin-out (0.8 = 20 % shorter). Placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8|Small field", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SmallFieldRecoveryTimeMultiplier = 0.8f;

	/** Small-field rule: speed lost in hits (0.7 = 30 % less). Placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8|Small field", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SmallFieldSpeedLossMultiplier = 0.7f;

	/** Seconds a place must hold before the bonus changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8", meta = (ClampMin = "0.0"))
	float SwitchDelay = 0.5f;

	/** Seconds for the bonus to grow in and to fade away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot8", meta = (ClampMin = "0.0"))
	float Fade = 0.3f;
};

UCLASS()
class COSMICHOTSHOTSGAME_API USlot8Passive : public UPassiveBase
{
	GENERATED_BODY()

public:
	virtual void Step(const FPassiveStepContext& Context) override;
	virtual void Deactivate(FVehicleModifierStack& Modifiers) override;
	virtual FString GetDebugString() const override;
	virtual FString GetLogState() const override;

private:
	enum class EMode : uint8 { Off, TopPlaces, SmallField };

	EMode Mode = EMode::Off;
	/** Seconds the wanted mode has differed from Mode. */
	float PendingTime = 0.f;
};
