// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#include "FMODStudioPrivatePCH.h"
#include "FMODEventControlSectionTemplate.h"
#include "FMODAmbientSound.h"
#include "FMODAudioComponent.h"
#include "IMovieScenePlayer.h"

struct FFMODEventControlPreAnimatedToken : IMovieScenePreAnimatedToken
{
    FFMODEventControlPreAnimatedToken() {}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
    FFMODEventControlPreAnimatedToken(FFMODEventControlPreAnimatedToken&&) = default;
    FFMODEventControlPreAnimatedToken& operator=(FFMODEventControlPreAnimatedToken&&) = default;
#else
    FFMODEventControlPreAnimatedToken(FFMODEventControlPreAnimatedToken&& RHS)
    {
        *this = MoveTemp(RHS);
    }
    FFMODEventControlPreAnimatedToken& operator=(FFMODEventControlPreAnimatedToken&& RHS)
    {
        Values = MoveTemp(RHS.Values);
        return *this;
    }
#endif

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

struct FFMODEventControlExecutionToken : IMovieSceneExecutionToken
{
    FFMODEventControlExecutionToken() = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
    FFMODEventControlExecutionToken(FFMODEventControlExecutionToken&&) = default;
    FFMODEventControlExecutionToken& operator=(FFMODEventControlExecutionToken&&) = default;
#else
    FFMODEventControlExecutionToken(FFMODEventControlExecutionToken&& RHS)
        : Values(MoveTemp(RHS.Values))
    {
    }
    FFMODEventControlExecutionToken& operator=(FFMODEventControlExecutionToken&& RHS)
    {
        Values = MoveTemp(RHS.Values);
        return *this;
    }
#endif

    virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
    {
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

            if (Value == EFMODEventControlKey::Play)
            {
                if (!AudioComponent->IsPlaying())
                    AudioComponent->Play();
            }
            else if (Value == EFMODEventControlKey::Stop)
            {
                AudioComponent->Stop();
            }
        }
    }

    EFMODEventControlKey::Type Value;
};

FFMODEventControlSectionTemplate::FFMODEventControlSectionTemplate(const UFMODEventControlSection& Section)
    : ControlCurve(Section.GetControlCurve())
{
}

void FFMODEventControlSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
    FFMODEventControlExecutionToken ExecutionToken;

    float Time = Context.GetTime();

    ExecutionToken.Value = (EFMODEventControlKey::Type)ControlCurve.Evaluate(Time);

    ExecutionTokens.Add(MoveTemp(ExecutionToken));
}
