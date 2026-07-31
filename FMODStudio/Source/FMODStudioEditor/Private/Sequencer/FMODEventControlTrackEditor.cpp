// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#include "FMODEventControlTrackEditor.h"
#include "FMODAmbientSound.h"
#include "FMODAudioComponent.h"
#include "FMODEvent.h"
#include "FMODStudioModule.h"
#include "Sequencer/FMODEventControlSection.h"
#include "Sequencer/FMODEventControlTrack.h"
#include "Sequencer/FMODEventWaveformCapture.h"
#include "AnimatedRange.h"
#include "Containers/Ticker.h"
#include "Rendering/DrawElements.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "SequencerSectionPainter.h"
#include "TimeToPixel.h"
#include "Styling/AppStyle.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSequence.h"
#include "Tracks/MovieSceneObjectPropertyTrack.h"
#include "Sections/MovieSceneObjectPropertySection.h"
#include "Channels/MovieSceneObjectPathChannel.h"
#include "fmod_studio.hpp"

#define LOCTEXT_NAMESPACE "FFMODEventControlTrackEditor"

struct FFMODWaveformRefreshState
{
    TWeakPtr<ISequencer> WeakSequencer;
    FTSTicker::FDelegateHandle WaveformRefreshTickerHandle;
    FDelegateHandle MovieSceneDataChangedHandle;
    TWeakObjectPtr<UMovieScene> LastFocusedMovieScene;
    TSet<FGuid> TrackedWaveformGuids;
    bool bAuditioningLoadRequested = false;
    bool bSequenceScanned = false;
    bool bFinalRefreshIssued = false;
    bool bStructuralScanComplete = false;
    bool bHasFMODPlayKeys = false;
    bool bDiscoveryDirty = true;
    bool bIssuingWaveformRefresh = false;
    bool bIsActive = true;
};

