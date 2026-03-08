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

		// UE4 / UE5.0-5.2 default to C++17.  UE5.3+ default to C++20.
		// UE5.7 removed CppStandardVersion.Cpp17 entirely, so we must NOT
		// reference that symbol on 5.3+.  On older engines the explicit
		// setting is harmless (it matches their default).
#if !UE_5_3_OR_LATER
		CppStandard = CppStandardVersion.Cpp17;
#endif

		// Suppress MSVC undefined-identifier warnings fired by engine headers under
		// strict conformance mode. Epic has changed this API three times:
		//
		//   UE4 / UE5.0-5.4  ->  bEnableUndefinedIdentifierWarnings (bool)
		//   UE5.5             ->  UndefinedIdentifierWarningLevel (WarningLevel enum)
		//   UE5.6+            ->  CppCompileWarningSettings.UndefinedIdentifierWarningLevel
		//
		// UBT injects UE_5_5_OR_LATER / UE_5_6_OR_LATER as C# preprocessor symbols,
		// so exactly one branch is compiled and no deprecation warning is emitted on
		// any engine version from UE4 through UE5.6 and beyond.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
#if UE_5_6_OR_LATER
			CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
#elif UE_5_5_OR_LATER
			UndefinedIdentifierWarningLevel = WarningLevel.Off;
#else
			bEnableUndefinedIdentifierWarnings = false;
#endif
		}

		// Modules available in all UE versions
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
