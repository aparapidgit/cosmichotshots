#include "Adventure/AdventureSubsystem.h"

#include "CosmicHotshotsGame.h"
#include "Adventure/AdventureDefinitions.h"
#include "Character/CharacterDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/CosmicHotshotsSettings.h"
#include "Game/GameFlowSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	TAutoConsoleVariable<int32> CVarAdventureUnlockAll(
		TEXT("ch.Adventure.UnlockAll"), 0,
		TEXT("Dev: 1 = every Adventure level and every section gate is open. Progress in the save is not changed."), ECVF_Cheat);

	TAutoConsoleVariable<FString> CVarAdventureSaveSlot(
		TEXT("ch.Adventure.SaveSlot"), TEXT("Adventure"),
		TEXT("Name of the Adventure save slots (the slot index is appended). Tests use their own name."), ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarAdventureForcePlace(
		TEXT("ch.Test.AdventureForcePlace"), 0,
		TEXT("Test helper: the Adventure unlock logic takes this as the player's place in the finished race. 0 = the real place."), ECVF_Cheat);

	/** Save games are per platform user; the game has one. */
	constexpr int32 AdventureSaveUserIndex = 0;

	/** Dev console command: forget the progress of the active Adventure game. */
	FAutoConsoleCommandWithWorld CmdAdventureReset(
		TEXT("ch.Adventure.Reset"),
		TEXT("Dev: forgets everything the active Adventure game has won and seen."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UAdventureSubsystem* Adventure = UAdventureSubsystem::Get(World))
			{
				Adventure->ResetProgress();
			}
		}));
}

// --- UAdventureCampaignDefinition

int32 UAdventureCampaignDefinition::GetNumLevels() const
{
	int32 NumLevels = 0;
	for (const FAdventureSection& Section : Sections)
	{
		NumLevels += Section.Levels.Num();
	}
	return NumLevels;
}

const UAdventureLevelDefinition* UAdventureCampaignDefinition::GetLevel(int32 LevelIndex) const
{
	int32 FirstOfSection = 0;
	for (const FAdventureSection& Section : Sections)
	{
		if (LevelIndex >= FirstOfSection && LevelIndex < FirstOfSection + Section.Levels.Num())
		{
			return Section.Levels[LevelIndex - FirstOfSection];
		}
		FirstOfSection += Section.Levels.Num();
	}
	return nullptr;
}

int32 UAdventureCampaignDefinition::GetSectionOfLevel(int32 LevelIndex) const
{
	int32 FirstOfSection = 0;
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		if (LevelIndex >= FirstOfSection && LevelIndex < FirstOfSection + Sections[SectionIndex].Levels.Num())
		{
			return SectionIndex;
		}
		FirstOfSection += Sections[SectionIndex].Levels.Num();
	}
	return INDEX_NONE;
}

int32 UAdventureCampaignDefinition::GetFirstLevelOfSection(int32 SectionIndex) const
{
	int32 FirstOfSection = 0;
	for (int32 Index = 0; Index < SectionIndex && Index < Sections.Num(); ++Index)
	{
		FirstOfSection += Sections[Index].Levels.Num();
	}
	return FirstOfSection;
}

// --- UAdventureSubsystem

UAdventureSubsystem* UAdventureSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAdventureSubsystem>() : nullptr;
}

const UAdventureCampaignDefinition* UAdventureSubsystem::GetCampaign() const
{
	return GetDefault<UCosmicHotshotsSettings>()->AdventureCampaign.LoadSynchronous();
}

FString UAdventureSubsystem::GetSlotName(int32 SlotIndex) const
{
	return FString::Printf(TEXT("%s_%d"), *CVarAdventureSaveSlot.GetValueOnGameThread(), SlotIndex);
}

bool UAdventureSubsystem::DoesSlotExist(int32 SlotIndex) const
{
	return UGameplayStatics::DoesSaveGameExist(GetSlotName(SlotIndex), AdventureSaveUserIndex);
}

