// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "IntegralKeyArea.h"

struct FIntegralCurve;
class SWidget;
class UEnum;
class UMovieSceneSection;

/** A key area for displaying and editing integral curves representing enums. */
class FFMODEventControlKeyArea
    : public FIntegralKeyArea<uint8> 
{
public:
    /** Creates a new key area for editing enum curves. */
    FFMODEventControlKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection, const UEnum* InEnum)
        : FIntegralKeyArea<uint8>(InCurve, InOwningSection)
        , Enum(InEnum)
    { }

    /** Creates a new key area for editing enum curves whose value can be overridden externally. */
    FFMODEventControlKeyArea(FIntegralCurve& InCurve, TAttribute<TOptional<uint8>> ExternalValue, UMovieSceneSection* InOwningSection, const UEnum* InEnum)
        : FIntegralKeyArea<uint8>(InCurve, ExternalValue, InOwningSection)
        , Enum(InEnum)
    { }

    // Begin FByteKeyArea interface
    virtual bool CanCreateKeyEditor() override;
    virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

protected:
    virtual uint8 ConvertCurveValueToIntegralType(int32 CurveValue) const override;
    // End FByteKeyArea interface

private:
    /** The enum which provides available integral values for this key area. */
    const UEnum* Enum;
};
