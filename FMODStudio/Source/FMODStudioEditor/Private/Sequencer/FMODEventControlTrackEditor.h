// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

class FMovieSceneTrackEditor;

/** FMOD Event control track */
class FFMODEventControlTrackEditor : public FMovieSceneTrackEditor
{
public:
    FFMODEventControlTrackEditor(TSharedRef<ISequencer> InSequencer);

    static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

    void AddControlKey(const FGuid ObjectGuid);

    // Begin ISequencerTrackEditor interface
    virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
    virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
    virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
    // End ISequencerTrackEditor interface

private:

    /** Delegate for AnimatablePropertyChanged in AddKey. */
    virtual FKeyPropertyResult AddKeyInternal(float KeyTime, UObject* Object);
};


/** Class for event control sections. */
class FFMODEventControlSection
    : public ISequencerSection
    , public TSharedFromThis<FFMODEventControlSection>
{
public:
    FFMODEventControlSection(UMovieSceneSection& InSection, TSharedRef<ISequencer> InOwningSequencer);

    // Begin ISequencerSection interface
    virtual UMovieSceneSection* GetSectionObject() override;
    virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }
    virtual float GetSectionHeight() const override;
    virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
    virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
    virtual const FSlateBrush* GetKeyBrush(FKeyHandle KeyHandle) const override;
    virtual FVector2D GetKeyBrushOrigin(FKeyHandle KeyHandle) const override;
    virtual bool SectionIsResizable() const override { return false; }
    // End ISequencerSection interface

private:
    /** The section we are visualizing. */
    UMovieSceneSection& Section;

    /** The sequencer that owns this section */
    TWeakPtr<ISequencer> OwningSequencerPtr;

    /** The UEnum for the EFMODEventControlKey enum */
    const UEnum* ControlKeyEnum;

    const FSlateBrush* LeftKeyBrush;
    const FSlateBrush* RightKeyBrush;
};
