// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventControlTrackEditor.h"
#include "Sequencer/FMODEventControlSection.h"
#include "Sequencer/FMODEventControlTrack.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrackEditor.h"
#include "ISequencerSection.h"
#include "MovieSceneTrack.h"
#include "ISectionLayoutBuilder.h"
#include "SequencerSectionPainter.h"
#include "IKeyArea.h"
#include "CommonMovieSceneTools.h"
#include "FMODEventControlKeyArea.h"
#include "FMODAmbientSound.h"

#define LOCTEXT_NAMESPACE "FFMODEventControlTrackEditor"

FFMODEventControlSection::FFMODEventControlSection(UMovieSceneSection& InSection, TSharedRef<ISequencer>InOwningSequencer)
: Section(InSection)
, OwningSequencerPtr(InOwningSequencer)
{
    ControlKeyEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EFMODEventControlKey"));
    checkf(ControlKeyEnum != nullptr, TEXT("FFMODEventControlSection could not find the EFMODControlKey UEnum by name."))

    LeftKeyBrush = FEditorStyle::GetBrush("Sequencer.KeyLeft");
    RightKeyBrush = FEditorStyle::GetBrush("Sequencer.KeyRight");
}

UMovieSceneSection* FFMODEventControlSection::GetSectionObject()
{
    return &Section;
}

float FFMODEventControlSection::GetSectionHeight() const
{
    static const float SectionHeight = 20.0f;
    return SectionHeight;
}

void FFMODEventControlSection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
    UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(&Section);
    LayoutBuilder.SetSectionAsKeyArea(MakeShareable(new FFMODEventControlKeyArea(ControlSection->GetControlCurve(), ControlSection, ControlKeyEnum)));
}

int32 FFMODEventControlSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
    TSharedPtr<ISequencer> OwningSequencer = OwningSequencerPtr.Pin();

    if (!OwningSequencer.IsValid())
    {
        return InPainter.LayerId + 1;
    }

    const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
    const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

    FLinearColor TrackColor;

    // TODO: Set / clip stop time based on event length
    UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(&Section);

    if (ControlSection != nullptr)
    {
        UFMODEventControlTrack* ParentTrack = Cast<UFMODEventControlTrack>(ControlSection->GetOuter());
        if (ParentTrack != nullptr)
        {
            TrackColor = ParentTrack->GetColorTint();
        }
    }

    // TODO: This should only draw the visible ranges.
    TArray<TRange<float>> DrawRanges;
    TOptional<float> CurrentRangeStart;
    for (auto KeyIterator = ControlSection->GetControlCurve().GetKeyIterator(); KeyIterator; ++KeyIterator)
    {
        FIntegralKey Key = *KeyIterator;
        if ((EFMODEventControlKey::Type)Key.Value == EFMODEventControlKey::Play)
        {
            if (CurrentRangeStart.IsSet() == false)
            {
                CurrentRangeStart = Key.Time;
            }
        }
        if ((EFMODEventControlKey::Type)Key.Value == EFMODEventControlKey::Stop)
        {
            if (CurrentRangeStart.IsSet())
            {
                DrawRanges.Add(TRange<float>(CurrentRangeStart.GetValue(), Key.Time));
                CurrentRangeStart.Reset();
            }
        }
    }

    if (CurrentRangeStart.IsSet())
    {
        DrawRanges.Add(TRange<float>(CurrentRangeStart.GetValue(), OwningSequencer->GetViewRange().GetUpperBoundValue()));
    }

    for (const TRange<float>& DrawRange : DrawRanges)
    {
        float XOffset = TimeToPixelConverter.TimeToPixel(DrawRange.GetLowerBoundValue());
        float XSize = TimeToPixelConverter.TimeToPixel(DrawRange.GetUpperBoundValue()) - XOffset;
        FSlateDrawElement::MakeBox(
            InPainter.DrawElements,
            InPainter.LayerId,
            InPainter.SectionGeometry.ToPaintGeometry(FVector2D(XOffset, (InPainter.SectionGeometry.GetLocalSize().Y - SequencerSectionConstants::KeySize.Y) / 2), FVector2D(XSize, SequencerSectionConstants::KeySize.Y)),
            FEditorStyle::GetBrush("Sequencer.Section.Background"),
            DrawEffects
        );
        FSlateDrawElement::MakeBox(
            InPainter.DrawElements,
            InPainter.LayerId,
            InPainter.SectionGeometry.ToPaintGeometry(FVector2D(XOffset, (InPainter.SectionGeometry.GetLocalSize().Y - SequencerSectionConstants::KeySize.Y) / 2), FVector2D(XSize, SequencerSectionConstants::KeySize.Y)),
            FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint"),
            DrawEffects,
            TrackColor
        );
    }

    return InPainter.LayerId + 1;
}


