// Copyright (c), Firelight Technologies Pty, Ltd. 2026-2026.

using UnrealBuildTool;
using System;

public struct FMODAudioLinkEditor
{
    public static void Apply(UnrealBuildTool.Rules.FMODStudio FMODModule, ReadOnlyTargetRules Target)
    {
        if (Target.bBuildEditor)
        {
            FMODModule.AddModule("FMODAudioLinkEditor", false);
        }
    }
}