int32 UAdventureSubsystem::GetSlotLevelsWon(int32 SlotIndex) const
{
	if (SlotIndex == ActiveSlot && ActiveSave)
	{
		return GetNumLevelsWon();
	}
	const UAdventureSaveGame* Save = DoesSlotExist(SlotIndex)
		? Cast<UAdventureSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(SlotIndex), AdventureSaveUserIndex)) : nullptr;
	return Save ? Save->WonLevels.Num() : 0;
}

void UAdventureSubsystem::StartNewGame(int32 SlotIndex)
{
	ActiveSave = Cast<UAdventureSaveGame>(UGameplayStatics::CreateSaveGameObject(UAdventureSaveGame::StaticClass()));
	ActiveSlot = SlotIndex;
	CurrentLevelIndex = INDEX_NONE;
	LastOutcome = FAdventureRaceOutcome();
	SaveActiveGame();
	UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE new game in slot %d"), SlotIndex);
}

bool UAdventureSubsystem::LoadGame(int32 SlotIndex)
{
	UAdventureSaveGame* Save = DoesSlotExist(SlotIndex)
		? Cast<UAdventureSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(SlotIndex), AdventureSaveUserIndex)) : nullptr;
	if (!Save)
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("ADVENTURE load: slot %d is empty"), SlotIndex);
		return false;
	}
	ActiveSave = Save;
	ActiveSlot = SlotIndex;
	CurrentLevelIndex = INDEX_NONE;
	LastOutcome = FAdventureRaceOutcome();
	UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE loaded slot %d: %d levels won, intro seen=%d"), SlotIndex, GetNumLevelsWon(), Save->bIntroSeen ? 1 : 0);
	return true;
}

void UAdventureSubsystem::SaveActiveGame() const
{
	if (ActiveSave && ActiveSlot != INDEX_NONE)
	{
		const bool bSaved = UGameplayStatics::SaveGameToSlot(ActiveSave, GetSlotName(ActiveSlot), AdventureSaveUserIndex);
		if (!bSaved)
		{
			UE_LOG(LogCosmicRace, Warning, TEXT("ADVENTURE save of slot %d failed"), ActiveSlot);
		}
	}
}

bool UAdventureSubsystem::IsLevelWon(int32 LevelIndex) const
{
	const UAdventureCampaignDefinition* Campaign = GetCampaign();
	const UAdventureLevelDefinition* Level = Campaign ? Campaign->GetLevel(LevelIndex) : nullptr;
	return Level && ActiveSave && ActiveSave->WonLevels.Contains(Level->PortalId);
}

int32 UAdventureSubsystem::GetNumLevelsWon() const
{
	// Counted against the campaign, so ids of levels that were removed from the data do not count.
	const UAdventureCampaignDefinition* Campaign = GetCampaign();
	int32 NumWon = 0;
	for (int32 LevelIndex = 0; Campaign && LevelIndex < Campaign->GetNumLevels(); ++LevelIndex)
	{
		NumWon += IsLevelWon(LevelIndex) ? 1 : 0;
	}
	return NumWon;
}

bool UAdventureSubsystem::IsLevelUnlocked(int32 LevelIndex) const
{
	const UAdventureCampaignDefinition* Campaign = GetCampaign();
	if (!Campaign || !Campaign->GetLevel(LevelIndex))
	{
		return false;
	}
	if (CVarAdventureUnlockAll.GetValueOnGameThread() != 0)
	{
		return true;
	}
	// One after the other through the whole campaign: this also keeps a section's first level closed until
	// the section before is finished, which is the gate rule.
	return LevelIndex == 0 || IsLevelWon(LevelIndex - 1);
}

