// Race rules tuning: laps, overtake detection, gaps and track projection.
// All values are placeholders to be tuned by playtesting. The project default asset is set in
// Project Settings > Cosmic Hotshots > Race; without one, the class defaults below are used.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RaceTuningData.generated.h"

UCLASS(BlueprintType)
class COSMICHOTSHOTSGAME_API URaceTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Laps per race (later the race setup / track definition overrides this). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "1"))
	int32 NumLaps = 3;

	/** How far around its previous position a car is searched for on the track line (cm, each way). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "100.0"))
	float ProjectionWindow = 3000.f;

	/** When the nearby search finds the line farther than this (cm), the whole track is searched (teleports). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "100.0"))
	float ReacquireDistance = 2500.f;

	/** A checkpoint marker farther than this (cm, sideways) from the track line is reported as misplaced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "0.0"))
	float CheckpointTolerance = 500.f;

	/** A new order between two cars must hold this long (s) before it counts as an overtake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtake", meta = (ClampMin = "0.0"))
	float OvertakeConfirmTime = 0.4f;

	/** One car counts as ahead of another only with at least this lead along the track (cm). Hysteresis against jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtake", meta = (ClampMin = "0.0"))
	float OvertakeMargin = 200.f;

	/** After A overtakes B, another overtake of B by A counts again only after this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtake", meta = (ClampMin = "0.0"))
	float OvertakePairCooldown = 3.f;

	/**
	 * An overtake is close when the sideways distance (cm) between the cars at the moment they were
	 * level is at most this. The speed difference does not matter (developer decision).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtake", meta = (ClampMin = "0.0"))
	float CloseOvertakeLateral = 600.f;

	/** Time gaps: every car records when it passed each stretch of this length (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaps", meta = (ClampMin = "50.0"))
	float GapBucketSize = 1000.f;

	/** Time gaps where no passing time exists yet: distance divided by at least this speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaps", meta = (ClampMin = "1.0"))
	float MinGapSpeed = 1000.f;

	/** Seconds before a collected item box comes back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0.1"))
	float ItemBoxRespawnTime = 5.f;

	/** Seconds the item roulette runs before the item can be used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0.0"))
	float ItemRollDuration = 1.5f;

	/** A car collects a box when its centre comes this close to the box (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "10.0"))
	float ItemPickupRadius = 250.f;

	/** Seconds a computer driver keeps a ready item before using it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0.0"))
	float AIItemUseDelay = 1.f;

	/** Seconds of countdown (3-2-1) before the cars may move. 0 = the race starts at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0.0"))
	float CountdownSeconds = 3.f;

	/** Seconds after the first human finishes before the race ends for everybody still driving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0.0"))
	float FinishTimeout = 30.f;
};
