#include "Game/GameFlowSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Race/RaceDebug.h"

void UGameFlowSubsystem::SetRaceConfig(const FRaceSessionConfig& InConfig)
{
	Config = InConfig;
	bHasConfig = true;
}

void UGameFlowSubsystem::ClearRaceConfig()
{
	Config = FRaceSessionConfig();
	bHasConfig = false;
}

int32 UGameFlowSubsystem::GetConfiguredLaps() const
{
	if (!bHasConfig)
	{
		return 0;
	}
	if (Config.NumLaps > 0)
	{
		return Config.NumLaps;
	}
	return Config.Track ? Config.Track->DefaultLaps : 0;
}

void UGameFlowSubsystem::StartRace(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// The configured track's map, else the map that is open now (dev: there is only the test track).
	FString MapName = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true);
	if (bHasConfig && Config.Track && Config.Track->Map.IsValid())
	{
		MapName = Config.Track->Map.GetLongPackageName();
	}
	UGameplayStatics::OpenLevel(World, FName(*MapName));
}

bool UGameFlowSubsystem::IsWaitingForRaceSetup() const
{
	return !bHasConfig && CosmicHotshots::RaceCVars::SkipMenus.GetValueOnAnyThread() == 0;
}
