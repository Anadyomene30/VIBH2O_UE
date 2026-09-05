using UnrealBuildTool;

public class VibH2O : ModuleRules
{
	public VibH2O(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// UVibH2OSettings derives from UDeveloperSettings in a public
			// header, so the dependency must be public, not private.
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking"
		});
	}
}
