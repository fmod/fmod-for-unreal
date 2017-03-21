// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#include "FMODStudioPrivatePCH.h"
#include "FMODEventParameterTrack.h"
#include "FMODEventParameterTrackInstance.h"
#include "FMODAmbientSound.h"
#include "FMODEvent.h"
#include "FMODStudioModule.h"
#include "fmod_studio.hpp"

FFMODEventParameterTrackInstance::FFMODEventParameterTrackInstance(UFMODEventParameterTrack& InEventParameterTrack)
{
    EventParameterTrack = &InEventParameterTrack;
}

void FFMODEventParameterTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
    for (auto ObjectPtr : RuntimeObjects)
    {
        UObject* Object = ObjectPtr.Get();
        AFMODAmbientSound* Sound = Cast<AFMODAmbientSound>(Object);

        if (Sound != nullptr)
        {
            UFMODAudioComponent* AudioComponent = Sound->AudioComponent;
            if (AudioComponent != nullptr)
            {
                TSharedPtr<NameToFloatMap> InitialParameterValues;
                TSharedPtr<NameToFloatMap>* InitialParameterValuesPtr = ObjectToInitialParameterValuesMap.Find(FObjectKey(Object));
                if (InitialParameterValuesPtr == nullptr)
                {
                    InitialParameterValues = MakeShareable(new NameToFloatMap());
                    ObjectToInitialParameterValuesMap.Add(FObjectKey(Object), InitialParameterValues);
                }
                else
                {
                    InitialParameterValues = *InitialParameterValuesPtr;
                }
                // TODO: Save current parameter values
            }
        }
    }
}

void FFMODEventParameterTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
    for (auto ObjectPtr : RuntimeObjects)
    {
        UObject* Object = ObjectPtr.Get();
        TSharedPtr<NameToFloatMap>* InitialParameterValuesPtr = ObjectToInitialParameterValuesMap.Find(FObjectKey(Object));

        if (InitialParameterValuesPtr != nullptr)
        {
            TSharedPtr<NameToFloatMap> InitialParameterValues = *InitialParameterValuesPtr;

            AFMODAmbientSound* Sound = Cast<AFMODAmbientSound>(Object);
            if (Sound != nullptr)
            {
                // TODO: Restorre current parameter values
            }
        }
    }
    ObjectToInitialParameterValuesMap.Empty();
}

void FFMODEventParameterTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
    TArray<FFMODEventParameterNameAndValue> ParameterNamesAndValues;
    EventParameterTrack->Eval(UpdateData.Position, ParameterNamesAndValues);
    for (UFMODAudioComponent* AudioComponent : AudioComponents)
    {
        for (const FFMODEventParameterNameAndValue& ParameterNameAndValue : ParameterNamesAndValues)
        {
            AudioComponent->SetParameter(ParameterNameAndValue.ParameterName, ParameterNameAndValue.Value);
        }
    }
}

void FFMODEventParameterTrackInstance::RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
    AudioComponents.Empty();

    // Collection the animated parameter names.
    TSet<FName> UsedParameterNames;
    for (UMovieSceneSection* Section : EventParameterTrack->GetAllSections())
    {
        UFMODEventParameterSection* ParameterSection = Cast<UFMODEventParameterSection>(Section);
        ParameterSection->GetParameterNames(UsedParameterNames);
    }

    // Cache the audio components which have matching parameter names.
    for (auto ObjectPtr : RuntimeObjects)
    {
        AFMODAmbientSound* Sound = Cast<AFMODAmbientSound>(ObjectPtr.Get());
        if (Sound != nullptr)
        {
            UFMODAudioComponent* AudioComponent = Sound->AudioComponent;
            if (AudioComponent != nullptr)
            {
                FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(AudioComponent->Event.Get());
                if (EventDesc != nullptr)
                {
                    for (FName ParameterName : UsedParameterNames)
                    {
                        FMOD_STUDIO_PARAMETER_DESCRIPTION ParameterDesc = {};
                        FMOD_RESULT Result = EventDesc->getParameter(TCHAR_TO_UTF8(*ParameterName.ToString()), &ParameterDesc);
                        if (Result == FMOD_OK)
                        {
                            AudioComponents.Add(AudioComponent);
                        }
                        // TODO: else Something?
                    }
                }
            }
        }
    }
}

void FFMODEventParameterTrackInstance::ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
    AudioComponents.Empty();
    ObjectToInitialParameterValuesMap.Empty();
}