namespace
{
TArray<TSharedPtr<FFMODWaveformRefreshState>> WaveformRefreshStates;

struct FVisualFMODRange
{
    TRange<float> Range;
    bool bDrawWaveform = false;
    FGuid EventGuid;
    FString EventLabel;
    FString EventPrefix;
    FString DurationLabel;
};

struct FResolvedEventForPlay
{
    UFMODEvent* Event = nullptr;
    bool bFoundEventKey = false;
};

FResolvedEventForPlay ResolveEventForPlay(
    const UMovieSceneObjectPropertyTrack* EventTrack,
    FFrameNumber PlayTime,
    UFMODEvent* FallbackEvent)
{
    FResolvedEventForPlay Result;
    if (EventTrack == nullptr)
    {
        Result.Event = FallbackEvent;
        return Result;
    }

    FFrameNumber LatestEventKeyTime;

    for (UMovieSceneSection* Section : EventTrack->GetAllSections())
    {
        const UMovieSceneObjectPropertySection* ObjectPropertySection = Cast<UMovieSceneObjectPropertySection>(Section);
        if (ObjectPropertySection == nullptr)
        {
            continue;
        }

        const TMovieSceneChannelData<const FMovieSceneObjectPathChannelKeyValue> ChannelData = ObjectPropertySection->ObjectChannel.GetData();
        const TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
        const TArrayView<const FMovieSceneObjectPathChannelKeyValue> Values = ChannelData.GetValues();

        for (int32 Index = 0; Index < Times.Num(); ++Index)
        {
            const FFrameNumber EventKeyTime = Times[Index];
            if (EventKeyTime <= PlayTime && (!Result.bFoundEventKey || EventKeyTime > LatestEventKeyTime))
            {
                Result.bFoundEventKey = true;
                LatestEventKeyTime = EventKeyTime;
                Result.Event = Cast<UFMODEvent>(Values[Index].Get());
            }
        }
    }

    return Result;
}

UFMODAudioComponent* GetAudioComponent(UObject* Object)
{
    if (AFMODAmbientSound* AmbientSound = Cast<AFMODAmbientSound>(Object))
    {
        return AmbientSound->AudioComponent;
    }

    return Cast<UFMODAudioComponent>(Object);
}

enum class EFMODEventPropertyTrackResolutionResult
{
    Invalid,
    NotFound,
    Found
};

struct FFMODEventControlTrackResolution
{
    EFMODEventPropertyTrackResolutionResult Result = EFMODEventPropertyTrackResolutionResult::Invalid;
    UFMODAudioComponent* AudioComponent = nullptr;
    const UMovieSceneObjectPropertyTrack* EventPropertyTrack = nullptr;
};

FFMODEventControlTrackResolution ResolveEventPropertyTrack(
    const UFMODEventControlTrack& ControlTrack,
    const UMovieScene& MovieScene,
    ISequencer& Sequencer)
{
    FFMODEventControlTrackResolution Resolution;
    const FGuid ControlGuid = ControlTrack.FindObjectBindingGuid();
    if (!ControlGuid.IsValid() || MovieScene.FindBinding(ControlGuid) == nullptr)
    {
        return Resolution;
    }

    UFMODAudioComponent* DirectAudioComponent = nullptr;
    AFMODAmbientSound* AmbientSound = nullptr;

    for (TWeakObjectPtr<> WeakObject : Sequencer.FindObjectsInCurrentSequence(ControlGuid))
    {
        UObject* Object = WeakObject.Get();
        if (!IsValid(Object))
        {
            return FFMODEventControlTrackResolution();
        }

        if (UFMODAudioComponent* Candidate = Cast<UFMODAudioComponent>(Object))
        {
            if (AmbientSound != nullptr ||
                (DirectAudioComponent != nullptr && DirectAudioComponent != Candidate))
            {
                return FFMODEventControlTrackResolution();
            }

            DirectAudioComponent = Candidate;
        }
        else if (AFMODAmbientSound* AmbientCandidate = Cast<AFMODAmbientSound>(Object))
        {
            if (DirectAudioComponent != nullptr ||
                (AmbientSound != nullptr && AmbientSound != AmbientCandidate))
            {
                return FFMODEventControlTrackResolution();
            }

            AmbientSound = AmbientCandidate;
        }
        else
        {
            return FFMODEventControlTrackResolution();
        }
    }

    FGuid ComponentGuid;
    UFMODAudioComponent* AudioComponent = nullptr;

    if (DirectAudioComponent != nullptr)
    {
        ComponentGuid = ControlGuid;
        AudioComponent = DirectAudioComponent;
    }
    else if (AmbientSound != nullptr && IsValid(AmbientSound->AudioComponent))
    {
        AudioComponent = AmbientSound->AudioComponent;
        int32 MatchingChildBindingCount = 0;

        for (const FMovieSceneBinding& Binding : MovieScene.GetBindings())
        {
            const FGuid ChildGuid = Binding.GetObjectGuid();
            const FMovieScenePossessable* Possessable = const_cast<UMovieScene&>(MovieScene).FindPossessable(ChildGuid);
            if (Possessable == nullptr || Possessable->GetParent() != ControlGuid)
            {
                continue;
            }

            bool bResolvesToAudioComponent = false;
            for (TWeakObjectPtr<> WeakObject : Sequencer.FindObjectsInCurrentSequence(ChildGuid))
            {
                if (WeakObject.Get() != AudioComponent)
                {
                    bResolvesToAudioComponent = false;
                    break;
                }

                bResolvesToAudioComponent = true;
            }

            if (bResolvesToAudioComponent)
            {
                ++MatchingChildBindingCount;
                ComponentGuid = ChildGuid;
                if (MatchingChildBindingCount > 1)
                {
                    return FFMODEventControlTrackResolution();
                }
            }
        }

        if (MatchingChildBindingCount != 1)
        {
            return FFMODEventControlTrackResolution();
        }
    }
    else
    {
        return Resolution;
    }

    const FMovieSceneBinding* ComponentBinding = MovieScene.FindBinding(ComponentGuid);
    if (ComponentBinding == nullptr)
    {
        return FFMODEventControlTrackResolution();
    }

    const UMovieSceneObjectPropertyTrack* EventPropertyTrack = nullptr;
    int32 EventPropertyTrackCount = 0;

    for (UMovieSceneTrack* Track : ComponentBinding->GetTracks())
    {
        const UMovieSceneObjectPropertyTrack* PropertyTrack = Cast<UMovieSceneObjectPropertyTrack>(Track);
        if (PropertyTrack == nullptr ||
            PropertyTrack->GetPropertyName() != GET_MEMBER_NAME_CHECKED(UFMODAudioComponent, Event))
        {
            continue;
        }

        ++EventPropertyTrackCount;
        EventPropertyTrack = PropertyTrack;
    }

    if (EventPropertyTrackCount > 1)
    {
        return FFMODEventControlTrackResolution();
    }

    Resolution.AudioComponent = AudioComponent;
    if (EventPropertyTrackCount == 0)
    {
        Resolution.Result = EFMODEventPropertyTrackResolutionResult::NotFound;
        return Resolution;
    }

    Resolution.Result = EFMODEventPropertyTrackResolutionResult::Found;
    Resolution.EventPropertyTrack = EventPropertyTrack;
    return Resolution;
}

bool HasFMODPlayKeys(const UMovieScene& MovieScene)
{
    const auto HasPlayKey = [](const UFMODEventControlTrack& ControlTrack)
    {
        for (UMovieSceneSection* Section : ControlTrack.GetAllSections())
        {
            const UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(Section);
            if (ControlSection == nullptr)
            {
                continue;
            }

            const TMovieSceneChannelData<const uint8> ChannelData = ControlSection->ControlKeys.GetData();
            const TArrayView<const uint8> Values = ChannelData.GetValues();
            for (const uint8 Value : Values)
            {
                if ((EFMODEventControlKey)Value == EFMODEventControlKey::Play)
                {
                    return true;
                }
            }
        }

        return false;
    };

    for (const FMovieSceneBinding& Binding : MovieScene.GetBindings())
    {
        for (UMovieSceneTrack* Track : Binding.GetTracks())
        {
            if (const UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track); ControlTrack != nullptr && HasPlayKey(*ControlTrack))
            {
                return true;
            }
        }
    }

