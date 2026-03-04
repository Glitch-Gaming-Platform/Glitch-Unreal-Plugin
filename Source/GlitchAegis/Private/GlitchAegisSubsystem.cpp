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
	{
		return;
	}

	// -----------------------------------------------------------------------
	// Step 1: Register the install / session-start with full attribution data.
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
			FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
			{
				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("GlitchAegis: Install/session registered."));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: Install registration failed: %s"), *Body);
				}
			}),
			&FP
		);
	}

	// -----------------------------------------------------------------------
	// Step 2: DRM validation.
	// -----------------------------------------------------------------------
	RunDrmValidation();

	// -----------------------------------------------------------------------
	// Step 3: Start payout / retention heartbeat loop.
	// -----------------------------------------------------------------------
	StartHeartbeatLoop();
}

void UGlitchAegisSubsystem::Deinitialize()
{
	// Clear the timer before the world and timer manager are torn down.
	// Without this, the timer manager may fire into a garbage-collected object.
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	}

	Super::Deinitialize();
}

void UGlitchAegisSubsystem::RunDrmValidation()
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	// Capture a weak pointer so the lambda doesn't access a destroyed subsystem
	// if the HTTP response arrives after the game instance has shut down.
	TWeakObjectPtr<UGlitchAegisSubsystem> WeakThis(this);

	GlitchSDK::ValidateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		CachedInstallId,
		FOnGlitchResponse::CreateLambda(
			[WeakThis](bool bSuccess, const FString& Body)
			{
				UGlitchAegisSubsystem* Self = WeakThis.Get();
				if (!Self)
				{
					return; // Subsystem was destroyed before response arrived
				}

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
					UE_LOG(LogTemp, Log, TEXT("GlitchAegis: License valid. User: %s"), *UserName);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: License validation failed: %s"), *Body);
				}

				Self->OnDrmValidated.Broadcast(bSuccess, UserName);
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
	{
		return;
	}

	GlitchSDK::SendHeartbeat(
		Settings->TitleToken,
		Settings->TitleId,
		CachedInstallId,
		TEXT(""),
		FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: Heartbeat failed: %s"), *Body);
			}
		})
	);
}
