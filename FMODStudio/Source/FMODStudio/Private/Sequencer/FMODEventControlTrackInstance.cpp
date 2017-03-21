// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#include "FMODStudioPrivatePCH.h"
#include "FMODEventControlTrackInstance.h"
#include "IMovieScenePlayer.h"
#include "FMODEventControlTrack.h"
#include "FMODEventControlSection.h"
#include "FMODAmbientSound.h"


FFMODEventControlTrackInstance::~FFMODEventControlTrackInstance()
{
}

static UFMODAudioComponent* AudioComponentFromRuntimeObject(UObject* Object)
{
    if(AFMODAmbientSound* Sound = Cast<AFMODAmbientSound>(Object))
    {
        return Sound->AudioComponent;
    }
    else
    {
        return Cast<UFMODAudioComponent>(Object);
    }
}

void FFMODEventControlTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
    if (UpdateData.Position >= UpdateData.LastPosition && Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
    {
        const TArray<UMovieSceneSection*> Sections = EventControlTrack->GetAllControlSections();
        EFMODEventControlKey::Type EventControlKey = EFMODEventControlKey::Stop;
        bool bKeyFound = false;

        for (int32 i = 0; i < Sections.Num(); ++i)
        {
            UFMODEventControlSection* Section = Cast<UFMODEventControlSection>(Sections[i]);
            if (Section->IsActive())
            {
                FIntegralCurve& EventControlKeyCurve = Section->GetControlCurve();
                FKeyHandle PreviousHandle = EventControlKeyCurve.FindKeyBeforeOrAt(UpdateData.Position);
                if (EventControlKeyCurve.IsKeyHandleValid(PreviousHandle))
                {
                    FIntegralKey& PreviousKey = EventControlKeyCurve.GetKey(PreviousHandle);
                    if (PreviousKey.Time >= UpdateData.LastPosition)
                    {
                        EventControlKey = (EFMODEventControlKey::Type)PreviousKey.Value;
                        bKeyFound = true;
                    }
                }
            }
        }
        
        if (bKeyFound)
        {
            for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
            {
                UFMODAudioComponent* AudioComponent = AudioComponentFromRuntimeObject(RuntimeObjects[i].Get());

                if (AudioComponent != nullptr)
                {
                    if (EventControlKey == EFMODEventControlKey::Play)
                    {
                        if (AudioComponent->IsActive())
                        {
                            AudioComponent->SetActive(false, true);
                        }
                        AudioComponent->SetActive(true, true);
                    }
                    else if(EventControlKey == EFMODEventControlKey::Stop)
                    {
                        AudioComponent->SetActive(false, true);
                    }
                }
            }
        }
    }
    else
    {
        for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
        {
            UObject* Object = RuntimeObjects[i].Get();
            AFMODAmbientSound* Sound = Cast<AFMODAmbientSound>(Object);

            if (Sound != nullptr)
            {
                Sound->AudioComponent->SetActive(false, true);
            }
            else if (UFMODAudioComponent* Component =  Cast<UFMODAudioComponent>(Object))
            {
                Component->SetActive(false, true);
            }
        }
    }
}