    for (UMovieSceneTrack* Track : MovieScene.GetTracks())
    {
        if (const UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track); ControlTrack != nullptr && HasPlayKey(*ControlTrack))
        {
            return true;
        }
    }

    return false;
}

void RequestWaveformsForControlTrack(
    const UFMODEventControlTrack& ControlTrack,
    ISequencer& Sequencer,
    const UMovieScene& MovieScene,
    IFMODStudioModule& FMODStudioModule,
    TSet<FGuid>& TrackedWaveformGuids)
{
    const FFMODEventControlTrackResolution EventResolution = ResolveEventPropertyTrack(ControlTrack, MovieScene, Sequencer);
    const UMovieSceneObjectPropertyTrack* EventPropertyTrack =
        EventResolution.Result == EFMODEventPropertyTrackResolutionResult::Found ? EventResolution.EventPropertyTrack : nullptr;
    UFMODEvent* FallbackEvent =
        EventResolution.Result == EFMODEventPropertyTrackResolutionResult::NotFound && IsValid(EventResolution.AudioComponent)
            ? EventResolution.AudioComponent->Event
            : nullptr;

    for (UMovieSceneSection* Section : ControlTrack.GetAllSections())
    {
        const UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(Section);
        if (ControlSection == nullptr)
        {
            continue;
        }

        const TMovieSceneChannelData<const uint8> ChannelData = ControlSection->ControlKeys.GetData();
        const TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
        const TArrayView<const uint8> Values = ChannelData.GetValues();
        for (int32 Index = 0; Index < Times.Num(); ++Index)
        {
            if ((EFMODEventControlKey)Values[Index] != EFMODEventControlKey::Play)
            {
                continue;
            }

            const FResolvedEventForPlay ResolvedEvent = ResolveEventForPlay(EventPropertyTrack, Times[Index], FallbackEvent);
            UFMODEvent* Event = ResolvedEvent.Event;
            if (!IsValid(Event) || !Event->AssetGuid.IsValid() || TrackedWaveformGuids.Contains(Event->AssetGuid))
            {
                continue;
            }

            FMOD::Studio::EventDescription* EventDescription = FMODStudioModule.GetEventDescription(Event, EFMODSystemContext::Auditioning);
            bool bIsOneShot = false;
            int32 EventLengthMs = 0;
            if (EventDescription == nullptr ||
                EventDescription->isOneshot(&bIsOneShot) != FMOD_OK || !bIsOneShot ||
                EventDescription->getLength(&EventLengthMs) != FMOD_OK || EventLengthMs <= 0)
            {
                continue;
            }

            FFMODEventWaveformCapture::Request(Event, EventLengthMs, bIsOneShot);
            TrackedWaveformGuids.Add(Event->AssetGuid);
        }
    }
}

void RequestWaveformsForFocusedMovieScene(
    ISequencer& Sequencer,
    const UMovieScene& MovieScene,
    IFMODStudioModule& FMODStudioModule,
    TSet<FGuid>& TrackedWaveformGuids)
{
    for (const FMovieSceneBinding& Binding : MovieScene.GetBindings())
    {
        for (UMovieSceneTrack* Track : Binding.GetTracks())
        {
            if (const UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track))
            {
                RequestWaveformsForControlTrack(*ControlTrack, Sequencer, MovieScene, FMODStudioModule, TrackedWaveformGuids);
            }
        }
    }

    for (UMovieSceneTrack* Track : MovieScene.GetTracks())
    {
        if (const UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track))
        {
            RequestWaveformsForControlTrack(*ControlTrack, Sequencer, MovieScene, FMODStudioModule, TrackedWaveformGuids);
        }
    }
}
}

FFMODEventControlSection::FFMODEventControlSection(UMovieSceneSection &InSection, TSharedRef<ISequencer> InOwningSequencer, FGuid InObjectBinding)
    : Section(InSection)
    , OwningSequencerPtr(InOwningSequencer)
    , ObjectBinding(InObjectBinding)
{
}

UMovieSceneSection *FFMODEventControlSection::GetSectionObject()
{
    return &Section;
}

float FFMODEventControlSection::GetSectionHeight() const
{
    static const float SectionHeight = 52.f;
    return SectionHeight;
}

