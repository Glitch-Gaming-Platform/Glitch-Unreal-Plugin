#pragma once

#include "CoreMinimal.h"
// Http.h intentionally excluded from this public header — it lives in the HTTP
// private module. GlitchSDK.cpp includes HttpModule.h / IHttpRequest.h directly.

/**
 * Glitch Gaming API — Unreal Engine Integration
 *
 * All HTTP calls are async and fire on the game thread via FHttpModule.
 * No libcurl dependency. No blocking calls.
 *
 * IMPORTANT: DECLARE_DELEGATE macros MUST live at global scope (outside any
 * namespace). FOnGlitchResponse is declared here at file scope, then the
 * namespace begins below.
 */

// ---------------------------------------------------------------------------
// Delegate — declared at GLOBAL scope (Unreal requirement)
// ---------------------------------------------------------------------------

DECLARE_DELEGATE_TwoParams(FOnGlitchResponse,
	bool         /*bSuccess*/,
	const FString& /*ResponseBody*/);

// ---------------------------------------------------------------------------
// Everything else lives in the GlitchSDK namespace
// ---------------------------------------------------------------------------

namespace GlitchSDK
{
	// -------------------------------------------------------------------------
	// Data Structures
	// -------------------------------------------------------------------------

	/** Device fingerprint for cross-device attribution matching */
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
		int32   DisplayDensity = 0;

		// Hardware
		FString CPUModel;
		int32   CPUCores = 0;
		FString GPUModel;
		int32   MemoryMB = 0;

		// Environment
		FString Language;
		FString Timezone;
		FString Region;

		// Desktop-specific
		TArray<FString>      FormFactors;
		FString              Architecture;
		FString              Bitness;
		FString              PlatformVersion;
		bool                 bIsWow64 = false;
		TMap<FString,FString> KeyboardLayout;   // key code -> character

