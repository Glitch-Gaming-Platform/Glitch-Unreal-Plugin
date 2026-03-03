#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GlitchAegisSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Glitch Aegis"))
class GLITCHAEGIS_API UGlitchAegisSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "General")
	FString TitleId;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta=(PasswordField=true))
	FString TitleToken;

	UPROPERTY(Config, EditAnywhere, Category = "Automation")
	bool bEnableAutomaticHeartbeat = true;
};