int32 FFMODEventControlSection::OnPaintSection(FSequencerSectionPainter &InPainter) const
{
    TSharedPtr<ISequencer> OwningSequencer = OwningSequencerPtr.Pin();

    if (!OwningSequencer.IsValid())
    {
        return InPainter.LayerId + 1;
    }

    const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
    const FTimeToPixel &TimeToPixelConverter = InPainter.GetTimeConverter();

    UFMODEventControlSection *ControlSection = Cast<UFMODEventControlSection>(&Section);

    TArray<FVisualFMODRange> VisualRanges;

    if (ControlSection != nullptr)
    {
        const UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(ControlSection->GetOuter());
        UMovieSceneSequence* Sequence = OwningSequencer->GetFocusedMovieSceneSequence();
        UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
        const FFMODEventControlTrackResolution EventResolution =
            ControlTrack != nullptr && MovieScene != nullptr
                ? ResolveEventPropertyTrack(*ControlTrack, *MovieScene, *OwningSequencer)
                : FFMODEventControlTrackResolution();
        const UMovieSceneObjectPropertyTrack* EventPropertyTrack =
            EventResolution.Result == EFMODEventPropertyTrackResolutionResult::Found ? EventResolution.EventPropertyTrack : nullptr;
        UFMODEvent* FallbackEvent =
            EventResolution.Result == EFMODEventPropertyTrackResolutionResult::NotFound && IsValid(EventResolution.AudioComponent)
                ? EventResolution.AudioComponent->Event
                : nullptr;

        TMovieSceneChannelData<const uint8> ChannelData = ControlSection->ControlKeys.GetData();
        TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
        TArrayView<const uint8> Values = ChannelData.GetValues();

        for (int32 Index = 0; Index < Times.Num(); ++Index)
        {
            if ((EFMODEventControlKey)Values[Index] != EFMODEventControlKey::Play)
            {
                continue;
            }

            const double PlaySeconds = Times[Index] / TimeToPixelConverter.GetTickResolution();
            const FResolvedEventForPlay ResolvedEvent = ResolveEventForPlay(EventPropertyTrack, Times[Index], FallbackEvent);
            UFMODEvent* EventForPlay = ResolvedEvent.Event;
            const FGuid EventGuid = IsValid(EventForPlay) ? EventForPlay->AssetGuid : FGuid();
            FString EventLabel = IsValid(EventForPlay) ? EventForPlay->GetName() : TEXT("NO EVENT");
            bool bDrawWaveform = false;
            int32 EventLengthMs = 0;
            FString EventPrefix;
            FString DurationLabel;
            double VisualEndSeconds = OwningSequencer->GetViewRange().GetUpperBoundValue();

            if (IsValid(EventForPlay) && IFMODStudioModule::IsAvailable())
            {
                IFMODStudioModule &FMODStudioModule = IFMODStudioModule::Get();
                if (FMODStudioModule.AreAuditioningBanksLoaded())
                {
                    FMOD::Studio::EventDescription *EventDescription = FMODStudioModule.GetEventDescription(EventForPlay, EFMODSystemContext::Auditioning);
                    if (EventDescription)
                    {
                        bool bIs3D = false;
                        bool bIsOneshot = false;
                        const FMOD_RESULT Is3DResult = EventDescription->is3D(&bIs3D);
                        const FMOD_RESULT IsOneshotResult = EventDescription->isOneshot(&bIsOneshot);
                        if (Is3DResult == FMOD_OK && IsOneshotResult == FMOD_OK)
                        {
                            EventPrefix = FString::Printf(TEXT("%s_%s"),
                                bIs3D ? TEXT("3D") : TEXT("2D"),
                                bIsOneshot ? TEXT("Once") : TEXT("Loop"));
                        }

                        if (EventDescription->getLength(&EventLengthMs) == FMOD_OK && EventLengthMs > 0)
                        {
                            VisualEndSeconds = PlaySeconds + EventLengthMs / 1000.0;
                            DurationLabel = FString::Printf(TEXT("%.2fs"), EventLengthMs / 1000.0);
                            bDrawWaveform = true;
                        }
                    }
                }
            }


            for (int32 CandidateIndex = Index + 1; CandidateIndex < Times.Num(); ++CandidateIndex)
            {
                const double CandidateSeconds = Times[CandidateIndex] / TimeToPixelConverter.GetTickResolution();
                const EFMODEventControlKey CandidateValue = (EFMODEventControlKey)Values[CandidateIndex];
                if (CandidateSeconds > PlaySeconds &&
                    CandidateSeconds < VisualEndSeconds &&
                    (CandidateValue == EFMODEventControlKey::Stop || CandidateValue == EFMODEventControlKey::Play))
                {
                    VisualEndSeconds = CandidateSeconds;
                    break;
                }
            }

            if (VisualEndSeconds > PlaySeconds)
            {
                VisualRanges.Add({ TRange<float>(PlaySeconds, VisualEndSeconds), bDrawWaveform, EventGuid, MoveTemp(EventLabel), MoveTemp(EventPrefix), MoveTemp(DurationLabel) });
            }
        }
    }
    for (const FVisualFMODRange &VisualRange : VisualRanges)
    {
        const TRange<float>& DrawRange = VisualRange.Range;
        float XOffset = TimeToPixelConverter.SecondsToPixel(DrawRange.GetLowerBoundValue());
        float XSize = TimeToPixelConverter.SecondsToPixel(DrawRange.GetUpperBoundValue()) - XOffset;
        const float RangeHeight = InPainter.SectionGeometry.GetLocalSize().Y;
        const float HeaderHeight = 18.0f;
        FSlateDrawElement::MakeBox(InPainter.DrawElements, InPainter.LayerId,
            InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(XSize, RangeHeight),
                FSlateLayoutTransform(1.0f, FVector2D(XOffset, 0.0f))),
            FAppStyle::GetBrush("Sequencer.Section.Background"), DrawEffects);
        FSlateDrawElement::MakeBox(InPainter.DrawElements, InPainter.LayerId,
            InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(XSize, RangeHeight),
                FSlateLayoutTransform(1.0f, FVector2D(XOffset, 0.0f))),
            FAppStyle::GetBrush("Sequencer.Section.BackgroundTint"), DrawEffects, FLinearColor::FromSRGBColor(FColor(93, 95, 136, 255)));
        const FSlateFontInfo HeaderFont = FAppStyle::GetFontStyle("SmallFont");
        const float HeaderLeftPadding = 6.0f;
        const float HeaderRightPadding = 4.0f;
        const float HeaderTextGap = 6.0f;
        const float HeaderTextHeight = 16.0f;
        const float HeaderTextY = 3.0f;
        const float HeaderLeft = XOffset + HeaderLeftPadding;
        const float HeaderRight = XOffset + XSize - HeaderRightPadding;
        const float HeaderContentWidth = FMath::Max(0.0f, HeaderRight - HeaderLeft);

        FString ParametersLabel;
        if (!VisualRange.EventPrefix.IsEmpty() && !VisualRange.DurationLabel.IsEmpty())
        {
            ParametersLabel = FString::Printf(TEXT("(%s_%s)"), *VisualRange.EventPrefix, *VisualRange.DurationLabel);
        }
        else if (!VisualRange.EventPrefix.IsEmpty())
        {
            ParametersLabel = FString::Printf(TEXT("(%s)"), *VisualRange.EventPrefix);
        }
        else if (!VisualRange.DurationLabel.IsEmpty())
        {
            ParametersLabel = FString::Printf(TEXT("(%s)"), *VisualRange.DurationLabel);
        }

        const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const float EventLabelBaseWidth = FontMeasure->Measure(VisualRange.EventLabel, HeaderFont).X;
        const float ParametersWidth = ParametersLabel.IsEmpty() ? 0.0f : FontMeasure->Measure(ParametersLabel, HeaderFont).X;
        const bool bDrawParameters = !ParametersLabel.IsEmpty() && ParametersWidth > 0.0f &&
            EventLabelBaseWidth + HeaderTextGap + ParametersWidth <= HeaderContentWidth;

        float TitleRight = HeaderRight;
        if (bDrawParameters)
        {
            const float ParametersLeft = HeaderRight - ParametersWidth;
            const FPaintGeometry ParametersGeometry = InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(ParametersWidth, HeaderTextHeight),
                FSlateLayoutTransform(1.0f, FVector2D(ParametersLeft, HeaderTextY)));
            const FPaintGeometry ParametersClipGeometry = InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(ParametersWidth, 18.0f),
                FSlateLayoutTransform(1.0f, FVector2D(ParametersLeft, 2.0f)));
            InPainter.DrawElements.PushClip(FSlateClippingZone(ParametersClipGeometry));
            FSlateDrawElement::MakeText(InPainter.DrawElements, InPainter.LayerId + 4,
                ParametersGeometry, ParametersLabel, HeaderFont, DrawEffects, FLinearColor::White);
            InPainter.DrawElements.PopClip();
            TitleRight = ParametersLeft - HeaderTextGap;
        }

        const float TitleAvailableWidth = FMath::Max(0.0f, TitleRight - HeaderLeft);
        if (!VisualRange.EventLabel.IsEmpty() && TitleAvailableWidth > 0.0f)
        {
            FSlateFontInfo TitleFont = HeaderFont;
            const int32 BaseTitleFontSize = HeaderFont.Size;
            const int32 MinTitleFontSize = FMath::Min(6, BaseTitleFontSize);
            if (BaseTitleFontSize > 0 && EventLabelBaseWidth > KINDA_SMALL_NUMBER && TitleAvailableWidth < EventLabelBaseWidth)
            {
                const float ScaledTitleSize = BaseTitleFontSize * (TitleAvailableWidth / EventLabelBaseWidth);
                TitleFont.Size = FMath::Clamp(FMath::FloorToInt(ScaledTitleSize), MinTitleFontSize, BaseTitleFontSize);
            }

            const FPaintGeometry TitleGeometry = InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(TitleAvailableWidth, HeaderTextHeight),
                FSlateLayoutTransform(1.0f, FVector2D(HeaderLeft, HeaderTextY)));
            const FPaintGeometry TitleClipGeometry = InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(TitleAvailableWidth, 18.0f),
                FSlateLayoutTransform(1.0f, FVector2D(HeaderLeft, 2.0f)));
            InPainter.DrawElements.PushClip(FSlateClippingZone(TitleClipGeometry));
            FSlateDrawElement::MakeText(InPainter.DrawElements, InPainter.LayerId + 4,
                TitleGeometry, VisualRange.EventLabel, TitleFont, DrawEffects, FLinearColor::White);
            InPainter.DrawElements.PopClip();
        }

        if (VisualRange.bDrawWaveform)
        {
            const float WaveformTop = HeaderHeight;
            const float WaveformBottom = FMath::Max(WaveformTop, RangeHeight - 2.0f);
            const float WaveformHeight = WaveformBottom - WaveformTop;
            const float WaveformWidth = FMath::Max(0.0f, XSize);
            if (WaveformWidth > 0.0f && WaveformHeight > 0.0f)
            {
                const float WaveformBaselineY = WaveformBottom;
                const FLinearColor WaveformPeakColor = FLinearColor::FromSRGBColor(FColor(42, 50, 136, 255));

                const FFMODEventWaveformData* RealWaveform = FFMODEventWaveformCapture::FindReady(VisualRange.EventGuid);
                if (RealWaveform != nullptr && RealWaveform->DurationMs > 0 && RealWaveform->BucketDurationMs > 0 && !RealWaveform->Peaks.IsEmpty())
                {
                    const double VisibleDurationMs = FMath::Min<double>(RealWaveform->DurationMs,
                        FMath::Max(0.0, (DrawRange.GetUpperBoundValue() - DrawRange.GetLowerBoundValue()) * 1000.0));
                    const int32 ColumnCount = FMath::Max(1, FMath::CeilToInt(WaveformWidth));
                    const double BucketsPerPixel = VisibleDurationMs /
                        (static_cast<double>(RealWaveform->BucketDurationMs) * ColumnCount);
                    for (int32 ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex)
                    {
                        float Peak = 0.0f;
                        if (BucketsPerPixel >= 1.0)
                        {
                            const double StartMs = VisibleDurationMs * ColumnIndex / ColumnCount;
                            const double EndMs = VisibleDurationMs * (ColumnIndex + 1) / ColumnCount;
                            const int32 StartPeakIndex = FMath::Clamp(FMath::FloorToInt(StartMs / RealWaveform->BucketDurationMs), 0, RealWaveform->Peaks.Num());
                            const int32 EndPeakIndex = FMath::Clamp(FMath::CeilToInt(EndMs / RealWaveform->BucketDurationMs), StartPeakIndex, RealWaveform->Peaks.Num());
                            for (int32 PeakIndex = StartPeakIndex; PeakIndex < EndPeakIndex; ++PeakIndex)
                            {
                                Peak = FMath::Max(Peak, RealWaveform->Peaks[PeakIndex]);
                            }
                        }
                        else
                        {
                            const double PeakPosition = VisibleDurationMs * (ColumnIndex + 0.5) /
                                (ColumnCount * RealWaveform->BucketDurationMs);
                            const int32 Index0 = FMath::Clamp(FMath::FloorToInt(PeakPosition), 0, RealWaveform->Peaks.Num() - 1);
                            const int32 Index1 = FMath::Min(Index0 + 1, RealWaveform->Peaks.Num() - 1);
                            const float Alpha = FMath::Frac(static_cast<float>(PeakPosition));
                            Peak = FMath::Lerp(RealWaveform->Peaks[Index0], RealWaveform->Peaks[Index1], Alpha);
                        }

                        const float NormalizedPeak = FMath::Clamp(Peak, 0.0f, 1.0f);
                        const float PeakY = WaveformBaselineY - NormalizedPeak * WaveformHeight;
                        const float X = XOffset + WaveformWidth * (ColumnIndex + 0.5f) / ColumnCount;
                        TArray<FVector2f> LinePoints;
                        LinePoints.Add(FVector2f(X, WaveformBaselineY));
                        LinePoints.Add(FVector2f(X, PeakY));
                        FSlateDrawElement::MakeLines(InPainter.DrawElements, InPainter.LayerId + 1, InPainter.SectionGeometry.ToPaintGeometry(), MoveTemp(LinePoints),
                            DrawEffects, WaveformPeakColor, false, 1.0f);
                    }
                }
            }
        }
    }

    return InPainter.LayerId + 5;
}

