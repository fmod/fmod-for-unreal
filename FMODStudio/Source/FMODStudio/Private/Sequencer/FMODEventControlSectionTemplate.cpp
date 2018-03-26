// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventControlSectionTemplate.h"
#include "FMODAmbientSound.h"
#include "FMODAudioComponent.h"
#include "IMovieScenePlayer.h"

struct FFMODEventControlPreAnimatedToken : IMovieScenePreAnimatedToken
{
    FFMODEventControlPreAnimatedToken() {}

    FFMODEventControlPreAnimatedToken(FFMODEventControlPreAnimatedToken&&) = default;
    FFMODEventControlPreAnimatedToken& operator=(FFMODEventControlPreAnimatedToken&&) = default;

    virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
    {
        UFMODAudioComponent* AudioComponent = CastChecked<UFMODAudioComponent>(&Object);

        if (Value == EFMODEventControlKey::Play)
        {
            AudioComponent->Play();
        }
        else if (Value == EFMODEventControlKey::Stop)
        {
            AudioComponent->Stop();
        }
    }

    EFMODEventControlKey::Type Value;
};

struct FFMODEventControlPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
    virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
    {
        UFMODAudioComponent* AudioComponent = CastChecked<UFMODAudioComponent>(&Object);

        FFMODEventControlPreAnimatedToken Token;

        if (AudioComponent)
        {
            if (AudioComponent->IsPlaying())
            {
                Token.Value = EFMODEventControlKey::Play;
            }
            else
            {
                Token.Value = EFMODEventControlKey::Stop;
            }
        }

        return MoveTemp(Token);
    }
};

struct FFMODEventKeyState : IPersistentEvaluationData
{
    FKeyHandle LastKeyHandle;
    FKeyHandle InvalidKeyHandle;
};

struct FFMODEventControlExecutionToken : IMovieSceneExecutionToken
{
    FFMODEventControlExecutionToken(EFMODEventControlKey::Type InEventControlKey, TOptional<FKeyHandle> InKeyHandle, float InKeyTime)
        : EventControlKey(InEventControlKey), KeyHandle(InKeyHandle), KeyTime(InKeyTime)
    {
    }

    /** Execute this token, operating on all objects referenced by 'Operand' */
    virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
    {
        if (KeyHandle.IsSet())
        {
            PersistentData.GetOrAddSectionData<FFMODEventKeyState>().LastKeyHandle = KeyHandle.GetValue();
        }

        for (TWeakObjectPtr<>& WeakObject : Player.FindBoundObjects(Operand))
        {
            UFMODAudioComponent* AudioComponent = Cast<UFMODAudioComponent>(WeakObject.Get());
            if (!AudioComponent)
            {
                AFMODAmbientSound* AmbientSound = Cast<AFMODAmbientSound>(WeakObject.Get());
                AudioComponent = AmbientSound ? AmbientSound->AudioComponent : nullptr;
            }

            if (!AudioComponent)
            {
                continue;
            }

            Player.SavePreAnimatedState(*AudioComponent, TMovieSceneAnimTypeID<FFMODEventControlExecutionToken>(), FFMODEventControlPreAnimatedTokenProducer());

            if (EventControlKey == EFMODEventControlKey::Play)
            {
                if (AudioComponent->IsPlaying())
                {
                    AudioComponent->Stop();
                }

                EFMODSystemContext::Type SystemContext = (GWorld && GWorld->WorldType == EWorldType::Editor) ? EFMODSystemContext::Editor : EFMODSystemContext::Runtime;
                AudioComponent->PlayInternal(SystemContext);

                float seekTime = Context.GetTime() - KeyTime;
                AudioComponent->SetTimelinePosition(seekTime * 1000.0f);
            }
            else if (EventControlKey == EFMODEventControlKey::Stop)
            {
                AudioComponent->Stop();
            }
        }
    }

    EFMODEventControlKey::Type EventControlKey;
    TOptional<FKeyHandle> KeyHandle;
    float KeyTime;
};

FFMODEventControlSectionTemplate::FFMODEventControlSectionTemplate(const UFMODEventControlSection& Section)
    : ControlCurve(Section.GetControlCurve())
{
}

void FFMODEventControlSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
    const bool bPlaying = Context.IsSilent() == false && Context.GetDirection() == EPlayDirection::Forwards && Context.GetRange().Size<float>() >= 0.f && 
        Context.GetStatus() == EMovieScenePlayerStatus::Playing;

    const FFMODEventKeyState* SectionData = PersistentData.FindSectionData<FFMODEventKeyState>();

    if (!bPlaying)
    {
        ExecutionTokens.Add(FFMODEventControlExecutionToken(EFMODEventControlKey::Stop, SectionData ? SectionData->InvalidKeyHandle : TOptional<FKeyHandle>(), 0.f));
    }
    else
    {
        float Time = Context.GetTime();
        FKeyHandle PreviousHandle = ControlCurve.FindKeyBeforeOrAt(Time);

        if (ControlCurve.IsKeyHandleValid(PreviousHandle) && (!SectionData || SectionData->LastKeyHandle != PreviousHandle))
        {
            FIntegralKey Key = ControlCurve.GetKey(PreviousHandle);
            ExecutionTokens.Add(FFMODEventControlExecutionToken((EFMODEventControlKey::Type)Key.Value, PreviousHandle, Key.Time));
        }
    }
}
