#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GlitchAegisSettings.generated.h"

/**
 * Project Settings > Glitch Aegis
 *
 * Zero-config entry point. Fill in TitleId and TitleToken,
 * enable bEnableAutomaticHeartbeat, and the subsystem handles the rest.
 */
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Glitch Aegis"))
class GLITCHAEGIS_API UGlitchAegisSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	// Required
	// -----------------------------------------------------------------------

	/** Your game's Title ID — visible on the Technical Integration page */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	FString TitleId;

	/** Title Token generated on the Technical Integration page (keep secret) */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta=(PasswordField=true))
	FString TitleToken;

	// -----------------------------------------------------------------------
	// Heartbeat / Retention
	// -----------------------------------------------------------------------

	/**
	 * When true, the GlitchAegisSubsystem automatically starts the payout
	 * heartbeat loop AND calls the DRM validation endpoint on game launch.
	 * Disable only if you want to drive both manually from C++ or Blueprints.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Automation")
	bool bEnableAutomaticHeartbeat = true;

	/**
	 * How often (seconds) to ping the retention/payout endpoint.
	 * Doc2 retention recommendation: 30s for accurate session tracking.
	 * Doc1 payout heartbeat: also acceptable at 60s.  Default: 30.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Automation", meta=(ClampMin="10", ClampMax="120"))
	float HeartbeatIntervalSeconds = 30.0f;

	// -----------------------------------------------------------------------
	// Attribution (recommended — Doc2 Installation section)
	// -----------------------------------------------------------------------

	/**
	 * Current build version string, e.g. "1.2.3".
	 * Sent with every install/heartbeat so you can track retention across updates.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Attribution")
	FString GameVersion;

	/**
	 * How users discovered your game: "social_media", "advertising", "organic", etc.
	 * Used for install attribution reporting in the analytics dashboard.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Attribution")
	FString ReferralSource;
};