#include "GlitchAegisSubsystem.h"
#include "GlitchAegisSettings.h"
#include "GlitchSDK.h"
#include "Engine/Engine.h"

// ---------------------------------------------------------------------------
// Resolve the effective install_id for this session.
//
// Priority:
//   1. -install_id=<UUID> command-line arg  (Glitch launcher, all builds)
//   2. Settings->TestInstallId              (Dev/Editor only, never Shipping)
//
// Returns empty string if neither source provides a value.
// ---------------------------------------------------------------------------
static FString ResolveInstallId(const UGlitchAegisSettings* Settings, bool& bOutIsTestId)
{
	bOutIsTestId = false;

	// 1. Command-line arg — present when launched via the Glitch launcher.
	FString FromCmdLine;
	if (FParse::Value(FCommandLine::Get(), TEXT("install_id="), FromCmdLine) && !FromCmdLine.IsEmpty())
	{
		return FromCmdLine;
	}

	// 2. Test install_id — only in non-Shipping builds.
#if !UE_BUILD_SHIPPING
	if (!Settings->TestInstallId.IsEmpty())
	{
		bOutIsTestId = true;
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: No -install_id= on command line. Using TestInstallId from Project Settings. "
			     "This ID will NOT be used in Shipping builds."));
		return Settings->TestInstallId;
	}
#endif

	return FString();
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	// Resolve the install_id (launcher arg > test ID > empty).
	CachedInstallId      = ResolveInstallId(Settings, bUsingTestInstallId);
	const bool bHasId    = !CachedInstallId.IsEmpty();

	// Guard: credentials must be set for any network activity.
	const bool bHasCreds = !Settings->TitleToken.IsEmpty() && !Settings->TitleId.IsEmpty();
	if (!bHasCreds)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: TitleToken or TitleId not configured in Project Settings > Glitch Aegis. "
			     "Heartbeat and validation disabled."));

		// If validation is required but credentials are missing, still fail fast.
		if (Settings->bRequireValidation)
		{
			HandleValidationResult(false, TEXT(""));
		}
		return;
	}

	// -----------------------------------------------------------------------
	// Case: no install_id and validation is required → fail immediately.
	// -----------------------------------------------------------------------
	if (!bHasId && Settings->bRequireValidation)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: bRequireValidation is true but no install_id is present "
			     "(not launched via Glitch launcher and no TestInstallId configured). "
			     "Treating as validation failure."));
		HandleValidationResult(false, TEXT(""));
		return;
	}

	// -----------------------------------------------------------------------
	// Heartbeat block: only runs when install_id is present AND enabled.
	// -----------------------------------------------------------------------
	if (bHasId && Settings->bEnableAutomaticHeartbeat)
	{
		// Step 1 — Register install / session-start with full attribution data.
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

		// Step 2 — Start the payout / retention heartbeat timer.
		StartHeartbeatLoop();
	}
	else if (!Settings->bEnableAutomaticHeartbeat)
	{
		UE_LOG(LogTemp, Log,
			TEXT("GlitchAegis: Automatic heartbeat is disabled. Call StartHeartbeatLoop() manually when ready."));
	}

	// -----------------------------------------------------------------------
	// Validation block: runs when required, OR when we have an ID regardless.
	// -----------------------------------------------------------------------
	if (bHasId && Settings->bRequireValidation)
	{
		RunDrmValidation();
	}
	else if (bHasId && !Settings->bRequireValidation)
	{
		// Not required — run silently and broadcast the result for anyone who
		// is listening, but never show the error screen.
		RunDrmValidation();
	}
}

// ---------------------------------------------------------------------------
// Deinitialize
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::Deinitialize()
{
	StopHeartbeatLoop();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// StartHeartbeatLoop / StopHeartbeatLoop
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::StartHeartbeatLoop()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: StartHeartbeatLoop — GetWorld() returned null. Heartbeat not started."));
		return;
	}

	if (CachedInstallId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: StartHeartbeatLoop — no install_id available. Heartbeat not started."));
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

#if !UE_BUILD_SHIPPING
	if (bUsingTestInstallId)
	{
		UE_LOG(LogTemp, Log,
			TEXT("GlitchAegis: Heartbeat loop started (%.0fs interval) using TEST install_id."), Interval);
	}
	else
#endif
	{
		UE_LOG(LogTemp, Log, TEXT("GlitchAegis: Heartbeat loop started (%.0fs interval)."), Interval);
	}
}