FFMODEventControlTrackEditor::FFMODEventControlTrackEditor(TSharedRef<ISequencer> InSequencer)
    : FMovieSceneTrackEditor(InSequencer)
{
}

FFMODEventControlTrackEditor::~FFMODEventControlTrackEditor()
{
    RemoveWaveformRefreshTicker();
}

void FFMODEventControlTrackEditor::RemoveWaveformRefreshTicker()
{
    const TSharedPtr<FFMODWaveformRefreshState> State = WaveformRefreshState.Pin();
    WaveformRefreshState.Reset();

    if (!State.IsValid())
    {
        return;
    }

    State->bIsActive = false;

    if (State->WaveformRefreshTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(State->WaveformRefreshTickerHandle);
        State->WaveformRefreshTickerHandle.Reset();
    }

    const TSharedPtr<ISequencer> Sequencer = State->WeakSequencer.Pin();
    if (Sequencer.IsValid() && State->MovieSceneDataChangedHandle.IsValid())
    {
        Sequencer->OnMovieSceneDataChanged().Remove(State->MovieSceneDataChangedHandle);
    }
    State->MovieSceneDataChangedHandle.Reset();

    WaveformRefreshStates.RemoveSingleSwap(State);
}

void FFMODEventControlTrackEditor::ShutdownWaveformRefreshTickers()
{
    for (const TSharedPtr<FFMODWaveformRefreshState>& State : WaveformRefreshStates)
    {
        if (!State.IsValid())
        {
            continue;
        }

        State->bIsActive = false;

        if (State->WaveformRefreshTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(State->WaveformRefreshTickerHandle);
            State->WaveformRefreshTickerHandle.Reset();
        }

        const TSharedPtr<ISequencer> Sequencer = State->WeakSequencer.Pin();
        if (Sequencer.IsValid() && State->MovieSceneDataChangedHandle.IsValid())
        {
            Sequencer->OnMovieSceneDataChanged().Remove(State->MovieSceneDataChangedHandle);
        }
        State->MovieSceneDataChangedHandle.Reset();
    }

    WaveformRefreshStates.Reset();
}

