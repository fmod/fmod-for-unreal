// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventParameterTrack.h"
#include "FMODEventParameterSection.h"
#include "FMODEventParameterSectionTemplate.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCommonHelpers.h"

#define LOCTEXT_NAMESPACE "FMODEventParameterTrack"

UFMODEventParameterTrack::UFMODEventParameterTrack(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
    TrackTint = FColor(0, 170, 255, 65);
#endif
}

FMovieSceneEvalTemplatePtr UFMODEventParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
    return FFMODEventParameterSectionTemplate(*CastChecked<UFMODEventParameterSection>(&InSection));
}

UMovieSceneSection* UFMODEventParameterTrack::CreateNewSection()
{
    return NewObject<UFMODEventParameterSection>(this, UFMODEventParameterSection::StaticClass(), NAME_None, RF_Transactional);
}

void UFMODEventParameterTrack::RemoveAllAnimationData()
{
    Sections.Empty();
}

bool UFMODEventParameterTrack::HasSection(const UMovieSceneSection& Section) const
{
    return Sections.Contains(&Section);
}

void UFMODEventParameterTrack::AddSection(UMovieSceneSection& Section)
{
    Sections.Add(&Section);
}

void UFMODEventParameterTrack::RemoveSection(UMovieSceneSection& Section)
{
    Sections.Remove(&Section);
}

bool UFMODEventParameterTrack::IsEmpty() const
{
    return Sections.Num() == 0;
}

TRange<float> UFMODEventParameterTrack::GetSectionBoundaries() const
{
    TArray< TRange<float> > Bounds;

    for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
    {
        Bounds.Add(Sections[SectionIndex]->GetRange());
    }

    return TRange<float>::Hull(Bounds);
}

const TArray<UMovieSceneSection*>& UFMODEventParameterTrack::GetAllSections() const
{
    return Sections;
}

#if WITH_EDITORONLY_DATA
FText UFMODEventParameterTrack::GetDefaultDisplayName() const
{
    return LOCTEXT("DisplayName", "FMOD Event Parameter");
}
#endif

void UFMODEventParameterTrack::AddParameterKey(FName ParameterName, float Time, float Value)
{
    UFMODEventParameterSection* NearestSection = Cast<UFMODEventParameterSection>(MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time));
    if (NearestSection == nullptr)
    {
        NearestSection = Cast<UFMODEventParameterSection>(CreateNewSection());
        NearestSection->SetStartTime(Time);
        NearestSection->SetEndTime(Time);
        Sections.Add(NearestSection);
    }
    NearestSection->AddParameterKey(ParameterName, Time, Value);
}

void UFMODEventParameterTrack::Eval(float Position, TArray<FFMODEventParameterNameAndValue>& OutValues) const
{
    for (UMovieSceneSection* Section : Sections)
    {
        UFMODEventParameterSection* ParameterSection = Cast<UFMODEventParameterSection>(Section);
        if (ParameterSection != nullptr)
        {
            ParameterSection->Eval(Position, OutValues);
        }
    }
}

#undef LOCTEXT_NAMESPACE
