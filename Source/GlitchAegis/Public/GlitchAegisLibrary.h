#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GlitchAegisLibrary.generated.h"

// ============================================================================
// Blueprint-friendly structs
// ============================================================================

/** Data for a single cloud-save slot */
USTRUCT(BlueprintType)
struct FGlitchSaveData
{
	GENERATED_BODY()

	/** Slot index 0–99 */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|CloudSave")
	int32 SlotIndex = 0;

	/** Base64-encoded save payload */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|CloudSave")
	FString PayloadBase64;

	/**
	 * SHA-256 hex hash of the raw (pre-base64) save bytes.
	 * You must compute this yourself before calling SaveToCloud.
	 * The server uses it to detect corruption and resolve conflicts.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|CloudSave")
	FString Checksum;

	/** "manual" (player-triggered) or "auto" (checkpoint) */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|CloudSave")
	FString SaveType = TEXT("manual");
};

/** Data for a single purchase / revenue event */
USTRUCT(BlueprintType)
struct FGlitchPurchaseData
{
	GENERATED_BODY()

	/**
	 * UUID of the GameInstall record this purchase is tied to.
	 * This is the ID returned by CreateInstall / the active install_id.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString GameInstallId;

	/** "in_app", "ad_revenue", "crypto", etc. */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString PurchaseType;

	/** Monetary value, e.g. 4.99 */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	float PurchaseAmount = 0.f;

	/** ISO-4217 currency code, e.g. "USD" */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString Currency;

	/** Third-party transaction / receipt ID — used for de-duplication */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString TransactionId;

	/** SKU or product code, e.g. "starter_pack" */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString ItemSku;

	/** Human-readable product name, e.g. "Beginner Pack" */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString ItemName;

	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	int32 Quantity = 1;

	/** Optional JSON metadata string, e.g. "{\"promo_code\":\"TWITTER20\"}" */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Purchase")
	FString MetadataJSON;
};

/** A single behavioral telemetry event for use with bulk dispatch */
USTRUCT(BlueprintType)
struct FGlitchEventData
{
	GENERATED_BODY()

	/** The install UUID for the current player session */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Events")
	FString GameInstallId;

	/** Screen / stage the player is on, e.g. "onboarding", "level_1" */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Events")
	FString StepKey;

	/** What the player did, e.g. "tutorial_complete", "player_death" */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Events")
	FString ActionKey;

	/** Optional JSON metadata string */
	UPROPERTY(BlueprintReadWrite, Category = "Glitch|Events")
	FString MetadataJSON;
};

// ============================================================================
// Delegate types for async responses
// ============================================================================

/** Called when an async license validation completes.
 *  bIsValid = true means 200 OK (allow play); false means 403/error (deny play). */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnLicenseValidated, bool, bIsValid, FString, UserName);

/** Generic async result: bSuccess = HTTP 2xx, ResponseBody = raw JSON */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGlitchResult, bool, bSuccess, FString, ResponseBody);

// ============================================================================
// Blueprint Function Library
// ============================================================================

