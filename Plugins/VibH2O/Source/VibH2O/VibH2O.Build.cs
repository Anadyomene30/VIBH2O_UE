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
			// FKey appears in the public cue types.
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking",
			// The cue director plays Level Sequences. Also used by the
			// non-shipping VibH2O.DemoSweep console command.
			"LevelSequence",
			"MovieScene"
		});

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
