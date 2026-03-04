using UnrealBuildTool;

public class GlitchAegis : ModuleRules
{
	public GlitchAegis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"HTTP",
				"Json",
				"JsonUtilities"
			}
		);
	}
}