// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventControlKeyArea.h"
#include "SFMODEventControlCurveKeyEditor.h"

bool FFMODEventControlKeyArea::CanCreateKeyEditor()
{
    return true;
}

TSharedRef<SWidget> FFMODEventControlKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
    return SNew(SFMODEventControlCurveKeyEditor)
        .Sequencer(Sequencer)
        .OwningSection(OwningSection)
        .Curve(&Curve)
        .Enum(Enum)
        .ExternalValue(ExternalValue);
};

uint8 FFMODEventControlKeyArea::ConvertCurveValueToIntegralType(int32 CurveValue) const
{
    return (uint8)FMath::Clamp<int32>(CurveValue, 0, TNumericLimits<uint8>::Max());
}