		FString AdvertisingID;
	};

	/** Full install / session registration data */
	struct FInstallData
	{
		FString UserInstallId;   // Required: unique ID for this player/session
		FString Platform;        // "pc", "steam", "windows", etc.
		FString DeviceType;      // "desktop", "mobile"
		FString GameVersion;     // e.g. "1.2.3"
		FString ReferralSource;  // "advertising", "social_media", "organic"

		// UTM attribution (optional)
		FString UtmSource;
		FString UtmMedium;
		FString UtmCampaign;
		FString UtmTerm;
		FString UtmContent;

		// Optional extras
		FString DeviceModel;
		FString OSName;
		FString OSVersion;
		FString Language;
		FString Region;
		FString Timezone;
		FString AdvertisingId;
	};

	/** Purchase / revenue event */
	struct FPurchaseData
	{
		FString GameInstallID;    // Required: UUID of existing install record
		FString PurchaseType;     // "in_app", "ad_revenue", "crypto"
		float   PurchaseAmount = 0.f;
		FString Currency;
		FString TransactionID;    // Use to prevent duplicate records
		FString ItemSKU;
		FString ItemName;
		int32   Quantity = 1;
		FString MetadataJSON;
	};

	/** Cloud save slot */
	struct FGameSaveData
	{
		int32   SlotIndex = 0;    // 0-99
		FString PayloadBase64;
		FString Checksum;         // SHA-256 hex of raw (pre-base64) bytes
		int32   BaseVersion = 0;
		FString SaveType = TEXT("manual");
		FString ClientTimestamp;  // ISO-8601
		FString MetadataJSON;
	};

	/** Behavioral telemetry event */
	struct FGameEventData
	{
		FString GameInstallID;
		FString StepKey;
		FString ActionKey;
		FString MetadataJSON;
		FString EventTimestamp;   // ISO-8601
	};

	// -------------------------------------------------------------------------
	// Core API — DRM / Session
	// -------------------------------------------------------------------------

	/** Verify that the player has a valid Glitch license. Call on startup. */
	void ValidateInstall(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		FOnGlitchResponse OnComplete);

	/** Payout / retention heartbeat. Call on a timer, NOT every tick. */
	void SendHeartbeat(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const FString& AnalyticsSessionId,
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Core API — Install Registration
	// -------------------------------------------------------------------------

	/**
	 * Register an install/session with full attribution data.
	 * Optionally pass a Fingerprint pointer for cross-device attribution.
	 * Pass nullptr for Fingerprint to skip fingerprinting.
	 */
	void CreateInstall(
		const FString&            TitleToken,
		const FString&            TitleId,
		const FInstallData&       Data,
		FOnGlitchResponse         OnComplete,
		const FFingerprintComponents* Fingerprint = nullptr);

	/** Legacy helper — basic install with no attribution data */
	void CreateInstallRecord(
		const FString& AuthToken,
		const FString& TitleId,
		const FString& UserInstallId,
		const FString& Platform,
		FOnGlitchResponse OnComplete);

	/** Legacy helper — install with fingerprint (use CreateInstall instead) */
	void CreateInstallRecordWithFingerprint(
		const FString&            AuthToken,
		const FString&            TitleId,
		const FString&            UserInstallId,
		const FString&            Platform,
		const FFingerprintComponents& Fingerprint,
		const FString&            GameVersion,
		const FString&            ReferralSource,
		FOnGlitchResponse         OnComplete);

	/**
	 * Mark an install record as void (or restore it).
	 * InstallUuid is the database UUID returned by the /installs endpoint.
	 * bVoid=true to void, false to restore.
	 */
	void VoidInstall(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallUuid,
		bool           bVoid,
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
		const FString&      TitleToken,
		const FString&      TitleId,
		const FString&      InstallId,
		const FGameSaveData& SaveData,
		FOnGlitchResponse   OnComplete);

	void ResolveSaveConflict(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const FString& SaveId,
		const FString& ConflictId,
		const FString& Choice,     // "keep_server" or "use_client"
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Behavioral Telemetry
	// -------------------------------------------------------------------------

	void RecordEvent(
		const FString&       TitleToken,
		const FString&       TitleId,
		const FGameEventData& Event,
		FOnGlitchResponse    OnComplete);

	void RecordEventsBulk(
		const FString&              TitleToken,
		const FString&              TitleId,
		const TArray<FGameEventData>& Events,
		FOnGlitchResponse           OnComplete);

	// -------------------------------------------------------------------------
	// Revenue
	// -------------------------------------------------------------------------

	void RecordPurchase(
		const FString&      AuthToken,
		const FString&      TitleId,
		const FPurchaseData& PurchaseData,
		FOnGlitchResponse   OnComplete);

	// -------------------------------------------------------------------------
	// Wishlist
	// -------------------------------------------------------------------------

	void ToggleWishlist(
		const FString& UserJwt,
		const FString& TitleId,
		const FString& FingerprintId,
		FOnGlitchResponse OnComplete);

	void UpdateWishlistScore(
		const FString& UserJwt,
		const FString& TitleId,
		int32          Score,
		FOnGlitchResponse OnComplete);

	// -------------------------------------------------------------------------
	// Fingerprinting
	// -------------------------------------------------------------------------

	/** Collect system fingerprint using platform APIs (Windows/Mac/Linux) */
	FFingerprintComponents CollectSystemFingerprint();

	/** Serialize fingerprint struct to JSON object string */
	FString FingerprintToJSON(const FFingerprintComponents& Fingerprint);

	/** Serialize install data struct to JSON object string */
	FString InstallDataToJSON(const FInstallData& Data, const FFingerprintComponents* Fingerprint);

	/** Serialize purchase struct to JSON object string */
	FString PurchaseToJSON(const FPurchaseData& Purchase);

	// -------------------------------------------------------------------------
	// Internal helpers (in .cpp, exposed for testability)
	// -------------------------------------------------------------------------
	namespace Internal
	{
		FString EscapeJSON(const FString& Input);
		void PostJSON(const FString& Url, const FString& Token, const FString& Body, FOnGlitchResponse OnComplete);
		void PatchJSON(const FString& Url, const FString& Token, const FString& Body, FOnGlitchResponse OnComplete);
		void GetRequest(const FString& Url, const FString& Token, FOnGlitchResponse OnComplete);
	}

	// -------------------------------------------------------------------------
	// Progression System (Achievements + Leaderboards)
	// -------------------------------------------------------------------------

	/**
	 * Submits a "Run" to the progression system.
	 * This single endpoint handles BOTH achievement unlocks AND leaderboard submissions.
	 * Stats: achievement api_key -> value (server checks thresholds)
	 * Scores: leaderboard api_key -> score value
	 */
	void SubmitProgressionRun(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const TMap<FString, float>& Stats,
		const TMap<FString, float>& Scores,
		const TMap<FString, FString>& Metadata,
		FOnGlitchResponse OnComplete);

	/** Download the player's achievement list (locked/unlocked/in-progress) */
	void GetPlayerAchievements(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		FOnGlitchResponse OnComplete);

	/** Download leaderboard entries for a given board */
	void GetLeaderboard(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& BoardApiKey,
		FOnGlitchResponse OnComplete);

} // namespace GlitchSDK
