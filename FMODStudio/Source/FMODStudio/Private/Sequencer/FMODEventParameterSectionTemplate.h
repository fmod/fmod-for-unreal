// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "FMODEventParameterSection.h"
#include "FMODEventParameterSectionTemplate.generated.h"

USTRUCT()
struct FFMODEventParameterSectionTemplate : public FMovieSceneEvalTemplate
{
    GENERATED_BODY()

    FFMODEventParameterSectionTemplate() {}
    FFMODEventParameterSectionTemplate(const UFMODEventParameterSection& Section);

private:
    /** The scalar parameter names and their associated curves. */
    UPROPERTY()
    TArray<FFMODEventParameterNameAndCurve> Parameters;

    virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
    virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
