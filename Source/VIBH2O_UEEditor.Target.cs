using UnrealBuildTool;

public class VIBH2O_UEEditorTarget : TargetRules
{
	public VIBH2O_UEEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		// "Latest" rather than a pinned version: this project must compile as-is
		// on both 5.5 and 5.8. Every include in the plugin is explicit, so the
		// engine's include order has no bearing on it.
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("VIBH2O_UE");
	}
}
