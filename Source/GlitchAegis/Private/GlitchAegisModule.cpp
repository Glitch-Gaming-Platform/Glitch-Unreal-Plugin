#include "GlitchAegis.h"
#include "Modules/ModuleManager.h"

void FGlitchAegisModule::StartupModule()
{
	// Nothing to initialise at module load time.
	// All plugin work is driven by UGlitchAegisSubsystem (a GameInstance subsystem)
	// which Unreal instantiates automatically when a game session starts.
}

void FGlitchAegisModule::ShutdownModule()
{
	// Nothing to clean up.
}

IMPLEMENT_MODULE(FGlitchAegisModule, GlitchAegis)