void FFMODEventControlTrackEditor::OnInitialize()
{
    FMovieSceneTrackEditor::OnInitialize();

    RemoveWaveformRefreshTicker();

    const TSharedPtr<ISequencer> Sequencer = GetSequencer();
    if (!Sequencer.IsValid())
    {
        return;
    }

    const TSharedRef<FFMODWaveformRefreshState> State = MakeShared<FFMODWaveformRefreshState>();
    State->WeakSequencer = Sequencer;
    WaveformRefreshState = State;
    WaveformRefreshStates.Add(State);

    const TWeakPtr<FFMODWaveformRefreshState> WeakState = State;
    State->MovieSceneDataChangedHandle = Sequencer->OnMovieSceneDataChanged().AddLambda(
        [WeakState](EMovieSceneDataChangeType)
        {
            const TSharedPtr<FFMODWaveformRefreshState> PinnedState = WeakState.Pin();
            if (PinnedState.IsValid() && PinnedState->bIsActive && !PinnedState->bIssuingWaveformRefresh)
            {
                PinnedState->bDiscoveryDirty = true;
            }
        });

    State->WaveformRefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [WeakState](float) -> bool
            {
                const TSharedPtr<FFMODWaveformRefreshState> PinnedState = WeakState.Pin();
                if (!PinnedState.IsValid() || !PinnedState->bIsActive)
                {
                    return false;
                }

                const TSharedPtr<ISequencer> PinnedSequencer = PinnedState->WeakSequencer.Pin();
                if (!PinnedSequencer.IsValid())
                {
                    return false;
                }

                UMovieSceneSequence* FocusedSequence = PinnedSequencer->GetFocusedMovieSceneSequence();
                UMovieScene* FocusedMovieScene = FocusedSequence != nullptr ? FocusedSequence->GetMovieScene() : nullptr;
                if (!IsValid(FocusedMovieScene))
                {
                    return true;
                }

                if (FocusedMovieScene != PinnedState->LastFocusedMovieScene.Get())
                {
                    PinnedState->LastFocusedMovieScene = FocusedMovieScene;
                    PinnedState->TrackedWaveformGuids.Reset();
                    PinnedState->bSequenceScanned = false;
                    PinnedState->bFinalRefreshIssued = false;
                    PinnedState->bStructuralScanComplete = false;
                    PinnedState->bHasFMODPlayKeys = false;
                    PinnedState->bDiscoveryDirty = true;
                }

                if (PinnedState->bDiscoveryDirty)
                {
                    PinnedState->bDiscoveryDirty = false;
                    PinnedState->bSequenceScanned = false;
                    PinnedState->bStructuralScanComplete = false;
                }

                if (!PinnedState->bStructuralScanComplete)
                {
                    PinnedState->bHasFMODPlayKeys = HasFMODPlayKeys(*FocusedMovieScene);
                    PinnedState->bStructuralScanComplete = true;
                }

                if (!PinnedState->bHasFMODPlayKeys)
                {
                    return true;
                }

                if (!IFMODStudioModule::IsAvailable())
                {
                    return true;
                }

                IFMODStudioModule& FMODStudioModule = IFMODStudioModule::Get();
                if (!FMODStudioModule.AreAuditioningBanksLoaded())
                {
                    if (!PinnedState->bAuditioningLoadRequested)
                    {
                        FMODStudioModule.LoadAuditioningBanks();
                        PinnedState->bAuditioningLoadRequested = true;
                    }
                    return true;
                }

                if (!PinnedState->bSequenceScanned)
                {
                    const int32 TrackedGuidCount = PinnedState->TrackedWaveformGuids.Num();
                    RequestWaveformsForFocusedMovieScene(*PinnedSequencer, *FocusedMovieScene, FMODStudioModule, PinnedState->TrackedWaveformGuids);
                    PinnedState->bSequenceScanned = true;

                    if (PinnedState->TrackedWaveformGuids.Num() > TrackedGuidCount)
                    {
                        PinnedState->bFinalRefreshIssued = false;
                    }
                }

                if (PinnedState->bSequenceScanned && !PinnedState->TrackedWaveformGuids.IsEmpty() && !PinnedState->bFinalRefreshIssued)
                {
                    for (const FGuid& EventGuid : PinnedState->TrackedWaveformGuids)
                    {
                        if (!FFMODEventWaveformCapture::IsTerminal(EventGuid))
                        {
                            return true;
                        }
                    }

                    PinnedState->bIssuingWaveformRefresh = true;
                    PinnedSequencer->NotifyMovieSceneDataChanged(
                        EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
                    PinnedState->bIssuingWaveformRefresh = false;
                    PinnedState->bFinalRefreshIssued = true;
                }

                return true;
            }),
        0.25f);
}

