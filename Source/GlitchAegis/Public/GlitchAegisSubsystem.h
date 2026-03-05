#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "GlitchAegisSubsystem.generated.h"

/**
 * Called when the automatic startup DRM check completes.
 * bIsValid = true  → license OK (200), player may proceed
 * bIsValid = false → denied (403), no install_id, or validation required but missing
 * UserName         → player's Glitch username if valid
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAegisDrmResult, bool, bIsValid, FString, UserName);

/**
 * UGlitchAegisSubsystem
 *
 * Auto-starts on game launch. Reads install_id from -install_id=UUID on the
 * command line (injected by the Glitch launcher).  In Development / Editor
 * builds a TestInstallId from Project Settings is used as a fallback when no
 * command-line argument is present.
 *
 * Feature flags (all in Edit > Project Settings > Glitch Aegis):
 *
 *   bEnableAutomaticHeartbeat        — Toggle the auto heartbeat + install
 *                                      registration loop on or off.
 *
 *   bRequireValidation               — When true, validation is always run.
 *                                      Missing install_id counts as a failure.
 *
 *   bShowErrorScreenOnValidationFailure — When true (and bRequireValidation),
 *                                      a blocking fullscreen error widget is
 *                                      shown on validation failure.
 *
 *   TestInstallId                    — Used in Dev/Editor when no real
 *                                      -install_id= argument is present.
 */
UCLASS()
class GLITCHAEGIS_API UGlitchAegisSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -----------------------------------------------------------------------
	// Events
	// -----------------------------------------------------------------------

	/**
	 * Fires after DRM validation completes (automatic or manual).
	 * Bind in your GameMode or Level Blueprint to gate gameplay.
	 *
	 * bIsValid = false is also broadcast when bRequireValidation is true and
	 * no install_id is available (no launcher, no TestInstallId configured).
	 *
	 * Example:
	 *   GlitchSubsystem->OnDrmValidated.AddDynamic(this, &AMyGameMode::HandleDrmResult);
	 */
	UPROPERTY(BlueprintAssignable, Category = "Glitch|Security")
	FOnAegisDrmResult OnDrmValidated;

	// -----------------------------------------------------------------------
	// Manual control
	// -----------------------------------------------------------------------

	/**
	 * Starts the payout / retention heartbeat loop.
	 * Called automatically if bEnableAutomaticHeartbeat is true.
	 * Call manually if you disabled auto-start and want to begin the loop
	 * at a specific point (e.g. after your own DRM gate passes).
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Aegis")
	void StartHeartbeatLoop();

	/**
	 * Stops a running heartbeat loop.
	 * Useful if you want to pause heartbeats during loading screens or
	 * paused-game states where the session should not count as active time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Aegis")
	void StopHeartbeatLoop();

	/**
	 * Manually trigger a DRM validation check.
	 * Result is broadcast through OnDrmValidated.
	 * Useful if you disabled bRequireValidation but still want to check
	 * from a specific point in your game flow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Security")
	void RunValidation();

	/** Returns the install_id in use for this session (launcher arg or test ID). */
	UFUNCTION(BlueprintPure, Category = "Glitch|Aegis")
	FString GetInstallId() const { return CachedInstallId; }

	/**
	 * Returns true if the install_id came from the test fallback
	 * (TestInstallId setting) rather than from the Glitch launcher.
	 * Always returns false in Shipping builds.
	 */
	UFUNCTION(BlueprintPure, Category = "Glitch|Aegis")
	bool IsUsingTestInstallId() const { return bUsingTestInstallId; }

private:
	void OnHeartbeatTimerTick();
	void RunDrmValidation();
	void HandleValidationResult(bool bIsValid, const FString& UserName);
	void ShowErrorScreen();

	FTimerHandle HeartbeatTimerHandle;
	FString      CachedInstallId;
	bool         bUsingTestInstallId = false;
};