UCLASS()
class GLITCHAEGIS_API UGlitchAegisLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// DRM / License (Doc1 Step 4)
	// -----------------------------------------------------------------------

	/**
	 * Validates the player's license against the Glitch server.
	 *
	 * This is the correct async API. Bind OnComplete to receive the result:
	 *   - bIsValid = true  → license OK, allow gameplay
	 *   - bIsValid = false → license denied (403) or no session; stop the game
	 *   - UserName         → Glitch username if valid, empty otherwise
	 *
	 * Call this on BeginPlay of your first level before showing the Main Menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Security",
		meta=(DisplayName="Validate License (Async)"))
	static void ValidateLicenseAsync(FOnLicenseValidated OnComplete);

	/**
	 * @deprecated Use ValidateLicenseAsync instead.
	 * Returns true if the HTTP request was dispatched (NOT if the license is valid).
	 * The async result is discarded — the 403 "deny" case is silently ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Security",
		meta=(DisplayName="Validate License (Legacy)", DeprecatedFunction,
			  DeprecationMessage="Use ValidateLicenseAsync to actually act on the 403 deny response."))
	static bool ValidateLicense(FString& OutUserName);

	// -----------------------------------------------------------------------
	// Telemetry / Behavioral Events (Doc2 Events & Funnels)
	// -----------------------------------------------------------------------

	/**
	 * Records a single behavioral event.
	 * StepKey = the screen/stage (e.g. "onboarding")
	 * ActionKey = the action (e.g. "tutorial_complete")
	 * MetadataJSON = optional extra data as a JSON string, e.g. "{\"score\":100}"
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Telemetry",
		meta=(DisplayName="Record Game Event"))
	static void RecordGameEvent(FString Step, FString Action, FString MetadataJSON);

	/**
	 * Sends multiple events in a single HTTP request.
	 * Use on mobile or any context where minimising network calls matters.
	 * Doc2 explicitly recommends batching events every 1–2 minutes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Telemetry",
		meta=(DisplayName="Record Game Events (Bulk)"))
	static void RecordGameEventsBulk(const TArray<FGlitchEventData>& Events);

	// -----------------------------------------------------------------------
	// Cloud Save (Doc1 Step 3)
	// -----------------------------------------------------------------------

	/**
	 * Saves a game slot to Glitch Cloud.
	 *
	 * IMPORTANT: SaveData.Checksum must be a valid SHA-256 hex string of the
	 * raw (pre-base64) save bytes. Compute it before calling this function.
	 * The server uses it to detect corruption and to resolve sync conflicts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|CloudSave",
		meta=(DisplayName="Save To Cloud"))
	static void SaveToCloud(FGlitchSaveData SaveData);

	// -----------------------------------------------------------------------
	// Purchases / Revenue (Doc2 Purchases)
	// -----------------------------------------------------------------------

	/**
	 * Records a purchase or revenue event.
	 * PurchaseData.GameInstallId must match the active session's install UUID.
	 * Use TransactionId to prevent duplicate records if the webhook fires twice.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Revenue",
		meta=(DisplayName="Record Purchase"))
	static void RecordPurchase(FGlitchPurchaseData PurchaseData);

	// -----------------------------------------------------------------------
	// Install management (Doc2 Installation — Voiding)
	// -----------------------------------------------------------------------

	/**
	 * Marks an install record as void (fraudulent, duplicate, or immediately uninstalled).
	 * InstallUuid is the UUID returned by the /installs POST endpoint — NOT user_install_id.
	 * Pass bVoid=false to restore a previously voided record.
	 * OnComplete fires with the server response.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Install",
		meta=(DisplayName="Void Install Record"))
	static void VoidInstall(FString InstallUuid, bool bVoid, FOnGlitchResult OnComplete);

	// -----------------------------------------------------------------------
	// Fingerprinting helpers (Doc2 Cross-Platform Fingerprinting)
	// -----------------------------------------------------------------------

	/**
	 * Collects the system hardware fingerprint and sends it as part of a
	 * CreateInstall call. This improves cross-device attribution accuracy.
	 * Doc2: "providing keyboard layout data dramatically improves our ability
	 * to connect a user's web session to their game install."
	 *
	 * Call once on first launch. The subsystem calls this automatically
	 * if bEnableAutomaticHeartbeat is true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Fingerprinting",
		meta=(DisplayName="Send Fingerprinted Install"))
	static void SendFingerprintedInstall(FOnGlitchResult OnComplete);

	// -----------------------------------------------------------------------
	// Achievements
	// -----------------------------------------------------------------------

	/** Reports progress toward an achievement. If the threshold is met,
	 *  Glitch unlocks it automatically and the Aegis overlay shows a toast.
	 *  ApiKey: The nickname from the dashboard (e.g. "boss_killed")
	 *  Value: Progress amount. Use 1 for simple unlocks. */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Achievements",
		meta=(DisplayName="Report Achievement Progress"))
	static void ReportAchievementProgress(FString ApiKey, float Value = 1.0f);

	/** Check if an achievement is unlocked (uses local cache, instant). */
	UFUNCTION(BlueprintPure, Category = "Glitch|Achievements",
		meta=(DisplayName="Is Achievement Unlocked"))
	static bool IsAchievementUnlocked(FString ApiKey);

	/** Force-refresh the achievement cache from the server. */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Achievements",
		meta=(DisplayName="Refresh Achievements"))
	static void RefreshAchievements();

	// -----------------------------------------------------------------------
	// Leaderboards
	// -----------------------------------------------------------------------

	/** Submits a score to a leaderboard.
	 *  BoardApiKey: The API key from the dashboard (e.g. "high_score")
	 *  Score: The numeric value to submit */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Leaderboards",
		meta=(DisplayName="Submit Leaderboard Score"))
	static void SubmitLeaderboardScore(FString BoardApiKey, float Score);

	/** Downloads leaderboard entries. Result arrives via OnComplete callback. */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Leaderboards",
		meta=(DisplayName="Get Leaderboard (Async)"))
	static void GetLeaderboardAsync(FString BoardApiKey, FOnGlitchResult OnComplete);

	// -----------------------------------------------------------------------
	// Steam-to-Glitch Bridge
	// -----------------------------------------------------------------------

	/** Drop-in replacement for Steam's SetAchievement.
	 *  Buffers the unlock until FlushSteamBridge is called. */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Steam Bridge",
		meta=(DisplayName="Steam Bridge: Set Achievement"))
	static void SteamBridgeSetAchievement(FString AchievementApiName);

	/** Drop-in replacement for Steam's UploadLeaderboardScore.
	 *  Buffers the score until FlushSteamBridge is called. */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Steam Bridge",
		meta=(DisplayName="Steam Bridge: Upload Score"))
	static void SteamBridgeUploadScore(FString BoardApiKey, float Score);

	/** Drop-in replacement for Steam's StoreStats.
	 *  Flushes all buffered achievements and scores to Glitch. */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Steam Bridge",
		meta=(DisplayName="Steam Bridge: Store Stats"))
	static void SteamBridgeStoreStats();
};