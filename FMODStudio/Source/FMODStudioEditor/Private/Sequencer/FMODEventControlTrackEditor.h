// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

class FMenuBuilder;
class FSequencerSectionPainter;

struct FFMODWaveformRefreshState;

/** FMOD Event control track */
class FFMODEventControlTrackEditor : public FMovieSceneTrackEditor
{
public:
    FFMODEventControlTrackEditor(TSharedRef<ISequencer> InSequencer);
    virtual ~FFMODEventControlTrackEditor() override;

    static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);
    static void ShutdownWaveformRefreshTickers();

    void AddControlKey(TArray<FGuid> ObjectGuids);

    virtual void OnInitialize() override;
    virtual void OnRelease() override;

    // Begin ISequencerTrackEditor interface
    virtual void BuildObjectBindingTrackMenu(FMenuBuilder &MenuBuilder, const TArray<FGuid> &ObjectBindings, const UClass *ObjectClass) override;
    virtual TSharedRef<ISequencerSection> MakeSectionInterface(
        UMovieSceneSection &SectionObject, UMovieSceneTrack &Track, FGuid ObjectBinding) override;
    virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
    // End ISequencerTrackEditor interface

private:
    void RemoveWaveformRefreshTicker();

    TWeakPtr<FFMODWaveformRefreshState> WaveformRefreshState;
    /** Delegate for AnimatablePropertyChanged in AddKey. */
    virtual FKeyPropertyResult AddKeyInternal(FFrameNumber KeyTime, UObject *Object);
};

/** Class for event control sections. */
class FFMODEventControlSection : public ISequencerSection, public TSharedFromThis<FFMODEventControlSection>
{
public:
    FFMODEventControlSection(UMovieSceneSection &InSection, TSharedRef<ISequencer> InOwningSequencer, FGuid InObjectBinding);

    // Begin ISequencerSection interface
    virtual UMovieSceneSection *GetSectionObject() override;
    virtual float GetSectionHeight() const override;
    virtual int32 OnPaintSection(FSequencerSectionPainter &InPainter) const override;
    virtual bool SectionIsResizable() const override { return false; }
    // End ISequencerSection interface

private:
    /** The section we are visualizing. */
    UMovieSceneSection &Section;

    /** The sequencer that owns this section */
    TWeakPtr<ISequencer> OwningSequencerPtr;

    /** The object binding visualized by this editor-only section interface. */
    FGuid ObjectBinding;

};
