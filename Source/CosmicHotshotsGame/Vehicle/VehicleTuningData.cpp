#include "Vehicle/VehicleTuningData.h"

UVehicleTuningData::UVehicleTuningData()
{
	// Four corners on the underside of the default 400 x 200 x 100 body.
	HoverPoints = {
		FVector(150.0, -80.0, -50.0),
		FVector(150.0, 80.0, -50.0),
		FVector(-150.0, -80.0, -50.0),
		FVector(-150.0, 80.0, -50.0),
	};

	// Default steering curve: weak at standstill, full at low speed, about half at top speed.
	FRichCurve* Curve = SteeringBySpeed.GetRichCurve();
	Curve->AddKey(0.0f, 0.35f);
	Curve->AddKey(0.15f, 1.0f);
	Curve->AddKey(1.0f, 0.55f);
}
