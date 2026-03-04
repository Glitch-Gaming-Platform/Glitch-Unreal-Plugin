#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "GlitchAegisSubsystem.generated.h"

/**
 * Called when the automatic startup DRM check completes.
 * bIsValid = true  → license OK (200), player may proceed
 * bIsValid = false → denied (403) or no Glitch session found
 * UserName         → player's Glitch username if valid
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAegisDrmResult, bool, bIsValid, FString, UserName);

/**
 * UGlitchAegisSubsystem
 *
 * Auto-starts on game launch. Reads install_id from -install_id=UUID on the
 * command line (injected by the Glitch launcher), registers the install with
 * the analytics backend, runs the DRM validation check, then starts the
 * payout heartbeat loop.
 *
 * Zero-config: just fill in TitleId + TitleToken in
 *   Edit > Project Settings > Glitch Aegis
 * and set bEnableAutomaticHeartbeat = true.
 */
UCLASS()
class GLITCHAEGIS_API UGlitchAegisSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// -----------------------------------------------------------------------
	// Events
	// -----------------------------------------------------------------------

	/**
	 * Fires after the automatic DRM validation completes on startup.
	 * Bind to this in your GameMode or first Level Blueprint to gate gameplay.
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

	/** Returns the install_id injected by the Glitch launcher, or empty string */
	UFUNCTION(BlueprintPure, Category = "Glitch|Aegis")
	FString GetInstallId() const { return CachedInstallId; }

private:
	void OnHeartbeatTimerTick();
	void RunDrmValidation();

	FTimerHandle HeartbeatTimerHandle;
	FString CachedInstallId;
};