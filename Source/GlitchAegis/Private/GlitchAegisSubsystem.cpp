#include "GlitchAegisSubsystem.h"
#include "GlitchAegisSettings.h"
#include "GlitchSDK.h"
#include "Kismet/KismetSystemLibrary.h"

void UGlitchAegisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	
	// 1. Try to find install_id from command line (passed by Glitch Launcher)
	// Format: -install_id=UUID
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
	GetWorld()->GetTimerManager().SetTimer(HeartbeatTimerHandle, this, &UGlitchAegisSubsystem::OnHeartbeatTimerTick, 60.0f, true, 1.0f);
}

void UGlitchAegisSubsystem::OnHeartbeatTimerTick()
{
	const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
	
	// Call the SDK SendHeartbeat
	GlitchSDK::SendHeartbeat(
		TCHAR_TO_UTF8(*Settings->TitleToken),
		TCHAR_TO_UTF8(*Settings->TitleId),
		TCHAR_TO_UTF8(*CachedInstallId)
	);
}