void FFMODEventControlTrackEditor::OnRelease()
{
    RemoveWaveformRefreshTicker();

    FMovieSceneTrackEditor::OnRelease();
}TSharedRef<ISequencerTrackEditor> FFMODEventControlTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
    return MakeShareable(new FFMODEventControlTrackEditor(InSequencer));
}

bool FFMODEventControlTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
    return Type == UFMODEventControlTrack::StaticClass();
}

TSharedRef<ISequencerSection> FFMODEventControlTrackEditor::MakeSectionInterface(
    UMovieSceneSection &SectionObject, UMovieSceneTrack &Track, FGuid ObjectBinding)
{
    check(SupportsType(SectionObject.GetOuter()->GetClass()));
    const TSharedPtr<ISequencer> OwningSequencer = GetSequencer();
    return MakeShareable(new FFMODEventControlSection(SectionObject, OwningSequencer.ToSharedRef(), ObjectBinding));
}

void FFMODEventControlTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder &MenuBuilder, const TArray<FGuid> &ObjectBindings, const UClass *ObjectClass)
{
    if (ObjectClass->IsChildOf(AFMODAmbientSound::StaticClass()) || ObjectClass->IsChildOf(UFMODAudioComponent::StaticClass()))
    {
        MenuBuilder.AddMenuEntry(LOCTEXT("AddFMODEventControlTrack", "FMOD Event Control Track"),
            LOCTEXT("FMODEventControlTooltip", "Adds a track for controlling FMOD event."), FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &FFMODEventControlTrackEditor::AddControlKey, ObjectBindings)));
    }
}

