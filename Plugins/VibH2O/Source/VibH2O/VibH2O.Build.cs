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
			"DeveloperSettings",
			// FKey appears in public headers: AVibH2OBackdrop's toggle key and the cue types.
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking",
			// The iPad remote: Json builds its state snapshot, Projects finds
			// the plugin folder its page is served from.
			"Json",
			"Projects",
			// The cue director plays Level Sequences. Also used by the
			// non-shipping VibH2O.DemoSweep console command.
			"LevelSequence",
			"MovieScene",
			// The control desk: the camera rail it frames, and the JSON of its presets.
			"CinematicCamera",
			"JsonUtilities"
		});

		// The remote's page ships as loose files next to the packaged game,
		// not inside the pak: it can then be edited on site, in a text editor,
		// and reloaded with VibH2O.Restart - no repackaging.
		RuntimeDependencies.Add("$(PluginDir)/Resources/Remote/...", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(PluginDir)/Resources/RegiePresets/...", StagedFileType.NonUFS);

		// Editor builds only: the cue director's learn step records an undo
		// transaction, and the background guard lifts the editor's throttle
		// during Play. Every use is behind WITH_EDITOR; the Game target never
		// sees this module.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// MIDI input for the cues goes through WinMM, the system MIDI layer.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("winmm.lib");
		}
	}
}
