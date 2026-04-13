#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GlitchAegisSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Glitch Aegis"))
class GLITCHAEGIS_API UGlitchAegisSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	// Required
	// -----------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "General")
	FString TitleId;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta=(PasswordField=true))
	FString TitleToken;

	// -----------------------------------------------------------------------
	// Heartbeat / Retention
	// -----------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Heartbeat")
	bool bEnableAutomaticHeartbeat = true;

	UPROPERTY(Config, EditAnywhere, Category = "Heartbeat", meta=(ClampMin="10", ClampMax="120", EditCondition="bEnableAutomaticHeartbeat"))
	float HeartbeatIntervalSeconds = 30.0f;

	// -----------------------------------------------------------------------
	// DRM / Validation
	// -----------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Validation")
	bool bRequireValidation = false;

	UPROPERTY(Config, EditAnywhere, Category = "Validation", meta=(EditCondition="bRequireValidation"))
	bool bShowErrorScreenOnValidationFailure = true;

	// -----------------------------------------------------------------------
	// Achievements
	// -----------------------------------------------------------------------

	/** When true, achievement data is loaded from Glitch on startup.
	 *  Use ReportAchievement() to unlock trophies defined on the dashboard. */
	UPROPERTY(Config, EditAnywhere, Category = "Achievements")
	bool bEnableAchievements = true;

	/** When true, achievements are auto-loaded on startup and cached locally. */
	UPROPERTY(Config, EditAnywhere, Category = "Achievements", meta=(EditCondition="bEnableAchievements"))
	bool bAutoLoadAchievements = true;

	// -----------------------------------------------------------------------
	// Leaderboards
	// -----------------------------------------------------------------------

	/** When true, SubmitScore() and GetLeaderboard() Blueprint nodes are active. */
	UPROPERTY(Config, EditAnywhere, Category = "Leaderboards")
	bool bEnableLeaderboards = true;

	// -----------------------------------------------------------------------
	// Cloud Saves
	// -----------------------------------------------------------------------

	/** When true, SaveToCloud() and LoadFromCloud() are available. */
	UPROPERTY(Config, EditAnywhere, Category = "Cloud Saves")
	bool bEnableCloudSaves = true;

	// -----------------------------------------------------------------------
	// Steam-to-Glitch Bridge
	// -----------------------------------------------------------------------

	/** When true, the GlitchSteamBridge functions are activated.
	 *  Use these as drop-in replacements for your existing Steam API calls.
	 *  Achievement/leaderboard API key names on Glitch must match Steam. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Bridge")
	bool bEnableSteamBridge = false;

	// -----------------------------------------------------------------------
	// Fingerprinting
	// -----------------------------------------------------------------------

	/** When true, system fingerprint data is collected and sent with the first
	 *  install registration. Improves cross-device attribution accuracy.
	 *  Disable this if you experience crashes on UE 4.26 or earlier. */
	UPROPERTY(Config, EditAnywhere, Category = "Fingerprinting")
	bool bEnableFingerprinting = true;

	// -----------------------------------------------------------------------
	// Development / Testing
	// -----------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Development",
		meta=(DisplayName="Test Install ID (Dev/Editor Only)"))
	FString TestInstallId;

	// -----------------------------------------------------------------------
	// Attribution
	// -----------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Attribution")
	FString GameVersion;

	UPROPERTY(Config, EditAnywhere, Category = "Attribution")
	FString ReferralSource;
};
