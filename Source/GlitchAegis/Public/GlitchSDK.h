#pragma once

#include "CoreMinimal.h"
// Note: Http.h is intentionally NOT included here — it lives in the HTTP private
// module and must only be included in .cpp files (GlitchSDK.cpp already does this).

/**
 * Glitch Gaming API Integration for Unreal Engine
 *
 * All HTTP calls are async and execute on the game thread via Unreal's FHttpModule.
 * No libcurl dependency. No blocking calls.
 */

namespace GlitchSDK
{
	// -------------------------------------------------------------------------
	// Data Structures
	// -------------------------------------------------------------------------

	/** Device fingerprint components for cross-platform user tracking */
	struct FFingerprintComponents
	{
		// Device
		FString DeviceModel;
		FString DeviceType;
		FString DeviceManufacturer;

		// OS
		FString OSName;
		FString OSVersion;

		// Display
		FString DisplayResolution;
		int32 DisplayDensity = 0;

		// Hardware
		FString CPUModel;
		int32 CPUCores = 0;
		FString GPUModel;
		int32 MemoryMB = 0;

		// Environment
		FString Language;
		FString Timezone;
		FString Region;

		// Desktop-specific
		TArray<FString> FormFactors;
		FString Architecture;
		FString Bitness;
		FString PlatformVersion;
		bool bIsWow64 = false;

		// Keyboard layout: key code -> character
		TMap<FString, FString> KeyboardLayout;

		// Advertising ID
		FString AdvertisingID;
	};

	/** Purchase / revenue data */
	struct FPurchaseData
	{
		FString GameInstallID;    // Required: UUID of existing install
		FString PurchaseType;     // "in_app", "ad_revenue", "crypto"
		float PurchaseAmount = 0.f;
		FString Currency;
		FString TransactionID;
		FString ItemSKU;
		FString ItemName;
		int32 Quantity = 1;
		FString MetadataJSON;
	};

	/** Cloud save slot data */
	struct FGameSaveData
	{
		int32 SlotIndex = 0;       // 0-99
		FString PayloadBase64;
		FString Checksum;          // SHA-256 of raw binary
		int32 BaseVersion = 0;
		FString SaveType = TEXT("manual");
		FString ClientTimestamp;   // ISO-8601
		FString MetadataJSON;
	};

	/** Behavioral telemetry event */
	struct FGameEventData
	{
		FString GameInstallID;
		FString StepKey;
		FString ActionKey;
		FString MetadataJSON;
		FString EventTimestamp;    // ISO-8601
	};

	// -------------------------------------------------------------------------
	// Delegate types — all HTTP calls are async; bind these to receive results
	// -------------------------------------------------------------------------

	DECLARE_DELEGATE_TwoParams(FOnGlitchResponse,
		bool  /*bSuccess*/,
		const FString& /*ResponseBody*/);

	// -------------------------------------------------------------------------
	// Core API Functions
	// -------------------------------------------------------------------------

	/** Verify license / DRM on startup */
	void ValidateInstall(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		FOnGlitchResponse OnComplete);

	/** Send a DRM heartbeat (call on a timer, NOT every tick) */
	void SendHeartbeat(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const FString& AnalyticsSessionId,
		FOnGlitchResponse OnComplete);

	/** Create a basic install record */
	void CreateInstallRecord(
		const FString& AuthToken,
		const FString& TitleId,
		const FString& UserInstallId,
		const FString& Platform,
		FOnGlitchResponse OnComplete);

	/** Create an install record with fingerprint data */
	void CreateInstallRecordWithFingerprint(
		const FString& AuthToken,
		const FString& TitleId,
		const FString& UserInstallId,
		const FString& Platform,
		const FFingerprintComponents& Fingerprint,
		const FString& GameVersion,
		const FString& ReferralSource,
		FOnGlitchResponse OnComplete);

	/** Record a purchase / revenue event */
	void RecordPurchase(
		const FString& AuthToken,
		const FString& TitleId,
		const FPurchaseData& PurchaseData,
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Cloud Save
	// -------------------------------------------------------------------------

	void ListSaves(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		FOnGlitchResponse OnComplete);

	void StoreSave(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const FGameSaveData& SaveData,
		FOnGlitchResponse OnComplete);

	void ResolveSaveConflict(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const FString& SaveId,
		const FString& ConflictId,
		const FString& Choice,   // "keep_server" or "use_client"
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Behavioral Telemetry
	// -------------------------------------------------------------------------

	void RecordEvent(
		const FString& TitleToken,
		const FString& TitleId,
		const FGameEventData& Event,
		FOnGlitchResponse OnComplete);

	void RecordEventsBulk(
		const FString& TitleToken,
		const FString& TitleId,
		const TArray<FGameEventData>& Events,
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Wishlist Intelligence
	// -------------------------------------------------------------------------

	void ToggleWishlist(
		const FString& UserJwt,
		const FString& TitleId,
		const FString& FingerprintId,
		FOnGlitchResponse OnComplete);

	void UpdateWishlistScore(
		const FString& UserJwt,
		const FString& TitleId,
		int32 Score,
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Utility
	// -------------------------------------------------------------------------

	/** Auto-collect system fingerprint using platform APIs */
	FFingerprintComponents CollectSystemFingerprint();

	/** Serialize fingerprint struct to a JSON object string */
	FString FingerprintToJSON(const FFingerprintComponents& Fingerprint);

	/** Serialize purchase struct to a JSON object string */
	FString PurchaseToJSON(const FPurchaseData& Purchase);

	// -------------------------------------------------------------------------
	// Internal helpers (used by .cpp only, exposed here for unit-testability)
	// -------------------------------------------------------------------------
	namespace Internal
	{
		FString EscapeJSON(const FString& Input);
		void PostJSON(const FString& Url, const FString& Token, const FString& Body, FOnGlitchResponse OnComplete);
		void GetRequest(const FString& Url, const FString& Token, FOnGlitchResponse OnComplete);
	}

} // namespace GlitchSDK