#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GlitchAegisLibrary.generated.h"

USTRUCT(BlueprintType)
struct FGlitchSaveData {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) int32 SlotIndex = 0;
    UPROPERTY(BlueprintReadWrite) FString PayloadBase64;
    UPROPERTY(BlueprintReadWrite) FString SaveType = "manual";
};

UCLASS()
class GLITCHAEGIS_API UGlitchAegisLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    /** The Aegis Handshake: Call this on BeginPlay to verify the license */
	UFUNCTION(BlueprintCallable, Category = "Glitch|Security")
	static bool ValidateLicense(FString& OutUserName);

    /** Record a custom in-game event (Telemetry) */
    UFUNCTION(BlueprintCallable, Category = "Glitch|Telemetry")
    static void RecordGameEvent(FString Step, FString Action, FString MetadataJSON);

    /** Save game progress to the Glitch Cloud */
    UFUNCTION(BlueprintCallable, Category = "Glitch|CloudSave")
    static void SaveToCloud(FGlitchSaveData SaveData);
};