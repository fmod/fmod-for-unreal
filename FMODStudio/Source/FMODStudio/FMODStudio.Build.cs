// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

namespace UnrealBuildTool.Rules
{
	public class FMODStudio : ModuleRules
	{
		public FMODStudio(TargetInfo Target)
		{
			// Turn off unity builds to catch errors (although the engine seems to error when building for Android!)
			if (Target.Platform != UnrealTargetPlatform.Android)
			{
				bFasterWithoutUnity = true;
			}

			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"FMODStudio/Private",
					"FMODStudio/Public/FMOD",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				}
				);

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("AssetRegistry");
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);

			string configName = "";

			if (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				configName = "D";
				Definitions.Add("FMODSTUDIO_LINK_DEBUG=1");
				//System.Console.WriteLine("Linking FMOD Studio debug build");
			}
			else if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				configName = "L";
				Definitions.Add("FMODSTUDIO_LINK_LOGGING=1");
				//System.Console.WriteLine("Linking FMOD Studio logging build");
			}
			else
			{
				configName = "";
				Definitions.Add("FMODSTUDIO_LINK_RELEASE=1");
				//System.Console.WriteLine("Linking FMOD Studio release build");
			}

			string platformName = Target.Platform.ToString();

			string platformMidName = "";
			string linkExtension = "";
			string dllExtension = "";
			string libPrefix = "";

			// PublicAdditionalLibraries seems to load from the Engine/Source directory so make a path relative to there
			string BasePath = "../Plugins/FMODStudio/Lib/" + platformName + "/";

			bool copyThirdParty = false;
			bool copyThirdPartyToExe = false;

			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Win32:
					linkExtension = "_vc.lib";
					dllExtension = ".dll";
					copyThirdParty = true;
					break;
				case UnrealTargetPlatform.Win64:
					platformMidName = "64";
					linkExtension = "_vc.lib";
					dllExtension = ".dll";
					copyThirdParty = true;
					break;
				case UnrealTargetPlatform.Mac:
					linkExtension = dllExtension = ".dylib";
					libPrefix = "lib";
					copyThirdParty = true;
					break;
				case UnrealTargetPlatform.XboxOne:
					linkExtension = "_vc.lib";
					dllExtension = ".dll";
					copyThirdParty = true;
					copyThirdPartyToExe = true;
					break;
				case UnrealTargetPlatform.PS4:
					linkExtension = "_stub.a";
					dllExtension = ".prx";
					libPrefix = "lib";
					copyThirdParty = true;
					copyThirdPartyToExe = true;

					break;
				case UnrealTargetPlatform.Android:
					linkExtension = dllExtension = ".so";
					libPrefix = "lib";
					// This is tricky to get right.
					// Only external modules pass their PublicAdditionalLibraries as library directories for android linking
					// But plugins can't use the external type in the .uproject file.
					// Relative path is based off the linker in the game's intermediate directory so we need an absolute path
					// The current working directory as of the build step is Engine\Source so we can base it from there
					BasePath = System.IO.Path.Combine(System.IO.Path.GetFullPath("."), "../Plugins/FMODStudio/Lib/Android");
					WriteAndroidDeploy(System.IO.Path.Combine(BasePath, "deploy.txt"), configName);
					break;
				case UnrealTargetPlatform.IOS:
					linkExtension = "_iphoneos.a";
					libPrefix = "lib";
					break;
				case UnrealTargetPlatform.WinRT:
				case UnrealTargetPlatform.WinRT_ARM:
				case UnrealTargetPlatform.HTML5:
				case UnrealTargetPlatform.Linux:
					//extName = ".a";
					throw new System.Exception(System.String.Format("Unsupported platform {0}", Target.Platform.ToString()));
					//break;
			}
			
			//System.Console.WriteLine("FMOD Current path: " + System.IO.Path.GetFullPath("."));
			//System.Console.WriteLine("FMOD Base path: " + BasePath);

			PublicLibraryPaths.Add(BasePath);

			string fmodLibName = System.String.Format("{0}fmod{1}{2}{3}", libPrefix, configName, platformMidName, linkExtension);
			string fmodStudioLibName = System.String.Format("{0}fmodstudio{1}{2}{3}", libPrefix, configName, platformMidName, linkExtension);

			string fmodDllName = System.String.Format("{0}fmod{1}{2}{3}", libPrefix, configName, platformMidName, dllExtension);
			string fmodStudioDllName = System.String.Format("{0}fmodstudio{1}{2}{3}", libPrefix, configName, platformMidName, dllExtension);

			string fmodLibPath = System.IO.Path.Combine(BasePath, fmodLibName);
			string fmodStudioLibPath = System.IO.Path.Combine(BasePath, fmodStudioLibName);

			PublicAdditionalLibraries.Add(fmodLibPath);
			PublicAdditionalLibraries.Add(fmodStudioLibPath);

			if (copyThirdParty)
			{
				string destPath;
				if (copyThirdPartyToExe)
				{
					destPath = System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartyBinariesDirectory, System.String.Format("../{0}", platformName));
				}
				else
				{
					destPath = System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartyBinariesDirectory, System.String.Format("FMODStudio/{0}", platformName));
				}
				System.IO.Directory.CreateDirectory(destPath);

				string fmodDllSource = System.IO.Path.Combine(BasePath, fmodDllName);
				string fmodStudioDllSource = System.IO.Path.Combine(BasePath, fmodStudioDllName);

				string fmodDllDest = System.IO.Path.Combine(destPath, fmodDllName);
				string fmodStudioDllDest = System.IO.Path.Combine(destPath, fmodStudioDllName);

				CopyFile(fmodDllSource, fmodDllDest);
				CopyFile(fmodStudioDllSource, fmodStudioDllDest);
			}

			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDelayLoadDLLs.AddRange(
						new string[] {
							fmodDllName,
							fmodStudioDllName
							}
						);
			}
		}

		private void CopyFile(string source, string dest)
		{
			//System.Console.WriteLine("Copying {0} to {1}", source, dest);
			if (System.IO.File.Exists(dest))
			{
				System.IO.File.SetAttributes(dest, System.IO.File.GetAttributes(dest) & ~System.IO.FileAttributes.ReadOnly);
			}
			try
			{
				System.IO.File.Copy(source, dest, true);
			}
			catch (System.Exception ex)
			{
				System.Console.WriteLine("Failed to copy file: {0}", ex.Message);
			}
		}

		private void WriteAndroidDeploy(string fileName, string configLetter)
		{
			System.Console.WriteLine("WriteAndroidDeploy {0}, {1}", fileName, configLetter);
			string[] contents = new string[]
			{
				"fmod.jar",
				System.String.Format("libfmod{0}.so", configLetter),
				System.String.Format("libfmodstudio{0}.so", configLetter)
			};
			System.IO.File.WriteAllLines(fileName, contents);
		}
	}
}