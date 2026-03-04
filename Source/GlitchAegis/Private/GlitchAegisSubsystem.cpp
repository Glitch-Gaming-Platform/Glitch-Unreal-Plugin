#include "GlitchAegisSubsystem.h"
#include "GlitchAegisSettings.h"
#include "GlitchSDK.h"

void UGlitchAegisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	// Read install_id from the command line.
	// The Glitch launcher injects: -install_id=<UUID>
	if (!FParse::Value(FCommandLine::Get(), TEXT("install_id="), CachedInstallId))
	{
		// Not launched via Glitch — no payout session. Silent no-op.
		return;
	}

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: TitleToken or TitleId not configured in Project Settings > Glitch Aegis. Heartbeat and DRM disabled."));
		return;
	}

	if (!Settings->bEnableAutomaticHeartbeat)
		return;

	// -----------------------------------------------------------------------
	// Step 1: Register the install / session-start with full attribution data.
	// Sends device fingerprint for cross-device matching (Doc2 Fingerprinting).
	// -----------------------------------------------------------------------
	{
		GlitchSDK::FInstallData D;
		D.UserInstallId  = CachedInstallId;
		D.Platform       = TEXT("pc");
		D.DeviceType     = TEXT("desktop");
		D.GameVersion    = Settings->GameVersion;
		D.ReferralSource = Settings->ReferralSource;

		GlitchSDK::FFingerprintComponents FP = GlitchSDK::CollectSystemFingerprint();

		GlitchSDK::CreateInstall(
			Settings->TitleToken,
			Settings->TitleId,
			D,
			GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
			{
				if (bSuccess)
					UE_LOG(LogTemp, Log, TEXT("GlitchAegis: Install/session registered."));
				else
					UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: Install registration failed: %s"), *Body);
			}),
			&FP
		);
	}

	// -----------------------------------------------------------------------
	// Step 2: DRM validation — Doc1 Step 4.
	// "On the very first frame of your game, call the validation server.
	//  This ensures the player has a legitimate, active license before they
	//  ever see your Main Menu."
	// The result fires OnDrmValidated so Blueprint/GameMode can gate gameplay.
	// -----------------------------------------------------------------------
	RunDrmValidation();

	// -----------------------------------------------------------------------
	// Step 3: Start payout / retention heartbeat loop.
	// -----------------------------------------------------------------------
	StartHeartbeatLoop();
}

void UGlitchAegisSubsystem::RunDrmValidation()
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	// Capture delegate so we can fire it on the game thread from the lambda
	FOnAegisDrmResult DrmDelegate = OnDrmValidated;

	GlitchSDK::ValidateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		CachedInstallId,
		GlitchSDK::FOnGlitchResponse::CreateLambda(
			[DrmDelegate](bool bSuccess, const FString& Body)
			{
				// Parse user_name from response if present
				FString UserName;
				if (bSuccess)
				{
					// Simple substring parse — avoid pulling in a JSON library dependency
					const FString Key = TEXT("\"user_name\":\"");
					int32 Start = Body.Find(Key);
					if (Start != INDEX_NONE)
					{
						Start += Key.Len();
						int32 End = Body.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
						if (End != INDEX_NONE)
							UserName = Body.Mid(Start, End - Start);
					}
					UE_LOG(LogTemp, Log, TEXT("GlitchAegis: License valid. User: %s"), *UserName);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: License validation failed (403 / no session): %s"), *Body);
				}

				DrmDelegate.Broadcast(bSuccess, UserName);
			}
		)
	);
}

void UGlitchAegisSubsystem::StartHeartbeatLoop()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: StartHeartbeatLoop — GetWorld() returned null. Heartbeat not started."));
		return;
	}

	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	const float Interval = FMath::Clamp(Settings->HeartbeatIntervalSeconds, 10.0f, 120.0f);

	// First tick fires after one full interval — the init call above covers t=0.
	World->GetTimerManager().SetTimer(
		HeartbeatTimerHandle,
		this,
		&UGlitchAegisSubsystem::OnHeartbeatTimerTick,
		Interval,
		/*bLoop=*/true,
		/*FirstDelay=*/Interval
	);

	UE_LOG(LogTemp, Log, TEXT("GlitchAegis: Heartbeat loop started (%.0fs interval)."), Interval);
}

void UGlitchAegisSubsystem::OnHeartbeatTimerTick()
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty() || CachedInstallId.IsEmpty())
		return;

	// Heartbeat reuses the /installs endpoint.
	// The backend detects the recurring user_install_id and logs a GameRetention event
	// rather than a new install, then credits the payout timer.
	GlitchSDK::SendHeartbeat(
		Settings->TitleToken,
		Settings->TitleId,
		CachedInstallId,
		/*AnalyticsSessionId=*/TEXT(""),   // idle-detection is handled by the Glitch web wrapper
		GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: Heartbeat failed: %s"), *Body);
		})
	);
}