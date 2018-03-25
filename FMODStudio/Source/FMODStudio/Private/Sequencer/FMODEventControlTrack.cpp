// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventControlTrack.h"
#include "FMODEventControlSection.h"
#include "IMovieScenePlayer.h"
#include "FMODEventControlSectionTemplate.h"
#include "MovieSceneCommonHelpers.h"

#define LOCTEXT_NAMESPACE "FMODEventControlTrack"


UFMODEventControlTrack::UFMODEventControlTrack(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
    TrackTint = FColor(255,255,255,160);
#endif
}


FMovieSceneEvalTemplatePtr UFMODEventControlTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
    return FFMODEventControlSectionTemplate(*CastChecked<UFMODEventControlSection>(&InSection));
}


const TArray<UMovieSceneSection*>& UFMODEventControlTrack::GetAllSections() const
{
    return ControlSections;
}


void UFMODEventControlTrack::RemoveAllAnimationData()
{
    // do nothing
}


bool UFMODEventControlTrack::HasSection(const UMovieSceneSection& Section) const
{
    return ControlSections.Contains(&Section);
}


void UFMODEventControlTrack::AddSection(UMovieSceneSection& Section)
{
    ControlSections.Add(&Section);
}


void UFMODEventControlTrack::RemoveSection(UMovieSceneSection& Section)
{
    ControlSections.Remove(&Section);
}


bool UFMODEventControlTrack::IsEmpty() const
{
    return ControlSections.Num() == 0;
}


TRange<float> UFMODEventControlTrack::GetSectionBoundaries() const
{
    TArray< TRange<float> > Bounds;
    for (int32 i = 0; i < ControlSections.Num(); ++i)
    {
        Bounds.Add(ControlSections[i]->GetRange());
    }
    return TRange<float>::Hull(Bounds);
}


void UFMODEventControlTrack::AddNewSection(float SectionTime)
{
    if (MovieSceneHelpers::FindSectionAtTime(ControlSections, SectionTime) == nullptr)
    {
        UFMODEventControlSection* NewSection = Cast<UFMODEventControlSection>(CreateNewSection());
        NewSection->SetStartTime(SectionTime);
        NewSection->SetEndTime(SectionTime);
        ControlSections.Add(NewSection);
    }
}

UMovieSceneSection* UFMODEventControlTrack::CreateNewSection()
{
    return NewObject<UFMODEventControlSection>(this);
}

#if WITH_EDITORONLY_DATA
FText UFMODEventControlTrack::GetDefaultDisplayName() const
{
    return LOCTEXT("DisplayName", "FMOD Event");
}
#endif

#undef LOCTEXT_NAMESPACE
