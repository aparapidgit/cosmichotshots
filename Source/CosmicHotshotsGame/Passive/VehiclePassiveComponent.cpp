#include "Passive/VehiclePassiveComponent.h"

#include "CosmicHotshotsGame.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Passive/PassiveBase.h"
#include "Race/RaceGameState.h"
#include "Race/RaceTrackerComponent.h"
#include "Vehicle/RaceVehicle.h"
#include "Vehicle/VehicleModifierComponent.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPassivesEnabled(
		TEXT("ch.Passives.Enabled"), 1,
		TEXT("0 = switch off ALL character passives (to test base driving feel and the AI baseline)."), ECVF_Cheat);
}

UVehiclePassiveComponent::UVehiclePassiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehiclePassiveComponent::SetModifiers(UVehicleModifierComponent* InModifiers)
{
	Modifiers = InModifiers;
}

void UVehiclePassiveComponent::SetDefinition(const UPassiveDefinition* InDefinition)
{
	if (Passive && Passive->GetDefinition() == InDefinition)
	{
		return;
	}

	if (Passive && Modifiers)
	{
		Passive->Deactivate(Modifiers->GetStack());
	}
	Passive = nullptr;
	bRunning = false;
	LastLoggedState.Reset();

	const TSubclassOf<UPassiveBase> PassiveClass = InDefinition ? InDefinition->GetPassiveClass() : nullptr;
	if (PassiveClass)
	{
		Passive = NewObject<UPassiveBase>(this, PassiveClass);
		Passive->Initialize(InDefinition);
	}
}

void UVehiclePassiveComponent::StepPassive(float DeltaTime, float SimTime)
{
	if (!Passive || !Modifiers)
	{
		return;
	}

	const bool bEnabled = CVarPassivesEnabled.GetValueOnAnyThread() != 0 && Passive->GetDefinition()->bPassiveEnabled;
	if (!bEnabled)
	{
		if (bRunning)
		{
			bRunning = false;
			Passive->Deactivate(Modifiers->GetStack());
		}
		return;
	}
	bRunning = true;

	if (!Tracker.IsValid())
	{
		const ARaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ARaceGameState>() : nullptr;
		Tracker = RaceState ? RaceState->GetTracker() : nullptr;
	}

	FPassiveStepContext Context;
	Context.DeltaTime = DeltaTime;
	Context.Modifiers = &Modifiers->GetStack();
	if (URaceTrackerComponent* RaceTracker = Tracker.Get())
	{
		if (RacerId == INDEX_NONE)
		{
			RacerId = RaceTracker->FindRacerId(Cast<ARaceVehicle>(GetOwner()));
		}
		Context.Race = &RaceTracker->GetViewForStep(SimTime);
		Context.RacerId = RacerId;
	}

	Passive->Step(Context);

#if !UE_BUILD_SHIPPING
	// A log line whenever the passive's state changes, stamped with the race time it reacted to.
	const FString State = Passive->GetLogState();
	if (State != LastLoggedState)
	{
		LastLoggedState = State;
		UE_LOG(LogCosmicRace, Log, TEXT("PASSIVE %s t=%.3f %s"), *GetNameSafe(GetOwner()), Context.Race ? Context.Race->RaceTime : 0.0, *State);
	}
#endif
}
