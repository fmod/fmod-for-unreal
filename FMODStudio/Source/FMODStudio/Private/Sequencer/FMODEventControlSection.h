// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "FMODEventControlSection.generated.h"


/** Defines the types of FMOD event control keys. */
UENUM()
namespace EFMODEventControlKey
{
    enum Type
    {
        Play = 0,
        Stop = 1
    };
}


/** FMOD Event control section */
UCLASS(MinimalAPI)
class UFMODEventControlSection
    : public UMovieSceneSection
{
    GENERATED_UCLASS_BODY()

public:
    FMODSTUDIO_API void AddKey(float Time, EFMODEventControlKey::Type KeyType);

    FMODSTUDIO_API FIntegralCurve& GetControlCurve();
    const FIntegralCurve& GetControlCurve() const;

    // Begin UMovieSceneSection interface
    virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
    virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
    virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
    virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
    virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;
    // End UMovieSceneSection interface

private:

    /** Curve containing the event control keys */
    UPROPERTY()
    FIntegralCurve ControlKeys;
};
