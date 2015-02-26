// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

namespace UnrealBuildTool.Rules
{
	public class FMODStudioEditor : ModuleRules
	{
		public FMODStudioEditor(TargetInfo Target)
		{
			bFasterWithoutUnity = true;

			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"FMODStudioEditor/Private",
					"FMODStudio/Private",
					"FMODStudio/Private/FMOD",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"FMODStudio"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"Slate",
					"SlateCore",
					"InputCore",
					"Settings",
					"EditorStyle",
					"LevelEditor",
					"AssetTools",
					"AssetRegistry",
					"PropertyEditor",
					"WorkspaceMenuStructure"
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);
		}
	}
}