void FFMODEventControlTrackEditor::AddControlKey(TArray<FGuid> ObjectGuids)
{
    TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
    for (FGuid ObjectGuid : ObjectGuids)
    {
        UObject *Object = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(ObjectGuid) : nullptr;

        if (Object)
        {
            AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FFMODEventControlTrackEditor::AddKeyInternal, Object));
        }
    }
}

FKeyPropertyResult FFMODEventControlTrackEditor::AddKeyInternal(FFrameNumber KeyTime, UObject *Object)
{
    FKeyPropertyResult KeyPropertyResult;

    FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
    FGuid ObjectHandle = HandleResult.Handle;
    KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;

    if (ObjectHandle.IsValid())
    {
        FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UFMODEventControlTrack::StaticClass());
        UMovieSceneTrack *Track = TrackResult.Track;
        KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

        if (KeyPropertyResult.bTrackCreated && ensure(Track))
        {
            UFMODEventControlTrack *EventTrack = Cast<UFMODEventControlTrack>(Track);
            EventTrack->AddNewSection(KeyTime);
            EventTrack->SetDisplayName(LOCTEXT("TrackName", "FMOD Event"));
            KeyPropertyResult.bTrackModified = true;
        }
    }

    return KeyPropertyResult;
}

#undef LOCTEXT_NAMESPACE
