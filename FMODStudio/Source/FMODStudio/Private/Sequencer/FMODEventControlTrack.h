// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "MovieSceneNameableTrack.h"
#include "FMODEventControlTrack.generated.h"


class UMovieSceneSection;


/** Handles control of an FMOD Event */
UCLASS(MinimalAPI)
class UFMODEventControlTrack
    : public UMovieSceneNameableTrack
{
    GENERATED_UCLASS_BODY()

public:
    virtual TArray<UMovieSceneSection*> GetAllControlSections() const
    {
        return ControlSections;
    }

    // Begin UMovieSceneTrack interface
    virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
    virtual void RemoveAllAnimationData() override;
    virtual bool HasSection(const UMovieSceneSection& Section) const override;
    virtual void AddSection(UMovieSceneSection& Section) override;
    virtual void RemoveSection(UMovieSceneSection& Section) override;
    virtual bool IsEmpty() const override;
    virtual TRange<float> GetSectionBoundaries() const override;
    virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
    virtual void AddNewSection(float SectionTime);
    virtual UMovieSceneSection* CreateNewSection() override;

#if WITH_EDITORONLY_DATA
    virtual FText GetDefaultDisplayName() const override;
#endif
    // End UMovieSceneTrack interface

private:

    /** List of all evemt control sections. */
    UPROPERTY()
    TArray<UMovieSceneSection*> ControlSections;
};
