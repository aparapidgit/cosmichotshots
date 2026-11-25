#include "Player/PlayerSettingsSubsystem.h"

#include "CosmicHotshotsGame.h"
#include "Camera/CameraTuningData.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/RaceCameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Game/CosmicHotshotsSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Vehicle/RaceVehicle.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSettingsPersist(
		TEXT("ch.Settings.Persist"), 1,
		TEXT("0 = player settings are neither loaded nor saved (always the defaults). Read on the first use of the settings."), ECVF_Cheat);

	TAutoConsoleVariable<FString> CVarSettingsSlot(
		TEXT("ch.Settings.Slot"), TEXT("PlayerSettings"),
		TEXT("Name of the player settings save slot (the local player index is appended). Tests use their own name."), ECVF_Cheat);

	/** Save games are per platform user; the game has one. */
	constexpr int32 SettingsSaveUserIndex = 0;
}

UPlayerSettingsSubsystem* UPlayerSettingsSubsystem::Get(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UPlayerSettingsSubsystem>() : nullptr;
}

const UCameraTuningData* UPlayerSettingsSubsystem::GetCameraTuning()
{
	if (const UCameraTuningData* ProjectDefault = GetDefault<UCosmicHotshotsSettings>()->CameraTuning.LoadSynchronous())
	{
		return ProjectDefault;
	}
	return GetDefault<UCameraTuningData>();
}

bool UPlayerSettingsSubsystem::ShouldPersist() const
{
	return FApp::CanEverRender() && CVarSettingsPersist.GetValueOnGameThread() != 0;
}

FString UPlayerSettingsSubsystem::GetSlotName() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const int32 PlayerIndex = LocalPlayer ? FMath::Max(LocalPlayer->GetLocalPlayerIndex(), 0) : 0;
	return FString::Printf(TEXT("%s_%d"), *CVarSettingsSlot.GetValueOnGameThread(), PlayerIndex);
}

void UPlayerSettingsSubsystem::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;

	if (!ShouldPersist())
	{
		return;
	}
	const FString SlotName = GetSlotName();
	if (const UPlayerSettingsSave* Save = Cast<UPlayerSettingsSave>(UGameplayStatics::LoadGameFromSlot(SlotName, SettingsSaveUserIndex)))
	{
		Settings = Save->Settings;
		UE_LOG(LogCosmicRace, Log, TEXT("SETTINGS loaded %s: fov=%.0f shake=%d speedLines=%d"),
			*SlotName, Settings.BaseFov, static_cast<int32>(Settings.CameraShake), static_cast<int32>(Settings.SpeedLines));
	}
}

const FPlayerSettingsData& UPlayerSettingsSubsystem::GetSettings()
{
	EnsureLoaded();
	return Settings;
}

float UPlayerSettingsSubsystem::GetBaseFov()
{
	EnsureLoaded();
	const UCameraTuningData* Tuning = GetCameraTuning();
	const float BaseFov = Settings.BaseFov >= 0.f ? Settings.BaseFov : Tuning->FovNormal;
	return FMath::Clamp(BaseFov, Tuning->MinBaseFov, Tuning->MaxBaseFov);
}

void UPlayerSettingsSubsystem::SetBaseFov(float InBaseFov)
{
	EnsureLoaded();
	const UCameraTuningData* Tuning = GetCameraTuning();
	Settings.BaseFov = FMath::Clamp(InBaseFov, Tuning->MinBaseFov, Tuning->MaxBaseFov);
	HandleSettingsChanged();
}

void UPlayerSettingsSubsystem::SetCameraShake(EEffectIntensity Intensity)
{
	EnsureLoaded();
	Settings.CameraShake = Intensity;
	HandleSettingsChanged();
}

void UPlayerSettingsSubsystem::SetSpeedLines(EEffectIntensity Intensity)
{
	EnsureLoaded();
	Settings.SpeedLines = Intensity;
	HandleSettingsChanged();
}

float UPlayerSettingsSubsystem::ToScale(EEffectIntensity Intensity) const
{
	switch (Intensity)
	{
	case EEffectIntensity::Off:
		return 0.f;
	case EEffectIntensity::Low:
		return GetCameraTuning()->LowIntensityScale;
	default:
		return 1.f;
	}
}

void UPlayerSettingsSubsystem::ApplyToCamera(URaceCameraComponent* Camera)
{
	if (!Camera)
	{
		return;
	}
	EnsureLoaded();
	Camera->SetBaseFov(GetBaseFov());
	Camera->SetShakeScale(ToScale(Settings.CameraShake));
	Camera->SetSpeedLinesScale(ToScale(Settings.SpeedLines));
}

void UPlayerSettingsSubsystem::HandleSettingsChanged()
{
	if (ShouldPersist())
	{
		UPlayerSettingsSave* Save = Cast<UPlayerSettingsSave>(UGameplayStatics::CreateSaveGameObject(UPlayerSettingsSave::StaticClass()));
		Save->Settings = Settings;
		const FString SlotName = GetSlotName();
		const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName, SettingsSaveUserIndex);
		UE_LOG(LogCosmicRace, Log, TEXT("SETTINGS saved %s: fov=%.0f shake=%d speedLines=%d ok=%d"),
			*SlotName, Settings.BaseFov, static_cast<int32>(Settings.CameraShake), static_cast<int32>(Settings.SpeedLines), bSaved ? 1 : 0);
	}

	// Live preview: the vehicle this player drives right now.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	if (const ARaceVehicle* Vehicle = PlayerController ? Cast<ARaceVehicle>(PlayerController->GetPawn()) : nullptr)
	{
		ApplyToCamera(Vehicle->GetRaceCamera());

		// While the game is paused (Options from the pause menu) the view is not refreshed by itself: do it now.
		if (PlayerController->IsPaused() && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->UpdateCamera(0.f);
		}
	}
}
