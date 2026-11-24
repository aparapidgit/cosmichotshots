#include "Passive/PassiveBase.h"

#include "Race/RaceTrackerComponent.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierComponent.h"

bool FPassiveStepContext::QueueModifierOn(int32 OtherRacerId, const FVehicleModifierSpec& Spec) const
{
	const ARaceVehicle* Other = Tracker ? Tracker->GetRacerVehicle(OtherRacerId) : nullptr;
	if (!Other || !Other->GetModifiers())
	{
		return false;
	}
	Other->GetModifiers()->GetStack().Queue(Spec, SimTime);
	return true;
}
