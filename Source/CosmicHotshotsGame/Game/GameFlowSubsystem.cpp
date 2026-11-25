#include "Game/GameFlowSubsystem.h"

#include "CosmicHotshotsGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Race/RaceDebug.h"

UGameFlowSubsystem* UGameFlowSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameFlowSubsystem>() : nullptr;
}

void UGameFlowSubsystem::OpenMainMenu(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	ClearRaceConfig();

	const FSoftObjectPath& MenuMap = GetDefault<UCosmicHotshotsSettings>()->MainMenuMap;
	if (!MenuMap.IsValid())
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("FLOW main menu: no map set (Project Settings > Game > Cosmic Hotshots > Main Menu Map); reloading the current map."));
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true)));
		return;
	}
	UE_LOG(LogCosmicRace, Log, TEXT("FLOW main menu: opening %s"), *MenuMap.GetLongPackageName());
	UGameplayStatics::OpenLevel(World, FName(*MenuMap.GetLongPackageName()));
}

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
