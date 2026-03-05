#include "GlitchAegisLibrary.h"
#include "GlitchAegisSettings.h"
#include "GlitchAegisSubsystem.h"
#include "GlitchSDK.h"
#include "Engine/GameInstance.h"

// ---------------------------------------------------------------------------
// Internal helper: retrieve the active install_id for the current session.
//
// Mirrors the resolution logic in GlitchAegisSubsystem::Initialize() so that
// standalone Blueprint function library calls (which have no subsystem
// reference) still find the correct ID.
//
// Priority:
//   1. -install_id= command-line argument  (Glitch launcher, all builds)
//   2. Settings->TestInstallId             (non-Shipping only)
// ---------------------------------------------------------------------------
static FString GetEffectiveInstallId()
{
	FString FromCmdLine;
	if (FParse::Value(FCommandLine::Get(), TEXT("install_id="), FromCmdLine) && !FromCmdLine.IsEmpty())
	{
		return FromCmdLine;
	}

#if !UE_BUILD_SHIPPING
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	if (!Settings->TestInstallId.IsEmpty())
	{
		return Settings->TestInstallId;
	}
#endif

	return FString();
}

// ============================================================================
// DRM / License
// ============================================================================

void UGlitchAegisLibrary::ValidateLicenseAsync(FOnLicenseValidated OnComplete)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	const FString InstallId = GetEffectiveInstallId();

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty() || InstallId.IsEmpty())
	{
		// No active Glitch session — fire the delegate immediately as denied.
		OnComplete.ExecuteIfBound(false, TEXT(""));
		return;
	}

	GlitchSDK::ValidateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		InstallId,
		FOnGlitchResponse::CreateLambda(
			[OnComplete](bool bSuccess, const FString& Body)
			{
				FString UserName;
				if (bSuccess)
				{
					const FString Key = TEXT("\"user_name\":\"");
					int32 Start = Body.Find(Key);
					if (Start != INDEX_NONE)
					{
						Start += Key.Len();
						int32 End = Body.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
						if (End != INDEX_NONE)
						{
							UserName = Body.Mid(Start, End - Start);
						}
					}
				}
				OnComplete.ExecuteIfBound(bSuccess, UserName);
			}
		)
	);
}

bool UGlitchAegisLibrary::ValidateLicense(FString& OutUserName)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	const FString InstallId = GetEffectiveInstallId();

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty() || InstallId.IsEmpty())
	{
		OutUserName = TEXT("");
		return false;
	}

	// Fires the request — result is logged but NOT surfaced back to Blueprint.
	// Migrate to ValidateLicenseAsync to actually act on 403 responses.
	GlitchSDK::ValidateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		InstallId,
		FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
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

	OutUserName = TEXT("");
	return true; // true = request dispatched, NOT license valid
}

// ============================================================================
// Telemetry / Behavioral Events
// ============================================================================

void UGlitchAegisLibrary::RecordGameEvent(FString Step, FString Action, FString MetadataJSON)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty()) return;

	GlitchSDK::FGameEventData Event;
	Event.GameInstallID  = GetEffectiveInstallId();
	Event.StepKey        = Step;
	Event.ActionKey      = Action;
	Event.MetadataJSON   = MetadataJSON;
	Event.EventTimestamp = FDateTime::UtcNow().ToIso8601();

	GlitchSDK::RecordEvent(
		Settings->TitleToken,
		Settings->TitleId,
		Event,
		FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: RecordGameEvent failed: %s"), *Body);
			}
		})
	);
}

void UGlitchAegisLibrary::RecordGameEventsBulk(const TArray<FGlitchEventData>& Events)
{
	if (Events.Num() == 0) return;

	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty()) return;

	TArray<GlitchSDK::FGameEventData> SdkEvents;
	SdkEvents.Reserve(Events.Num());

	const FString Timestamp = FDateTime::UtcNow().ToIso8601();

	for (const FGlitchEventData& E : Events)
	{
		GlitchSDK::FGameEventData Ev;
		Ev.GameInstallID  = E.GameInstallId;
		Ev.StepKey        = E.StepKey;
		Ev.ActionKey      = E.ActionKey;
		Ev.MetadataJSON   = E.MetadataJSON;
		Ev.EventTimestamp = Timestamp;
		SdkEvents.Add(Ev);
	}

	GlitchSDK::RecordEventsBulk(
		Settings->TitleToken,
		Settings->TitleId,
		SdkEvents,
		FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: RecordGameEventsBulk failed: %s"), *Body);
			}
		})
	);
}