bool UAdventureSubsystem::IsSectionOpen(int32 SectionIndex) const
{
	const UAdventureCampaignDefinition* Campaign = GetCampaign();
	if (!Campaign || !Campaign->Sections.IsValidIndex(SectionIndex))
	{
		return false;
	}
	if (SectionIndex == 0 || CVarAdventureUnlockAll.GetValueOnGameThread() != 0)
	{
		return true;
	}
	for (int32 LevelIndex = Campaign->GetFirstLevelOfSection(SectionIndex - 1); LevelIndex < Campaign->GetFirstLevelOfSection(SectionIndex); ++LevelIndex)
	{
		if (!IsLevelWon(LevelIndex))
		{
			return false;
		}
	}
	// A section before this one that is still closed keeps this one closed too.
	return IsSectionOpen(SectionIndex - 1);
}

bool UAdventureSubsystem::IsIntroSeen() const
{
	return ActiveSave && ActiveSave->bIntroSeen;
}

void UAdventureSubsystem::MarkIntroSeen()
{
	if (ActiveSave && !ActiveSave->bIntroSeen)
	{
		ActiveSave->bIntroSeen = true;
		SaveActiveGame();
	}
}

bool UAdventureSubsystem::IsSectionIntroSeen(int32 SectionIndex) const
{
	return ActiveSave && ActiveSave->SeenSectionIntros.Contains(SectionIndex);
}

void UAdventureSubsystem::MarkSectionIntroSeen(int32 SectionIndex)
{
	if (ActiveSave && !ActiveSave->SeenSectionIntros.Contains(SectionIndex))
	{
		ActiveSave->SeenSectionIntros.Add(SectionIndex);
		SaveActiveGame();
	}
}

void UAdventureSubsystem::ResetProgress()
{
	if (!ActiveSave)
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("ADVENTURE reset: no active game"));
		return;
	}
	ActiveSave->WonLevels.Reset();
	ActiveSave->SeenSectionIntros.Reset();
	ActiveSave->bIntroSeen = false;
	LastOutcome = FAdventureRaceOutcome();
	SaveActiveGame();
	UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE progress of slot %d reset"), ActiveSlot);
}

bool UAdventureSubsystem::StartLevel(const UObject* WorldContext, int32 LevelIndex)
{
	const UAdventureCampaignDefinition* Campaign = GetCampaign();
	const UAdventureLevelDefinition* Level = Campaign ? Campaign->GetLevel(LevelIndex) : nullptr;
	UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(WorldContext);
	if (!ActiveSave || !Level || !Flow)
	{
		UE_LOG(LogCosmicRace, Warning, TEXT("ADVENTURE start level %d: no active game, no campaign or no such level"), LevelIndex);
		return false;
	}
	if (!IsLevelUnlocked(LevelIndex))
	{
		UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE level %d (%s) is locked"), LevelIndex, *Level->PortalId.ToString());
		return false;
	}

	FRaceSessionConfig Config;
	Config.HumanCharacters.Add(Campaign->PlayerCharacter);
	Config.NumAIOpponents = FMath::Clamp(Level->NumRivals, 0, UCosmicHotshotsSettings::MaxGridSize - 1);
	Config.AIDifficulty = Level->AIDifficulty;
	Config.NumLaps = Level->NumLaps;
	Config.Track = Level->Track;

	// Rivals: the level's own list first (never the player's character, never twice), the rest drawn now.
	for (const UCharacterDefinition* Rival : Level->Rivals)
	{
		if (Rival && Rival != Campaign->PlayerCharacter && !Config.AICharacters.Contains(Rival) && Config.AICharacters.Num() < Config.NumAIOpponents)
		{
			Config.AICharacters.Add(Rival);
		}
	}
	TArray<const UCharacterDefinition*> Taken;
	Taken.Add(Campaign->PlayerCharacter);
	for (const UCharacterDefinition* Rival : Config.AICharacters)
	{
		Taken.Add(Rival);
	}
	int32 Seed = 0;
	for (const UCharacterDefinition* Drawn : UGameFlowSubsystem::DrawRivalCharacters(Config.NumAIOpponents - Config.AICharacters.Num(), Taken, Seed))
	{
		Config.AICharacters.Add(Drawn);
	}
	Config.AIDriverSeed = Seed;

	FString RivalNames;
	for (const UCharacterDefinition* Rival : Config.AICharacters)
	{
		RivalNames += (RivalNames.IsEmpty() ? TEXT("") : TEXT(",")) + GetNameSafe(Rival);
	}
	UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE start level=%d id=%s player=%s rivals=%d laps=%d difficulty=%d track=%s seed=%d ai=[%s]"),
		LevelIndex, *Level->PortalId.ToString(), *GetNameSafe(Campaign->PlayerCharacter), Config.NumAIOpponents, Config.NumLaps,
		Config.AIDifficulty, *GetNameSafe(Config.Track), Seed, *RivalNames);

	CurrentLevelIndex = LevelIndex;
	Flow->SetRaceConfig(Config);
	Flow->StartRace(WorldContext);
	return true;
}

