// AI difficulty: one parameter set for computer drivers (Easy, Normal, Hard ...).
// All values are placeholders to be tuned by playtesting. The project lists its difficulty
// assets in Project Settings > Cosmic Hotshots > AI; without assets, these class defaults are used.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIDifficultyData.generated.h"

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API UAIDifficultyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FText DisplayName;

	// Pace

	/** Share of the car's top speed the driver aims for on straights (1 = flat out). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pace", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float TopSpeedFactor = 0.95f;

	/** Sideways acceleration (cm/s^2) the driver dares in corners: corner speed = sqrt(this x corner radius). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pace", meta = (ClampMin = "100.0"))
	float CornerAcceleration = 1800.f;

	/** Deceleration (cm/s^2) the driver plans its braking with. Lower = brakes earlier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pace", meta = (ClampMin = "100.0"))
	float PlannedBraking = 2000.f;

	// Line

	/** How far (cm) from the centre line the driver moves towards the inside of a corner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.0"))
	float ApexOffset = 700.f;

	/** Curvature (1 / corner radius in cm) at which the full apex offset is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.00001"))
	float ApexFullCurvature = 0.00012f;

	/** Each driver keeps its own line this far (cm) to one side at most, so they do not all drive nose to tail. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.0"))
	float PersonalLineSpread = 250.f;

	/** How fast (cm/s) the driver moves its line sideways. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "10.0"))
	float LineChangeSpeed = 600.f;

	/** How fast the steering moves, in full locks per second. Lower = smoother and slower to react. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.5"))
	float SteeringSpeed = 6.f;

	// Drift and boost

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost")
	bool bUseDrift = false;

	/** Corners at least this sharp (curvature, 1 / radius in cm) are drifted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseDrift", ClampMin = "0.00001"))
	float DriftMinCurvature = 0.00012f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost")
	bool bUseBoost = true;

	/** Boost meter (0..1) needed before the driver starts a boost on a straight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseBoost", ClampMin = "0.0", ClampMax = "1.0"))
	float BoostMinCharge = 0.3f;

	// Rubber banding (kept mild: passives already help trailing cars)

	/** Max speed bonus (0.04 = +4 %) for a computer driver far BEHIND the best human. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber banding", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RubberBandCatchUp = 0.04f;

	/** Max speed loss (0.04 = -4 %) for a computer driver far AHEAD of the best human. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber banding", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RubberBandSlowDown = 0.04f;

	/** Gap to the best human along the track (cm) at which the full effect applies; it grows linearly up to there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber banding", meta = (ClampMin = "100.0"))
	float RubberBandFullGap = 30000.f;
};