// ============================================================================
// Cloud Save
// ============================================================================

void UGlitchAegisLibrary::SaveToCloud(FGlitchSaveData SaveData)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	const FString InstallId = GetEffectiveInstallId();

	if (InstallId.IsEmpty() || Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty())
	{
		return;
	}

	if (SaveData.Checksum.IsEmpty() || SaveData.Checksum == TEXT("auto-generated"))
	{
		UE_LOG(LogTemp, Error,
			TEXT("GlitchAegis: SaveToCloud requires a valid SHA-256 checksum in SaveData.Checksum. "
			     "Compute it from the raw (pre-base64) save bytes before calling this function."));
		return;
	}

	GlitchSDK::FGameSaveData Data;
	Data.SlotIndex       = SaveData.SlotIndex;
	Data.PayloadBase64   = SaveData.PayloadBase64;
	Data.Checksum        = SaveData.Checksum;
	Data.SaveType        = SaveData.SaveType;
	Data.ClientTimestamp = FDateTime::UtcNow().ToIso8601();

	GlitchSDK::StoreSave(
		Settings->TitleToken,
		Settings->TitleId,
		InstallId,
		Data,
		FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("GlitchAegis: SaveToCloud succeeded."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: SaveToCloud failed: %s"), *Body);
			}
		})
	);
}

// ============================================================================
// Purchases / Revenue
// ============================================================================

void UGlitchAegisLibrary::RecordPurchase(FGlitchPurchaseData PurchaseData)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty()) return;

	GlitchSDK::FPurchaseData D;
	D.GameInstallID  = PurchaseData.GameInstallId;
	D.PurchaseType   = PurchaseData.PurchaseType;
	D.PurchaseAmount = PurchaseData.PurchaseAmount;
	D.Currency       = PurchaseData.Currency;
	D.TransactionID  = PurchaseData.TransactionId;
	D.ItemSKU        = PurchaseData.ItemSku;
	D.ItemName       = PurchaseData.ItemName;
	D.Quantity       = PurchaseData.Quantity;
	D.MetadataJSON   = PurchaseData.MetadataJSON;

	GlitchSDK::RecordPurchase(
		Settings->TitleToken,
		Settings->TitleId,
		D,
		FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("GlitchAegis: RecordPurchase succeeded."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: RecordPurchase failed: %s"), *Body);
			}
		})
	);
}

// ============================================================================
// Install management — Voiding
// ============================================================================

void UGlitchAegisLibrary::VoidInstall(FString InstallUuid, bool bVoid, FOnGlitchResult OnComplete)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty() || InstallUuid.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TEXT("Missing TitleToken, TitleId, or InstallUuid"));
		return;
	}

	GlitchSDK::VoidInstall(
		Settings->TitleToken,
		Settings->TitleId,
		InstallUuid,
		bVoid,
		FOnGlitchResponse::CreateLambda(
			[OnComplete](bool bSuccess, const FString& Body)
			{
				OnComplete.ExecuteIfBound(bSuccess, Body);
			}
		)
	);
}

// ============================================================================
// Fingerprinting — Blueprint-accessible install with full fingerprint
// ============================================================================

void UGlitchAegisLibrary::SendFingerprintedInstall(FOnGlitchResult OnComplete)
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	const FString InstallId = GetEffectiveInstallId();

	if (InstallId.IsEmpty() || Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TEXT("Missing install_id, TitleToken, or TitleId"));
		return;
	}

	GlitchSDK::FInstallData D;
	D.UserInstallId  = InstallId;
	D.Platform       = TEXT("pc");
	D.DeviceType     = TEXT("desktop");
	D.GameVersion    = Settings->GameVersion;
	D.ReferralSource = Settings->ReferralSource;

	GlitchSDK::FFingerprintComponents FP = GlitchSDK::CollectSystemFingerprint();

	GlitchSDK::CreateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		D,
		FOnGlitchResponse::CreateLambda(
			[OnComplete](bool bSuccess, const FString& Body)
			{
				OnComplete.ExecuteIfBound(bSuccess, Body);
			}
		),
		&FP
	);
}
