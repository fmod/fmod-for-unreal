// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "FMODEventParameterSection.h"
#include "MovieSceneNameableTrack.h"
#include "FMODEventParameterTrack.generated.h"


/** Handles manipulation of event parameters in a movie scene. */
UCLASS(MinimalAPI)
class UFMODEventParameterTrack
    : public UMovieSceneNameableTrack
{
    GENERATED_UCLASS_BODY()

public:

    // Begin UMovieSceneTrack interface
    virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
    virtual UMovieSceneSection* CreateNewSection() override;
    virtual void RemoveAllAnimationData() override;
    virtual bool HasSection(const UMovieSceneSection& Section) const override;
    virtual void AddSection(UMovieSceneSection& Section) override;
    virtual void RemoveSection(UMovieSceneSection& Section) override;
    virtual bool IsEmpty() const override;
    virtual TRange<float> GetSectionBoundaries() const override;
    virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;

#if WITH_EDITORONLY_DATA
    virtual FText GetDefaultDisplayName() const override;
#endif
    // End UMovieSceneTrack interface

public:
    /** Adds an event parameter key to the track. */
    void FMODSTUDIO_API AddParameterKey(FName ParameterName, float Position, float Value);

    /** Gets the animated values for this track. */
    void Eval(float Position, TArray<FFMODEventParameterNameAndValue>& OutValues) const;

private:
    /** The sections owned by this track. */
    UPROPERTY()
    TArray<UMovieSceneSection*> Sections;
};
