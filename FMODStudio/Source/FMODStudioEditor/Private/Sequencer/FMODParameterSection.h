// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "PropertySection.h"

class UMovieSceneSection;

/** A movie scene section for Event parameters. */
class FFMODParameterSection
    : public FPropertySection
{
public:
    FFMODParameterSection(UMovieSceneSection& InSectionObject, const FText& SectionName)
        : FPropertySection(InSectionObject, SectionName)
    { }

    // Begin ISequencerSection interface
    virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
    virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
    // End ISequencerSection interface
};