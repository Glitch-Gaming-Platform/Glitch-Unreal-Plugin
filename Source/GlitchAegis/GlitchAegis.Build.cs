using UnrealBuildTool;

public class GlitchAegis : ModuleRules
{
	public GlitchAegis(ReadOnlyTargetRules Target) : base(Target)
	{
		// Explicit PCH mode: each .cpp manages its own includes.
		// Required for correct compilation with UHT-generated headers.
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Disable unity builds so every file compiles independently.
		// This prevents hidden include-order bugs and gives clean error messages.
		bUseUnity = false;

		// Use C++17 explicitly for UE5.0-5.2 compatibility.
		// UE5.3+ defaults to C++20; this setting is forward-compatible.
		CppStandard = CppStandardVersion.Cpp17;

		// Suppress MSVC strict conformance warnings that come from UE5.3+ defaults.
		// bEnableUndefinedIdentifierWarnings was deprecated in UE5.5 and replaced
		// with UndefinedIdentifierWarningLevel. We use a version check so the plugin
		// compiles cleanly on UE4, UE5.0-5.4, and UE5.5+ without any warnings or errors.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
#if UE_5_5_OR_LATER
			UndefinedIdentifierWarningLevel = WarningLevel.Off;
#else
			bEnableUndefinedIdentifierWarnings = false;
#endif
		}

		// Modules available in all UE5 versions
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"HTTP",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
		});
	}
}
