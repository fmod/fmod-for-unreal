// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

namespace UnrealBuildTool.Rules
{
	public class FMODStudioOculus : ModuleRules
	{
		public FMODStudioOculus(TargetInfo Target)
		{
			// For some reason this flag blows out the size of the compiled lib from under 50MB to over 100MB!
			//bFasterWithoutUnity = true;

			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"FMODStudioOcculus/Private",
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

			// ModuleDirectory points to FMODStudio\source\FMODStudio, need to get back to lib
			string BasePath = System.IO.Path.Combine(ModuleDirectory, "../../Lib", Target.Platform.ToString());

			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Win32:
					PublicAdditionalLibraries.Add(System.IO.Path.Combine(BasePath, "ovrfmod32.lib"));
					PublicDelayLoadDLLs.Add("ovrfmod32.dll");
					Definitions.Add("FMOD_OSP_SUPPORTED=1");
					break;
				case UnrealTargetPlatform.Win64:
					PublicAdditionalLibraries.Add(System.IO.Path.Combine(BasePath, "ovrfmod64.lib"));
					PublicDelayLoadDLLs.Add("ovrfmod64.dll");
					Definitions.Add("FMOD_OSP_SUPPORTED=1");
					break;
				default:
					break;
			}

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"Slate",
						"SlateCore",
						"Settings",
					}
					);
			}
		}
	}
}