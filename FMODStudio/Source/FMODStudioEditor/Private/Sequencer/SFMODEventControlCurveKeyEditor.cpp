// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "SFMODEventControlCurveKeyEditor.h"
#include "MovieSceneToolHelpers.h"

#define LOCTEXT_NAMESPACE "SFMODEventControlCurveKeyEditor"

void SFMODEventControlCurveKeyEditor::Construct(const FArguments& InArgs)
{
    Sequencer = InArgs._Sequencer;
    OwningSection = InArgs._OwningSection;
    Curve = InArgs._Curve;
    ExternalValue = InArgs._ExternalValue;

    ChildSlot
    [
        MovieSceneToolHelpers::MakeEnumComboBox(
            InArgs._Enum,
            TAttribute<int32>::Create(TAttribute<int32>::FGetter::CreateSP(this, &SFMODEventControlCurveKeyEditor::OnGetCurrentValue)),
            FOnEnumSelectionChanged::CreateSP(this, &SFMODEventControlCurveKeyEditor::OnComboSelectionChanged)
        )
    ];
}

int32 SFMODEventControlCurveKeyEditor::OnGetCurrentValue() const
{
    if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
    {
        return ExternalValue.Get().GetValue();
    }
	
    float CurrentTime = Sequencer->GetLocalTime();
    int32 DefaultValue = 0;
    return Curve->Evaluate(CurrentTime, DefaultValue);
}

void SFMODEventControlCurveKeyEditor::OnComboSelectionChanged(int32 InSelectedItem, ESelectInfo::Type SelectInfo)
{
    FScopedTransaction Transaction(LOCTEXT("SetEnumKey", "Set Enum Key Value"));
    OwningSection->SetFlags(RF_Transactional);

    if (OwningSection->TryModify())
    {
        float CurrentTime = Sequencer->GetLocalTime();
        bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();

        FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
        if (Curve->IsKeyHandleValid(CurrentKeyHandle))
        {
            Curve->SetKeyValue(CurrentKeyHandle, InSelectedItem);
        }
        else
        {
            if (Curve->GetNumKeys() != 0 || bAutoSetTrackDefaults == false)
            {
                // When auto setting track defaults are disabled, add a key even when it's empty so that the changed
                // value is saved and is propagated to the property.
                Curve->AddKey(CurrentTime, InSelectedItem, CurrentKeyHandle);
            }

            if (OwningSection->GetStartTime() > CurrentTime)
            {
                OwningSection->SetStartTime(CurrentTime);
            }
            if (OwningSection->GetEndTime() < CurrentTime)
            {
                OwningSection->SetEndTime(CurrentTime);
            }
        }

        // Always update the default value when auto-set default values is enabled so that the last changes
        // are always saved to the track.
        if (bAutoSetTrackDefaults)
        {
            Curve->SetDefaultValue(InSelectedItem);
        }

        Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately );
    }
}

#undef LOCTEXT_NAMESPACE
