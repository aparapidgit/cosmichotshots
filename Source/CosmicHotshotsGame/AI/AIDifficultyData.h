// AI difficulty: one parameter set for computer drivers (Easy, Normal, Hard ...).
// The "Variety" values are ranges: every driver draws its own personality inside them from its
// seed (FRaceAIPersonality), so the drivers of one race differ in line, courage, wall hugging,
// boost use and how often they slip up.
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

	/**
	 * Share of the car's full steering lock the driver plans its corner speeds with. A corner too tight to
	 * be turned at speed with that much lock (a hairpin) is braked for. Lower = slower through tight corners.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pace", meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float CornerSteeringShare = 0.75f;

	/** Deceleration (cm/s^2) the driver plans its braking with. Lower = brakes earlier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pace", meta = (ClampMin = "100.0"))
	float PlannedBraking = 2000.f;

	// Line

	/** How far (cm) from the centre line the driver moves towards the inside of a corner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.0"))
	float ApexOffset = 700.f;

	/** Curvature (1 / corner radius in cm, averaged over the track just ahead) at which the full apex offset is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.00001"))
	float ApexFullCurvature = 0.00012f;

	/** Each driver keeps its own line this far (cm) to one side at most, so they do not all drive nose to tail. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.0"))
	float PersonalLineSpread = 250.f;

	/** How fast (cm/s) the driver moves its line sideways. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "10.0"))
	float LineChangeSpeed = 600.f;

	/** The centre of the car stays at least this far (cm) from the walls, except when the driver hugs a wall on purpose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "100.0"))
	float WallMargin = 250.f;

	/**
	 * Gap (cm) between the side of the car and the inside wall the driver aims for through corners, to charge the
	 * boost meter with a close pass. 0 = never hugs the wall. Smaller = more charge and more risk of touching.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.0"))
	float WallHugClearance = 0.f;

	/** The driver leaves its line by at most this much (cm) to collect an item box. 0 = only boxes that lie on its line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.0"))
	float ItemBoxMaxSidestep = 1100.f;

	/** How fast the steering moves, in full locks per second. Lower = smoother and slower to react. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0.5"))
	float SteeringSpeed = 6.f;

	// Drift and boost

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost")
	bool bUseDrift = false;

	/** Corners at least this sharp (curvature, 1 / radius in cm) are drifted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseDrift", ClampMin = "0.00001"))
	float DriftMinCurvature = 0.00012f;

	/** Only corners that last at least this long (s at the current speed) are drifted: no drifting through a quick chicane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseDrift", ClampMin = "0.0"))
	float DriftMinCornerTime = 1.2f;

	/** Slide angle (degrees) the driver tries to stay under in a drift. Past the car's critical angle it would lose control. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseDrift", ClampMin = "5.0", ClampMax = "90.0"))
	float DriftMaxSlipAngle = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost")
	bool bUseBoost = true;

	/** Boost meter (0..1) needed before the driver starts a boost on a straight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseBoost", ClampMin = "0.0", ClampMax = "1.0"))
	float BoostMinCharge = 0.3f;

	/** The driver boosts only where the track ahead turns no sharper than this (curvature, 1 / radius in cm). Low = straights only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift and boost", meta = (EditCondition = "bUseBoost", ClampMin = "0.0"))
	float BoostMaxCurvature = 0.00002f;

	// Variety (ranges the drivers' personalities are drawn from)

	/** Each driver cruises up to this much below TopSpeedFactor (0.02 = up to 2 % slower on straights). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float TopSpeedSpread = 0.f;

	/** Each driver's corner speed and braking differ by up to this share either way (0.15 = +-15 % sideways acceleration and braking). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float CornerCourageSpread = 0.1f;

	/** Share of the drivers that hug the inside wall at all (when WallHugClearance > 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallHugShare = 1.f;

	/** A cautious driver keeps up to this much (cm) more gap to the wall than WallHugClearance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = "0.0"))
	float WallHugClearanceSpread = 40.f;

	/** Each driver's BoostMinCharge differs by up to this much either way: some burn the boost at once, others save it up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float BoostChargeSpread = 0.15f;

	// Racing other cars (the same towards humans and computer drivers)

	/**
	 * Defending: with a faster car closing in from behind, the driver moves once towards that car's side of the
	 * track, by at most this much (cm), and then holds that line. 0 = never defends. Each driver's own
	 * aggression scales it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Racing", meta = (ClampMin = "0.0"))
	float DefendMaxShift = 150.f;

	/** While overtaking, the driver starts a boost with this much in the meter already (if lower than BoostMinCharge). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Racing", meta = (EditCondition = "bUseBoost", ClampMin = "0.0", ClampMax = "1.0"))
	float OvertakeBoostCharge = 0.2f;

	// Mistakes (small, seeded per driver, lap and stretch of track: the same race always has the same ones)

	/** Chance of a small mistake on each stretch of track (about 200 m). 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mistakes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MistakeChance = 0.06f;

	/** Mistake "lift": seconds off the throttle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mistakes", meta = (ClampMin = "0.0"))
	float MistakeLiftTime = 0.6f;

	/** Mistake "wide": how far (cm) the driver runs wide of its line for that stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mistakes", meta = (ClampMin = "0.0"))
	float MistakeWideOffset = 350.f;

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