void UGlitchAegisSubsystem::StopHeartbeatLoop()
{
	UWorld* World = GetWorld();
	if (World && HeartbeatTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("GlitchAegis: Heartbeat loop stopped."));
	}
}

// ---------------------------------------------------------------------------
// OnHeartbeatTimerTick
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// RunValidation (public — manual trigger from Blueprint / C++)
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::RunValidation()
{
	if (CachedInstallId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GlitchAegis: RunValidation called but no install_id is available."));
		HandleValidationResult(false, TEXT(""));
		return;
	}
	RunDrmValidation();
}

// ---------------------------------------------------------------------------
// RunDrmValidation (private)
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::RunDrmValidation()
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	TWeakObjectPtr<UGlitchAegisSubsystem> WeakThis(this);

	GlitchSDK::ValidateInstall(
		Settings->TitleToken,
		Settings->TitleId,
		CachedInstallId,
		FOnGlitchResponse::CreateLambda(
			[WeakThis](bool bSuccess, const FString& Body)
			{
				UGlitchAegisSubsystem* Self = WeakThis.Get();
				if (!Self) return;

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

				Self->HandleValidationResult(bSuccess, UserName);
			}
		)
	);
}

// ---------------------------------------------------------------------------
// HandleValidationResult — central dispatch after any validation outcome
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::HandleValidationResult(bool bIsValid, const FString& UserName)
{
	// Always broadcast so gameplay code can react.
	OnDrmValidated.Broadcast(bIsValid, UserName);

	if (!bIsValid)
	{
		const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
		if (Settings->bRequireValidation && Settings->bShowErrorScreenOnValidationFailure)
		{
			ShowErrorScreen();
		}
	}
}

// ---------------------------------------------------------------------------
// ShowErrorScreen
//
// Creates a simple fullscreen UMG widget that blocks all input.
// The widget is built entirely in C++ so no asset dependency is required —
// the plugin works out-of-the-box without needing a Blueprint widget.
//
// In non-Shipping builds a yellow "TEST MODE" banner is appended to the
// message so developers know this is a simulated failure.
// ---------------------------------------------------------------------------
void UGlitchAegisSubsystem::ShowErrorScreen()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UWorld* World = GI->GetWorld();
	if (!World) return;

	// Retrieve or create a PlayerController to own the widget.
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	// -----------------------------------------------------------------
	// Build a plain UUserWidget subclass-less widget backed by a Canvas.
	// We cannot create a real UMG Blueprint from C++ at runtime without an
	// asset, so we use GEngine->AddOnScreenDebugMessage as a guaranteed
	// always-visible overlay, supplemented by setting input mode to UI-only
	// so the player cannot interact with the game.
	// -----------------------------------------------------------------

	// Lock input — the player can no longer control the game.
	FInputModeUIOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	// Stop the heartbeat — no point pinging when the session is invalid.
	StopHeartbeatLoop();

	// Persistent on-screen message (visible until the process is killed).
	// Key = 0 means it will never auto-expire.
	const bool bIsTestId = bUsingTestInstallId;

	FString Message = TEXT(
		"ACCESS DENIED\n\n"
		"This game could not verify your Glitch license.\n"
		"Please relaunch the game through the Glitch launcher\n"
		"to obtain a valid session.\n\n"
		"If you believe this is an error, visit glitch.fun/support"
	);

#if !UE_BUILD_SHIPPING
	Message += TEXT("\n\n[TEST MODE — bRequireValidation is enabled in Project Settings]");
	if (bIsTestId)
	{
		Message += TEXT("\n[Using TestInstallId — validation was run against the test install record]");
	}
#endif

	if (GEngine)
	{
		// Duration = 0 → permanent until overwritten or process exit.
		// Color: bold red for shipping, orange for dev/test.
#if UE_BUILD_SHIPPING
		const FColor MsgColor = FColor::Red;
#else
		const FColor MsgColor = FColor::Orange;
#endif
		GEngine->AddOnScreenDebugMessage(
			/*Key=*/     static_cast<uint64>(-1),
			/*Duration=*/1e9f,   // ~31 years — effectively permanent
			MsgColor,
			Message,
			/*bNewerOnTop=*/true,
			/*TextScale=*/FVector2D(1.5f, 1.5f)
		);
	}

	UE_LOG(LogTemp, Error,
		TEXT("GlitchAegis: DRM validation failed. Error screen displayed. "
		     "Player input locked. Message:\n%s"), *Message);
}
