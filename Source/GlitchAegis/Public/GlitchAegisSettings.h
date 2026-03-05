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
	 *
	 * Note: if bRequireValidation is true, the heartbeat also retrieves the
	 * install_id needed for validation — disabling this means you must call
	 * StartHeartbeatLoop() manually before validation can succeed.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Heartbeat")
	bool bEnableAutomaticHeartbeat = true;

	/**
	 * How often (seconds) to ping the retention/payout endpoint.
	 * Doc2 retention recommendation: 30s for accurate session tracking.
	 * Doc1 payout heartbeat: also acceptable at 60s.  Default: 30.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Heartbeat", meta=(ClampMin="10", ClampMax="120", EditCondition="bEnableAutomaticHeartbeat"))
	float HeartbeatIntervalSeconds = 30.0f;

	// -----------------------------------------------------------------------
	// DRM / Validation
	// -----------------------------------------------------------------------

	/**
	 * When true, the plugin validates the player's Glitch license on startup.
	 *
	 * If validation fails (server returns 403 / not authenticated), or if no
	 * install_id is present AND this flag is true, the subsystem broadcasts
	 * OnDrmValidated(false) and — if bShowErrorScreenOnValidationFailure is
	 * also true — displays a fullscreen error widget that blocks gameplay.
	 *
	 * Leave false during development unless you are testing DRM behaviour.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Validation")
	bool bRequireValidation = false;

	/**
	 * When true, a built-in fullscreen error widget is displayed whenever
	 * DRM validation returns false OR when no install_id exists and
	 * bRequireValidation is true.  The player cannot dismiss the widget —
	 * they must relaunch from the Glitch launcher to obtain a valid session.
	 *
	 * Only active when bRequireValidation is also true.
	 * In Development / Editor builds the widget shows a "Test Mode" banner
	 * so you can distinguish it from a real failure.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Validation",
		meta=(EditCondition="bRequireValidation"))
	bool bShowErrorScreenOnValidationFailure = true;

	// -----------------------------------------------------------------------
	// Development / Testing
	// -----------------------------------------------------------------------

	/**
	 * A test install_id to use during development / editor sessions when the
	 * game is NOT launched via the Glitch launcher (so no real -install_id=
	 * command-line argument is present).
	 *
	 * Rules:
	 *  - Only used in Development and DebugGame build configurations, and
	 *    when running inside the editor (WITH_EDITOR).
	 *  - Never injected in Shipping builds regardless of this value.
	 *  - If bEnableAutomaticHeartbeat is true and this is non-empty, the
	 *    subsystem will use it as the install_id for heartbeat + validation.
	 *  - If bRequireValidation is true and the test ID is set, the subsystem
	 *    runs a real validation call against the server using this ID so you
	 *    can verify your DRM integration without the launcher.
	 *
	 * Obtain a test install_id from the Glitch dashboard or QA tools.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Development",
		meta=(DisplayName="Test Install ID (Dev/Editor Only)"))
	FString TestInstallId;

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
