// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2022.

#pragma once
#include "SlateCore/Public/Styling/SlateStyle.h"
#include "EditorStyle/Public/EditorStyleSet.h"

class FFMODStudioStyle
{
public:
    static void Initialize();

    static FName GetStyleSetName();

    static void Shutdown();

private:
    static TSharedRef<class FSlateStyleSet> Create();

private:
    static TSharedPtr<class FSlateStyleSet> StyleInstance;

private:
    FFMODStudioStyle() {}
};
