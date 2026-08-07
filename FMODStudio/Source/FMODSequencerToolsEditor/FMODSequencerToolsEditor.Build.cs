using UnrealBuildTool;

public class FMODSequencerToolsEditor : ModuleRules
{
	public FMODSequencerToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "EditorSubsystem", "FMODStudio" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "UnrealEd", "AssetRegistry", "ContentBrowser", "ContentBrowserData", "Sequencer", "SequencerCore", "MovieScene", "MovieSceneTracks", "LevelSequence", "LevelSequenceEditor", "FMODStudioEditor", "InputCore" });
	}
}
