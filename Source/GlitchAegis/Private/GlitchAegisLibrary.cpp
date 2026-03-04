#include "GlitchAegisLibrary.h"
#include "GlitchAegisSettings.h"
#include "GlitchSDK.h"

bool UGlitchAegisLibrary::ValidateLicense(FString& OutUserName)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	FString InstallId;
	FParse::Value(FCommandLine::Get(), TEXT("install_id="), InstallId);

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty() || InstallId.IsEmpty())
	{
		OutUserName = TEXT("");
		return false;
	}

	// NOTE: ValidateLicense is a Blueprint-callable convenience wrapper.
	// Because HTTP is async, this function fires the request and returns true
	// to indicate the request was *dispatched* successfully.
	// For a real DRM gate, bind the delegate below and gate gameplay in the callback.
	bool bDispatched = false;

	GlitchSDK::ValidateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		InstallId,
		GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("GlitchAegis: License validated."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: License validation failed: %s"), *Body);
			}
		})
	);

	bDispatched = true;
	OutUserName = TEXT(""); // Populated once the async response arrives — extend if needed
	return bDispatched;
}

void UGlitchAegisLibrary::RecordGameEvent(FString Step, FString Action, FString MetadataJSON)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	FString InstallId;
	FParse::Value(FCommandLine::Get(), TEXT("install_id="), InstallId);

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty())
	{
		return;
	}

	GlitchSDK::FGameEventData Event;
	Event.GameInstallID = InstallId;
	Event.StepKey       = Step;
	Event.ActionKey     = Action;
	Event.MetadataJSON  = MetadataJSON;
	Event.EventTimestamp = FDateTime::UtcNow().ToIso8601();

	GlitchSDK::RecordEvent(
		Settings->TitleToken,
		Settings->TitleId,
		Event,
		GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: RecordEvent failed: %s"), *Body);
			}
		})
	);
}

void UGlitchAegisLibrary::SaveToCloud(FGlitchSaveData SaveData)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	FString InstallId;
	FParse::Value(FCommandLine::Get(), TEXT("install_id="), InstallId);

	if (InstallId.IsEmpty() || Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty())
	{
		return;
	}

	GlitchSDK::FGameSaveData Data;
	Data.SlotIndex       = SaveData.SlotIndex;
	Data.PayloadBase64   = SaveData.PayloadBase64;
	Data.SaveType        = SaveData.SaveType;
	Data.ClientTimestamp = FDateTime::UtcNow().ToIso8601();
	Data.Checksum        = TEXT("auto-generated"); // Developer should supply a real SHA-256 checksum

	GlitchSDK::StoreSave(
		Settings->TitleToken,
		Settings->TitleId,
		InstallId,
		Data,
		GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: SaveToCloud failed: %s"), *Body);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("GlitchAegis: SaveToCloud succeeded."));
			}
		})
	);
}