// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventControlSection.h"


UFMODEventControlSection::UFMODEventControlSection(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ControlKeys.SetDefaultValue((int32)EFMODEventControlKey::Stop);
    ControlKeys.SetUseDefaultValueBeforeFirstKey(true);
    SetIsInfinite(true);
}


void UFMODEventControlSection::AddKey(float Time, EFMODEventControlKey::Type KeyType)
{
    ControlKeys.AddKey(Time, (int32)KeyType);
}


FIntegralCurve& UFMODEventControlSection::GetControlCurve()
{
    return ControlKeys;
}

const FIntegralCurve& UFMODEventControlSection::GetControlCurve() const
{
    return ControlKeys;
}

void UFMODEventControlSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
    Super::MoveSection(DeltaPosition, KeyHandles);

    ControlKeys.ShiftCurve(DeltaPosition, KeyHandles);
}


void UFMODEventControlSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
    Super::DilateSection(DilationFactor, Origin, KeyHandles);

    ControlKeys.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UFMODEventControlSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
    if (!TimeRange.Overlaps(GetRange()))
    {
        return;
    }

    for (auto It(ControlKeys.GetKeyHandleIterator()); It; ++It)
    {
        float Time = ControlKeys.GetKeyTime(It.Key());
        if (TimeRange.Contains(Time))
        {
            OutKeyHandles.Add(It.Key());
        }
    }
}


TOptional<float> UFMODEventControlSection::GetKeyTime(FKeyHandle KeyHandle) const
{
    if (ControlKeys.IsKeyHandleValid(KeyHandle))
    {
        return TOptional<float>(ControlKeys.GetKeyTime(KeyHandle));
    }
    return TOptional<float>();
}


void UFMODEventControlSection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
    if (ControlKeys.IsKeyHandleValid(KeyHandle))
    {
        ControlKeys.SetKeyTime(KeyHandle, Time);
    }
}
