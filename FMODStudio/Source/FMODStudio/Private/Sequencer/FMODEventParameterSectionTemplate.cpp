// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventParameterSectionTemplate.h"
#include "FMODAmbientSound.h"
#include "FMODEvent.h"
#include "FMODEventParameterTrack.h"
#include "IMovieScenePlayer.h"
#include "fmod_studio.hpp"

struct FFMODEventParameterPreAnimatedToken : IMovieScenePreAnimatedToken
{
    FFMODEventParameterPreAnimatedToken() {}

    FFMODEventParameterPreAnimatedToken(FFMODEventParameterPreAnimatedToken&&) = default;
    FFMODEventParameterPreAnimatedToken& operator=(FFMODEventParameterPreAnimatedToken&&) = default;

    virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
    {
        UFMODAudioComponent* AudioComponent = CastChecked<UFMODAudioComponent>(&Object);

        for (FFMODEventParameterNameAndValue& Value : Values)
        {
            AudioComponent->SetParameter(Value.ParameterName, Value.Value);
        }
    }

    TArray< FFMODEventParameterNameAndValue > Values;
};

struct FFMODEventParameterPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
    virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
    {
        UFMODAudioComponent* AudioComponent = CastChecked<UFMODAudioComponent>(&Object);

        FFMODEventParameterPreAnimatedToken Token;

        if (AudioComponent && AudioComponent->Event)
        {
            TArray<FMOD_STUDIO_PARAMETER_DESCRIPTION> ParameterDescriptions;
            AudioComponent->Event->GetParameterDescriptions(ParameterDescriptions);
            for (const FMOD_STUDIO_PARAMETER_DESCRIPTION& ParameterDescription : ParameterDescriptions)
            {
                float Value = AudioComponent->GetParameter(ParameterDescription.name);
                Token.Values.Add(FFMODEventParameterNameAndValue(ParameterDescription.name, Value));
            }
        }

        return MoveTemp(Token);
    }
};

struct FFMODEventParameterExecutionToken : IMovieSceneExecutionToken
{
    FFMODEventParameterExecutionToken() = default;

    FFMODEventParameterExecutionToken(FFMODEventParameterExecutionToken&&) = default;
    FFMODEventParameterExecutionToken& operator=(FFMODEventParameterExecutionToken&&) = default;

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

            Player.SavePreAnimatedState(*AudioComponent, TMovieSceneAnimTypeID<FFMODEventParameterExecutionToken>(), FFMODEventParameterPreAnimatedTokenProducer());

            for (const FFMODEventParameterNameAndValue& NameAndValue : Values)
            {
                AudioComponent->SetParameter(NameAndValue.ParameterName, NameAndValue.Value);
            }
        }
    }

    TArray< FFMODEventParameterNameAndValue > Values;
};

FFMODEventParameterSectionTemplate::FFMODEventParameterSectionTemplate(const UFMODEventParameterSection& Section)
    : Parameters(Section.GetParameterNamesAndCurves())
{
}

void FFMODEventParameterSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
    FFMODEventParameterExecutionToken ExecutionToken;

    float Time = Context.GetTime();

    for (const FFMODEventParameterNameAndCurve& Parameter : Parameters)
    {
        ExecutionToken.Values.Add(FFMODEventParameterNameAndValue(Parameter.ParameterName, Parameter.Curve.Eval(Time)));
    }

    ExecutionTokens.Add(MoveTemp(ExecutionToken));
}
