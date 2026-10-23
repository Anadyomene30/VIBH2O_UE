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
			"Networking",
			// The iPad remote: Json builds its state snapshot, Projects finds
			// the plugin folder its page is served from.
			"Json",
			"Projects",
			// Used only by the non-shipping VibH2O.DemoSweep console command,
			// which plays the demo Level Sequence as part of its visual pass.
			"LevelSequence",
			"MovieScene"
		});

		// The remote's page ships as loose files next to the packaged game,
		// not inside the pak: it can then be edited on site, in a text editor,
		// and reloaded with VibH2O.Restart - no repackaging.
		RuntimeDependencies.Add("$(PluginDir)/Resources/Remote/...", StagedFileType.NonUFS);
	}
}
