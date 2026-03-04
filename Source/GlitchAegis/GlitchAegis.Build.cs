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
		// Our code is conformant; this prevents noisy warnings from engine headers.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			bEnableUndefinedIdentifierWarnings = false;
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