void UAdventureSubsystem::HandleRaceFinished(const FRaceResultData& Results)
{
	const UAdventureCampaignDefinition* Campaign = GetCampaign();
	const UAdventureLevelDefinition* Level = Campaign ? Campaign->GetLevel(CurrentLevelIndex) : nullptr;
	if (!ActiveSave || !Level)
	{
		return;
	}

	// The player's own line of the results. Winning = finished in 1st place.
	const FRaceResultEntry* PlayerEntry = Results.Entries.FindByPredicate([](const FRaceResultEntry& Entry) { return Entry.bHuman; });
	int32 Place = PlayerEntry ? PlayerEntry->Place : 0;
	bool bFinished = PlayerEntry && PlayerEntry->bFinished;
#if !UE_BUILD_SHIPPING
	if (const int32 ForcedPlace = CVarAdventureForcePlace.GetValueOnGameThread(); ForcedPlace > 0)
	{
		Place = ForcedPlace;
		bFinished = true;
	}
#endif

	LastOutcome = FAdventureRaceOutcome();
	LastOutcome.LevelIndex = CurrentLevelIndex;
	LastOutcome.Place = Place;
	LastOutcome.bWon = bFinished && Place == 1;

	if (!LastOutcome.bWon)
	{
		UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE level %d (%s) not won: place %d, finished=%d. Nothing unlocked."),
			CurrentLevelIndex, *Level->PortalId.ToString(), Place, bFinished ? 1 : 0);
		return;
	}

	const bool bFirstWin = !ActiveSave->WonLevels.Contains(Level->PortalId);
	const int32 NextLevel = CurrentLevelIndex + 1;
	const int32 NextSection = Campaign->GetSectionOfLevel(NextLevel);
	const bool bNextSectionWasOpen = NextSection != INDEX_NONE && IsSectionOpen(NextSection);
	if (bFirstWin)
	{
		ActiveSave->WonLevels.Add(Level->PortalId);
		SaveActiveGame();
		if (Campaign->GetLevel(NextLevel))
		{
			LastOutcome.UnlockedLevelIndex = NextLevel;
			if (!bNextSectionWasOpen && IsSectionOpen(NextSection))
			{
				LastOutcome.OpenedSectionIndex = NextSection;
			}
		}
	}

	UE_LOG(LogCosmicRace, Log, TEXT("ADVENTURE level %d (%s) WON%s: unlocked level %d, opened section %d, %d/%d levels won"),
		CurrentLevelIndex, *Level->PortalId.ToString(), bFirstWin ? TEXT("") : TEXT(" again"),
		LastOutcome.UnlockedLevelIndex, LastOutcome.OpenedSectionIndex, GetNumLevelsWon(), Campaign->GetNumLevels());
}

FAdventureRaceOutcome UAdventureSubsystem::ConsumeLastOutcome()
{
	const FAdventureRaceOutcome Outcome = LastOutcome;
	LastOutcome = FAdventureRaceOutcome();
	return Outcome;
}
