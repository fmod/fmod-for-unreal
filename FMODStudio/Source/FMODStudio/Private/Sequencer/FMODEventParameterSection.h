// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "MovieSceneSection.h"
#include "FMODEventParameterSection.generated.h"

/** Structure representing the animated value of an event parameter. */
struct FFMODEventParameterNameAndValue
{
    FFMODEventParameterNameAndValue(FName InParameterName, float InValue)
    {
        ParameterName = InParameterName;
        Value = InValue;
    }

    FName ParameterName;
    float Value;
};

/** Structure representing an animated event parameter and it's associated animation curve. */
USTRUCT()
struct FFMODEventParameterNameAndCurve
{
    GENERATED_USTRUCT_BODY()

    FFMODEventParameterNameAndCurve()
    {
        Index = 0;
    }

    FFMODEventParameterNameAndCurve(FName InParameterName);

    UPROPERTY()
    FName ParameterName;

    UPROPERTY()
    int32 Index;

    UPROPERTY()
    FRichCurve Curve;
};

/** A single movie scene section which can contain data for multiple event parameters. */
UCLASS(MinimalAPI)
class UFMODEventParameterSection
    : public UMovieSceneSection
{
    GENERATED_UCLASS_BODY()

public:
    /** Adds a a key for a specific event parameter at the specified time with the specified value. */
    void AddParameterKey(FName InParameterName, float InTime, float InValue);

    /** Removes a parameter from this section.  */
    FMODSTUDIO_API bool RemoveParameter(FName InParameterName);

    const TArray<FFMODEventParameterNameAndCurve>& GetParameterNamesAndCurves() const;
    FMODSTUDIO_API TArray<FFMODEventParameterNameAndCurve>& GetParameterNamesAndCurves();

    /** Gets the set of all parameter names used by this section. */
    FMODSTUDIO_API void GetParameterNames(TSet<FName>& ParameterNames) const;

    /** Gets the animated values for this track. */
    void Eval(float Position, TArray<FFMODEventParameterNameAndValue>& OutScalarValues) const;

    // Begin UMovieSceneSection interface
    virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
    virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
    virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
    virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
    virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;
    // End UMovieSceneSection interface

private:
    void UpdateParameterIndicesFromRemoval(int32 RemovedIndex);
    void GatherCurves(TArray<const FRichCurve*> &OutCurves) const;
    void GatherCurves(TArray<FRichCurve*> &OutCurves);

private:

    /** The event parameter names and their associated curves. */
    UPROPERTY()
    TArray<FFMODEventParameterNameAndCurve> EventParameterNamesAndCurves;
};
