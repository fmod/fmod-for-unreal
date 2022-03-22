// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2022.

#include "FMODStudioStyle.h"
#include "SlateCore/Public/Styling/SlateStyle.h"
#include "EditorStyle/Public/Interfaces/IEditorStyleModule.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style.RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

//////////////////////////////////////////////////////////////////////////
// FFMODStudioStyle

void FFMODStudioStyle::Initialize()
{
    FSlateStyleSet& Style = static_cast<FSlateStyleSet&>(FEditorStyle::Get());

    Style.Set("ClassIcon.FMODAmbientSound", new IMAGE_BRUSH("Icons/AssetIcons/AmbientSound_16x", FVector2D(16.0f, 16.0f)));
    Style.Set("ClassThumbnail.FMODAmbientSound", new IMAGE_BRUSH("Icons/AssetIcons/AmbientSound_64x", FVector2D(64.0f, 64.0f)));
    Style.Set("ClassIcon.FMODAudioComponent", new IMAGE_BRUSH("Icons/ActorIcons/SoundActor_16x", FVector2D(16.0f, 16.0f)));
    Style.Set("ClassIcon.FMODAsset", new IMAGE_BRUSH("Icons/ActorIcons/SoundActor_16x", FVector2D(16.0f, 16.0f)));
}

//////////////////////////////////////////////////////////////////////////

#undef IMAGE_BRUSH
