// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UFMODEventControlTrack;


/**
 * Instance of a UEventControlTrack
 */
class FFMODEventControlTrackInstance
    : public IMovieSceneTrackInstance
{
public:

    FFMODEventControlTrackInstance(UFMODEventControlTrack& InTrack)
        : EventControlTrack(&InTrack)
    { }

    virtual ~FFMODEventControlTrackInstance();

    /** IMovieSceneTrackInstance interface */
    virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { /*TODO: Something?*/ }
    virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { /*TODO: Something?*/ }
    virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
    virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { /*TODO: Something?*/ }
    virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { /*TODO: Something?*/ }

private:
    /** Track that is being instanced */
    UFMODEventControlTrack* EventControlTrack;
};
