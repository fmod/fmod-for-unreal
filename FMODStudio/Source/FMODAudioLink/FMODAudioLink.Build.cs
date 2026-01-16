// Copyright (c), Firelight Technologies Pty, Ltd. 2026-2026.

using UnrealBuildTool;
using System.IO;

public struct FMODAudioLink
{
    public static void Apply(UnrealBuildTool.Rules.FMODStudio FMODModule, ReadOnlyTargetRules Target)
    {
        FMODModule.AddModule("FMODAudioLink");
        FMODModule.PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AudioLinkCore",
                "AudioLinkEngine",
                "SignalProcessing"
            }
        );
    }
}