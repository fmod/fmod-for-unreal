// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

class ISequencer;
class UMovieSceneSection;
struct FIntegralCurve;

/** A widget for editing a curve representing enum keys. */
class SFMODEventControlCurveKeyEditor : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFMODEventControlCurveKeyEditor) {}

        /** The sequencer which is controlling this key editor. */
        SLATE_ARGUMENT(ISequencer*, Sequencer)

        /** The section that owns the data edited by this key editor. */
        SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)

        /** The curve being edited by this curve editor. */
        SLATE_ARGUMENT(FIntegralCurve*, Curve)

        /** The UEnum type which provides options for the curve being edited. */
        SLATE_ARGUMENT(const UEnum*, Enum)

        /** Allows the value displayed and edited by this key editor to be supplied from an external source.  This
            is useful for curves on property tracks who's property value can change without changing the animation. */
        SLATE_ATTRIBUTE(TOptional<uint8>, ExternalValue)

    SLATE_END_ARGS();

    void Construct(const FArguments& InArgs);

private:
    int32 OnGetCurrentValue() const;

    void OnComboSelectionChanged(int32 InSelectedItem, ESelectInfo::Type SelectInfo);

    ISequencer* Sequencer;
    UMovieSceneSection* OwningSection;
    FIntegralCurve* Curve;
    TAttribute<TOptional<uint8>> ExternalValue;
};