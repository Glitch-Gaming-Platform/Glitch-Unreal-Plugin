#include "GlitchAegisSubsystem.h"
#include "GlitchAegisSettings.h"
#include "GlitchSDK.h"

void UGlitchAegisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	// Try to find install_id from command line, e.g.: -install_id=UUID
	if (FParse::Value(FCommandLine::Get(), TEXT("install_id="), CachedInstallId))
	{
		if (Settings->bEnableAutomaticHeartbeat)
		{
			StartHeartbeatLoop();
		}
	}
}

void UGlitchAegisSubsystem::StartHeartbeatLoop()
{
	// Guard: GetWorld() can be null during editor startup or subsystem teardown
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: StartHeartbeatLoop called but GetWorld() is null. Heartbeat not started."));
		return;
	}

	// Fire immediately after a 1-second grace period, then every 60 seconds.
	// The timer callback is async-safe: the HTTP call never blocks the game thread.
	World->GetTimerManager().SetTimer(
		HeartbeatTimerHandle,
		this,
		&UGlitchAegisSubsystem::OnHeartbeatTimerTick,
		60.0f,
		/*bLoop=*/true,
		/*FirstDelay=*/1.0f
	);
}

void UGlitchAegisSubsystem::OnHeartbeatTimerTick()
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();

	if (Settings->TitleToken.IsEmpty() || Settings->TitleId.IsEmpty() || CachedInstallId.IsEmpty())
	{
		return;
	}

	// Async — returns immediately; response handled in the lambda (game thread callback)
	GlitchSDK::SendHeartbeat(
		Settings->TitleToken,
		Settings->TitleId,
		CachedInstallId,
		/*AnalyticsSessionId=*/TEXT(""),
		GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("GlitchAegis: Heartbeat failed: %s"), *Body);
			}
		})
	);
}