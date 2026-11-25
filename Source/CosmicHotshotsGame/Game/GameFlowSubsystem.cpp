#include "Game/GameFlowSubsystem.h"

#include "CosmicHotshotsGame.h"
#include "Character/CharacterDefinition.h"
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

bool UGameFlowSubsystem::ConsumeRaceSetupRequest()
{
	const bool bRequested = bRaceSetupRequested;
	bRaceSetupRequested = false;
	return bRequested;
}

void UGameFlowSubsystem::StartSingleRace(const UObject* WorldContext)
{
	const UCosmicHotshotsSettings* Settings = GetDefault<UCosmicHotshotsSettings>();

	FRaceSessionConfig NewConfig = SingleRaceSetup;
	if (NewConfig.HumanCharacters.IsEmpty())
	{
		NewConfig.HumanCharacters.Add(nullptr);
	}
	if (!NewConfig.Track && !Settings->Tracks.IsEmpty())
	{
		NewConfig.Track = Settings->Tracks[0].LoadSynchronous();
	}
	NewConfig.NumAIOpponents = FMath::Clamp(NewConfig.NumAIOpponents, 0, UCosmicHotshotsSettings::MaxGridSize - NewConfig.HumanCharacters.Num());

	// Rivals: a random draw from the characters nobody drives, made now. Characters stay unique per race.
	TArray<const UCharacterDefinition*> Pool = Settings->GetRaceCharacters();
	Pool.RemoveAll([&NewConfig](const UCharacterDefinition* Candidate) { return NewConfig.HumanCharacters.Contains(Candidate); });

	const int32 CVarSeed = CosmicHotshots::RaceCVars::AICharacterSeed.GetValueOnGameThread();
	const int32 Seed = CVarSeed >= 0 ? CVarSeed : static_cast<int32>(FPlatformTime::Cycles64() & MAX_int32);
	FRandomStream Random(Seed);
	for (int32 Index = Pool.Num() - 1; Index > 0; --Index)
	{
		Pool.Swap(Index, Random.RandRange(0, Index));
	}

	FString RivalNames;
	NewConfig.AICharacters.Reset();
	for (int32 Index = 0; Index < NewConfig.NumAIOpponents && Index < Pool.Num(); ++Index)
	{
		NewConfig.AICharacters.Add(Pool[Index]);
		RivalNames += (Index > 0 ? TEXT(",") : TEXT("")) + GetNameSafe(Pool[Index]);
	}

	UE_LOG(LogCosmicRace, Log, TEXT("FLOW single race: player=%s rivals=%d laps=%d difficulty=%d items=%d track=%s seed=%d ai=[%s]"),
		*GetNameSafe(NewConfig.HumanCharacters[0]), NewConfig.NumAIOpponents, NewConfig.NumLaps, NewConfig.AIDifficulty,
		NewConfig.bItemBoxes ? 1 : 0, *GetNameSafe(NewConfig.Track), Seed, *RivalNames);

	SetRaceConfig(NewConfig);
	StartRace(WorldContext);
}

void UGameFlowSubsystem::OpenMainMenu(const UObject* WorldContext, bool bOpenRaceSetup)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	ClearRaceConfig();
	bRaceSetupRequested = bOpenRaceSetup;

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
