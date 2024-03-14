// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2024.

#include "FMODEventControlSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "UObject/SequencerObjectVersion.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"

FFMODEventControlChannel::FFMODEventControlChannel()
{
#if WITH_HOT_RELOAD
    if (!GIsHotReload)
#endif
    {
        SetEnum(StaticEnum<EFMODEventControlKey>());
    }
}

UFMODEventControlSection::UFMODEventControlSection(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetRange(TRange<FFrameNumber>::All());

    int32 LinkerCustomVersion = GetLinkerCustomVersion(FSequencerObjectVersion::GUID);
    EMovieSceneCompletionMode CompletionMode;

    if (LinkerCustomVersion < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState)
    {
        CompletionMode = EMovieSceneCompletionMode::KeepState;
    }
    else if (LinkerCustomVersion < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault)
    {
        CompletionMode = EMovieSceneCompletionMode::RestoreState;
    }
    else
    {
        CompletionMode = EMovieSceneCompletionMode::ProjectDefault;
    }

    EvalOptions.EnableAndSetCompletionMode(CompletionMode);

#if WITH_EDITOR

    ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ControlKeys, FMovieSceneChannelMetaData(), TMovieSceneExternalValue<uint8>());

#else

    ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ControlKeys);

#endif
}
