#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "GlitchAegisSubsystem.generated.h"

UCLASS()
class GLITCHAEGIS_API UGlitchAegisSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(BlueprintCallable, Category = "Glitch|Aegis")
	void StartHeartbeatLoop();

private:
	void OnHeartbeatTimerTick();
	FTimerHandle HeartbeatTimerHandle;
	FString CachedInstallId;
};
