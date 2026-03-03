#include "GlitchAegisLibrary.h"
#include "GlitchAegisSettings.h"
#include "GlitchSDK.h"

bool UGlitchAegisLibrary::ValidateLicense(FString& OutUserName) {
    const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
    FString InstallId;
    FParse::Value(FCommandLine::Get(), TEXT("install_id="), InstallId);

    std::string Result = GlitchSDK::ValidateInstall(
        TCHAR_TO_UTF8(*Settings->TitleToken),
        TCHAR_TO_UTF8(*Settings->TitleId),
        TCHAR_TO_UTF8(*InstallId)
    );

    // Simple check: if result contains "valid":true
    if (Result.find("\"valid\":true") != std::string::npos) {
        return true;
    }
    return false;
}

void UGlitchAegisLibrary::RecordGameEvent(FString Step, FString Action, FString MetadataJSON) {
    const UGlitchAegisSettings* Settings = GetDefault<UGlitchAegisSettings>();
    FString InstallId;
    FParse::Value(FCommandLine::Get(), TEXT("install_id="), InstallId);

    GlitchSDK::GameEventData Event;
    Event.GameInstallID = TCHAR_TO_UTF8(*InstallId);
    Event.StepKey = TCHAR_TO_UTF8(*Step);
    Event.ActionKey = TCHAR_TO_UTF8(*Action);
    Event.MetadataJSON = TCHAR_TO_UTF8(*MetadataJSON);

    GlitchSDK::RecordEvent(TCHAR_TO_UTF8(*Settings->TitleToken), TCHAR_TO_UTF8(*Settings->TitleId), Event);
}