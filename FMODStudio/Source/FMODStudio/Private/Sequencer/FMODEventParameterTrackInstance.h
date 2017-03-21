// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#pragma once

#include "IMovieSceneTrackInstance.h"
#include "ObjectKey.h"

class FTrackInstancePropertyBindings;
class UFMODEventParameterTrack;
class UFMODAudioComponent;

/**
 * A movie scene track instance for material tracks.
 */
class FFMODEventParameterTrackInstance
    : public IMovieSceneTrackInstance
{
private:
    typedef TMap<FName, float> NameToFloatMap;

public:
    FFMODEventParameterTrackInstance(UFMODEventParameterTrack& InEventParameterTrack);

    // Begin IMovieSceneTrackInstance interface
    virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
    virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
    virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
    virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
    virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
    // End IMovieSceneTrackInstance interface

private:
    /** Track that is being instanced */
    UFMODEventParameterTrack* EventParameterTrack;

    /** The initial values of instance parameters for the objects controlled by this track. */
    TMap<FObjectKey, TSharedPtr<NameToFloatMap>> ObjectToInitialParameterValuesMap;

    /** The FMODAudioComponents to be animated on update. */
    TArray<UFMODAudioComponent*> AudioComponents;
};