const FSlateBrush* FFMODEventControlSection::GetKeyBrush(FKeyHandle KeyHandle) const
{
    UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(&Section);
    if (ControlSection != nullptr)
    {
        FIntegralKey ControlKey = ControlSection->GetControlCurve().GetKey(KeyHandle);
        if ((EFMODEventControlKey::Type)ControlKey.Value == EFMODEventControlKey::Play)
        {
            return LeftKeyBrush;
        }
        else if ((EFMODEventControlKey::Type)ControlKey.Value == EFMODEventControlKey::Stop)
        {
            return RightKeyBrush;
        }
    }
    return nullptr;
}

FVector2D FFMODEventControlSection::GetKeyBrushOrigin(FKeyHandle KeyHandle) const
{
    UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(&Section);
    if (ControlSection != nullptr)
    {
        FIntegralKey ControlKey = ControlSection->GetControlCurve().GetKey(KeyHandle);
        if ((EFMODEventControlKey::Type)ControlKey.Value == EFMODEventControlKey::Play)
        {
            return FVector2D(-1.0f, 1.0f);
        }
        else if ((EFMODEventControlKey::Type)ControlKey.Value == EFMODEventControlKey::Stop)
        {
            return FVector2D(1.0f, 1.0f);
        }
    }
    return FVector2D(0.0f, 0.0f);
}


FFMODEventControlTrackEditor::FFMODEventControlTrackEditor(TSharedRef<ISequencer> InSequencer)
    : FMovieSceneTrackEditor(InSequencer)
{ }

TSharedRef<ISequencerTrackEditor> FFMODEventControlTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
    return MakeShareable(new FFMODEventControlTrackEditor(InSequencer));
}

bool FFMODEventControlTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
    return Type == UFMODEventControlTrack::StaticClass();
}

TSharedRef<ISequencerSection> FFMODEventControlTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
    check(SupportsType(SectionObject.GetOuter()->GetClass()));

    const TSharedPtr<ISequencer> OwningSequencer = GetSequencer();
    return MakeShareable(new FFMODEventControlSection(SectionObject, OwningSequencer.ToSharedRef()));
}

void FFMODEventControlTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
    if (ObjectClass->IsChildOf(AFMODAmbientSound::StaticClass()) || ObjectClass->IsChildOf(UFMODAudioComponent::StaticClass()))
    {
        const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddFMODEventControlTrack", "FMOD Event Control Track"),
            LOCTEXT("FMODEventControlTooltip", "Adds a track for controlling FMOD event."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &FFMODEventControlTrackEditor::AddControlKey, ObjectBinding))
        );
    }
}

void FFMODEventControlTrackEditor::AddControlKey(const FGuid ObjectGuid)
{
    TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
    UObject* Object = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(ObjectGuid) : nullptr;

    if (Object)
    {
        AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FFMODEventControlTrackEditor::AddKeyInternal, Object));
    }
}

FKeyPropertyResult FFMODEventControlTrackEditor::AddKeyInternal(float KeyTime, UObject* Object)
{
	FKeyPropertyResult KeyPropertyResult;

    FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
    FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;

    if (ObjectHandle.IsValid())
    {
        FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UFMODEventControlTrack::StaticClass());
        UMovieSceneTrack* Track = TrackResult.Track;
		KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

        if (KeyPropertyResult.bTrackCreated && ensure(Track))
        {
            UFMODEventControlTrack* EventTrack = Cast<UFMODEventControlTrack>(Track);
            EventTrack->AddNewSection(KeyTime);
            EventTrack->SetDisplayName(LOCTEXT("TrackName", "FMOD Event"));
        }
    }

	return KeyPropertyResult;
}

#undef LOCTEXT_NAMESPACE
