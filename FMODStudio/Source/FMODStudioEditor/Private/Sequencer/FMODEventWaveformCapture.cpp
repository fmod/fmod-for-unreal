// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#include "Sequencer/FMODEventWaveformCapture.h"

#include "FMODEvent.h"
#include "FMODStudioModule.h"
#include "FMODUtils.h"
#include "Containers/Set.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"

#include "fmod_studio.hpp"
#include "fmod_dsp.h"

namespace
{
constexpr int32 CaptureSampleRate = 48000;
constexpr int32 CaptureUpdatesPerTick = 8;
constexpr int32 CaptureWatchdogPaddingMs = 2000;
constexpr int32 MaxPrepareUpdates = 120;
constexpr int32 MaxConsecutiveNoProgressUpdates = 120;

enum class EWaveformCaptureState : uint8
{
    Pending,
    PreparingEventChannelGroup,
    Capturing,
    Ready,
    Failed,
};

enum class EWaveformSessionState : uint8
{
    Uninitialized,
    Ready,
    Failed,
};

struct FActiveWaveformCapture
{
    FGuid EventGuid;
    int32 ExpectedDurationMs = 0;
    int32 CaptureSampleRate = 0;
    int32 PrepareUpdateCount = 0;
    uint64 CapturedFrameCount = 0;
    uint64 TargetFrameCount = 0;
    uint64 LastObservedFrameCount = 0;
    int32 ConsecutiveNoProgressUpdates = 0;
    TArray<float> Peaks;
    FMOD::Studio::EventInstance* EventInstance = nullptr;
    FMOD::ChannelGroup* EventChannelGroup = nullptr;
    FMOD::DSP* CaptureDsp = nullptr;
    FMOD_DSP_DESCRIPTION DspDescription = {};
    bool bCaptureArmed = false;
    bool bLoopPreview = false;
};

class FWaveformCaptureService
{
public:
    void Request(const UFMODEvent* Event, int32 EventLengthMs, bool bIsOneShot)
    {
        if (!IsValid(Event) || !Event->AssetGuid.IsValid() || EventLengthMs <= 0)
        {
            return;
        }

        const FGuid EventGuid = Event->AssetGuid;
        if (States.Contains(EventGuid))
        {
            return;
        }

        if (SessionState == EWaveformSessionState::Failed)
        {
            States.Add(EventGuid, EWaveformCaptureState::Failed);
            return;
        }

        FPendingRequest& Request = PendingRequests.AddDefaulted_GetRef();
        Request.EventGuid = EventGuid;
        Request.ExpectedDurationMs = EventLengthMs;
        Request.bLoopPreview = !bIsOneShot;
        States.Add(EventGuid, EWaveformCaptureState::Pending);
        EnsureTicker();
    }

    const FFMODEventWaveformData* FindReady(const FGuid& EventGuid) const
    {
        const EWaveformCaptureState* State = States.Find(EventGuid);
        return State != nullptr && *State == EWaveformCaptureState::Ready ? ReadyWaveforms.Find(EventGuid) : nullptr;
    }

    bool IsTerminal(const FGuid& EventGuid) const
    {
        if (!EventGuid.IsValid())
        {
            return false;
        }

        const EWaveformCaptureState* State = States.Find(EventGuid);
        return State != nullptr && (*State == EWaveformCaptureState::Ready || *State == EWaveformCaptureState::Failed);
    }

    void Shutdown()
    {
        if (TickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            TickerHandle.Reset();
        }

        PendingRequests.Reset();
        CleanupActiveCapture();
        CleanupActiveSystemResources();
        SessionState = EWaveformSessionState::Uninitialized;
        ReadyWaveforms.Reset();
        States.Reset();
    }

private:
    struct FPendingRequest
    {
        FGuid EventGuid;
        int32 ExpectedDurationMs = 0;
        bool bLoopPreview = false;
    };

    static FMOD_RESULT F_CALL CaptureDspRead(
        FMOD_DSP_STATE* DspState,
        float* InBuffer,
        float* OutBuffer,
        unsigned int Length,
        int InChannels,
        int* OutChannels)
    {
        void* UserData = nullptr;
        if (DspState == nullptr || FMOD_DSP_GETUSERDATA(DspState, &UserData) != FMOD_OK || UserData == nullptr)
        {
            return FMOD_ERR_INVALID_PARAM;
        }

        FActiveWaveformCapture* Capture = static_cast<FActiveWaveformCapture*>(UserData);
        if (OutChannels != nullptr)
        {
            *OutChannels = InChannels;
        }
        if (InBuffer != nullptr && OutBuffer != nullptr && InBuffer != OutBuffer && Length > 0 && InChannels > 0)
        {
            FMemory::Memcpy(OutBuffer, InBuffer, static_cast<SIZE_T>(Length) * InChannels * sizeof(float));
        }

        if (InBuffer == nullptr || Length == 0 || InChannels <= 0)
        {
            return FMOD_OK;
        }

        if (!Capture->bCaptureArmed || Capture->CaptureSampleRate <= 0)
        {
            return FMOD_OK;
        }

        if (Capture->bLoopPreview)
        {
            for (unsigned int FrameIndex = 0; FrameIndex < Length; ++FrameIndex)
            {
                if (Capture->CapturedFrameCount >= Capture->TargetFrameCount)
                {
                    break;
                }

                const uint64 CapturedFrameIndex = Capture->CapturedFrameCount++;
                const uint64 BucketIndex = CapturedFrameIndex * 1000ull / static_cast<uint64>(Capture->CaptureSampleRate);
                if (BucketIndex >= static_cast<uint64>(Capture->ExpectedDurationMs))
                {
                    continue;
                }
                float FramePeak = 0.0f;
                const float* Frame = InBuffer + static_cast<SIZE_T>(FrameIndex) * InChannels;
                for (int ChannelIndex = 0; ChannelIndex < InChannels; ++ChannelIndex)
                {
                    FramePeak = FMath::Max(FramePeak, FMath::Abs(Frame[ChannelIndex]));
                }

                Capture->Peaks[static_cast<int32>(BucketIndex)] = FMath::Max(Capture->Peaks[static_cast<int32>(BucketIndex)], FramePeak);
            }

            return FMOD_OK;
        }

        for (unsigned int FrameIndex = 0; FrameIndex < Length; ++FrameIndex)
        {
            const uint64 CapturedFrameIndex = Capture->CapturedFrameCount++;
            const uint64 BucketIndex = CapturedFrameIndex * 1000ull / static_cast<uint64>(Capture->CaptureSampleRate);
            if (BucketIndex >= static_cast<uint64>(Capture->ExpectedDurationMs))
            {
                continue;
            }
            float FramePeak = 0.0f;
            const float* Frame = InBuffer + static_cast<SIZE_T>(FrameIndex) * InChannels;
            for (int ChannelIndex = 0; ChannelIndex < InChannels; ++ChannelIndex)
            {
                FramePeak = FMath::Max(FramePeak, FMath::Abs(Frame[ChannelIndex]));
            }

            Capture->Peaks[static_cast<int32>(BucketIndex)] = FMath::Max(Capture->Peaks[static_cast<int32>(BucketIndex)], FramePeak);
        }

        return FMOD_OK;
    }

    bool Tick(float)
    {
        if (!ActiveCapture.IsValid() && !PendingRequests.IsEmpty())
        {
            StartPendingCapture();
        }
        else if (ActiveCapture.IsValid())
        {
            AdvanceActiveCapture();
        }

        if (!ActiveCapture.IsValid() && PendingRequests.IsEmpty())
        {
            if (TickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
                TickerHandle.Reset();
            }
            return false;
        }
        return true;
    }

    void EnsureTicker()
    {
        if (!TickerHandle.IsValid())
        {
            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FWaveformCaptureService::Tick));
        }
    }

    void FailActiveCaptureSilently()
    {
        if (!ActiveCapture.IsValid())
        {
            return;
        }

        States.FindOrAdd(ActiveCapture->EventGuid) = EWaveformCaptureState::Failed;
        CleanupActiveCapture();
    }

    void TerminalizePendingRequests()
    {
        for (const FPendingRequest& Request : PendingRequests)
        {
            States.FindOrAdd(Request.EventGuid) = EWaveformCaptureState::Failed;
        }
        PendingRequests.Reset();
    }

    void FailSession(FMOD_RESULT)
    {
        if (SessionState == EWaveformSessionState::Failed)
        {
            FailActiveCaptureSilently();
            TerminalizePendingRequests();
            return;
        }

        FailActiveCaptureSilently();
        CleanupActiveSystemResources();
        SessionState = EWaveformSessionState::Failed;
        TerminalizePendingRequests();
    }

    bool EnsureSystemInitialized()
    {
        if (SessionState == EWaveformSessionState::Ready && StudioSystem != nullptr && CoreSystem != nullptr)
        {
            return true;
        }

        if (SessionState == EWaveformSessionState::Failed)
        {
            FailActiveCaptureSilently();
            TerminalizePendingRequests();
            return false;
        }

        if (StudioSystem != nullptr || CoreSystem != nullptr || !LoadedBanks.IsEmpty())
        {
            CleanupActiveSystemResources();
        }
        SessionState = EWaveformSessionState::Uninitialized;

        const auto FailSystemInitialization = [this](FMOD_RESULT Result)
        {
            FailSession(Result);
            return false;
        };

        FMOD_RESULT Result = FMOD::Studio::System::create(&StudioSystem);
        if (Result != FMOD_OK)
        {
            return FailSystemInitialization(Result);
        }

        Result = StudioSystem->getCoreSystem(&CoreSystem);
        if (Result != FMOD_OK)
        {
            return FailSystemInitialization(Result);
        }

        Result = CoreSystem->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);
        if (Result != FMOD_OK)
        {
            return FailSystemInitialization(Result);
        }

        Result = CoreSystem->setSoftwareFormat(CaptureSampleRate, FMOD_SPEAKERMODE_STEREO, 0);
        if (Result != FMOD_OK)
        {
            return FailSystemInitialization(Result);
        }

        Result = CoreSystem->setDSPBufferSize(256, 4);
        if (Result != FMOD_OK)
        {
            return FailSystemInitialization(Result);
        }

        Result = StudioSystem->initialize(
            32,
            FMOD_STUDIO_INIT_ALLOW_MISSING_PLUGINS | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE,
            FMOD_INIT_MIX_FROM_UPDATE,
            nullptr);
        if (Result != FMOD_OK)
        {
            return FailSystemInitialization(Result);
        }

        TArray<FString> BankPaths;
        IFMODStudioModule::Get().GetAllBankPaths(BankPaths, true);
        if (BankPaths.IsEmpty())
        {
            return FailSystemInitialization(FMOD_ERR_FILE_NOTFOUND);
        }

        TSet<FString> SeenBankPaths;
        for (const FString& BankPath : BankPaths)
        {
            FString LoadPath = FPaths::ConvertRelativePathToFull(BankPath);
            FPaths::NormalizeFilename(LoadPath);
            FPaths::CollapseRelativeDirectories(LoadPath);

            FString DedupKey = LoadPath;
#if PLATFORM_WINDOWS
            DedupKey.ToLowerInline();
#endif
            if (SeenBankPaths.Contains(DedupKey))
            {
                continue;
            }

            FMOD::Studio::Bank* Bank = nullptr;
            Result = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*LoadPath), FMOD_STUDIO_LOAD_BANK_NORMAL, &Bank);
            if (Result != FMOD_OK)
            {
                return FailSystemInitialization(Result);
            }
            LoadedBanks.Add(Bank);
            SeenBankPaths.Add(MoveTemp(DedupKey));
        }

        SessionState = EWaveformSessionState::Ready;
        return true;
    }

    void StartPendingCapture()
    {
        const FPendingRequest Request = PendingRequests[0];
        PendingRequests.RemoveAt(0);
        const EWaveformCaptureState* State = States.Find(Request.EventGuid);
        if (State == nullptr || *State != EWaveformCaptureState::Pending)
        {
            return;
        }

        ActiveCapture = MakeUnique<FActiveWaveformCapture>();
        ActiveCapture->EventGuid = Request.EventGuid;
        ActiveCapture->ExpectedDurationMs = Request.ExpectedDurationMs;
        ActiveCapture->bLoopPreview = Request.bLoopPreview;
        if (!EnsureSystemInitialized())
        {
            return;
        }

        FMOD_SPEAKERMODE SpeakerMode = FMOD_SPEAKERMODE_DEFAULT;
        FMOD_RESULT Result = CoreSystem->getSoftwareFormat(&ActiveCapture->CaptureSampleRate, &SpeakerMode, nullptr);
        if (Result != FMOD_OK || ActiveCapture->CaptureSampleRate <= 0)
        {
            FailSession(Result == FMOD_OK ? FMOD_ERR_INTERNAL : Result);
            return;
        }

        FMOD::Studio::ID EventId = FMODUtils::ConvertGuid(ActiveCapture->EventGuid);
        FMOD::Studio::EventDescription* EventDescription = nullptr;
        Result = StudioSystem->getEventByID(&EventId, &EventDescription);
        if (Result != FMOD_OK || EventDescription == nullptr)
        {
            FailActiveCapture(Result == FMOD_OK ? FMOD_ERR_INVALID_PARAM : Result);
            return;
        }

        bool bIsOneShot = false;
        bool bIs3D = false;
        int32 EventLengthMs = 0;
        if (EventDescription->isOneshot(&bIsOneShot) != FMOD_OK ||
            (ActiveCapture->bLoopPreview ? bIsOneShot : !bIsOneShot) ||
            EventDescription->is3D(&bIs3D) != FMOD_OK ||
            EventDescription->getLength(&EventLengthMs) != FMOD_OK || EventLengthMs <= 0)
        {
            FailActiveCapture(FMOD_ERR_INVALID_PARAM);
            return;
        }
        ActiveCapture->ExpectedDurationMs = EventLengthMs;
        if (ActiveCapture->bLoopPreview)
        {
            ActiveCapture->TargetFrameCount =
                static_cast<uint64>(ActiveCapture->ExpectedDurationMs) * static_cast<uint64>(ActiveCapture->CaptureSampleRate) / 1000ull;
        }

        Result = EventDescription->createInstance(&ActiveCapture->EventInstance);
        if (Result != FMOD_OK)
        {
            FailActiveCapture(Result);
            return;
        }

        if (bIs3D)
        {
            const FMOD_VECTOR Zero = { 0.0f, 0.0f, 0.0f };
            const FMOD_VECTOR Forward = { 0.0f, 0.0f, 1.0f };
            const FMOD_VECTOR Up = { 0.0f, 1.0f, 0.0f };
            Result = CoreSystem->set3DListenerAttributes(0, &Zero, &Zero, &Forward, &Up);
            if (Result != FMOD_OK)
            {
                FailActiveCapture(Result);
                return;
            }

            FMOD_3D_ATTRIBUTES Attributes = {};
            Attributes.position = Zero;
            Attributes.velocity = Zero;
            Attributes.forward = Forward;
            Attributes.up = Up;
            Result = ActiveCapture->EventInstance->set3DAttributes(&Attributes);
            if (Result != FMOD_OK)
            {
                FailActiveCapture(Result);
                return;
            }
        }

        Result = ActiveCapture->EventInstance->setPaused(true);
        if (Result != FMOD_OK)
        {
            FailActiveCapture(Result);
            return;
        }
        Result = ActiveCapture->EventInstance->start();
        if (Result != FMOD_OK)
        {
            FailActiveCapture(Result);
            return;
        }
        States.FindOrAdd(ActiveCapture->EventGuid) = EWaveformCaptureState::PreparingEventChannelGroup;
    }
    void AdvanceActiveCapture()
    {
        for (int32 UpdateIndex = 0; UpdateIndex < CaptureUpdatesPerTick && ActiveCapture.IsValid(); ++UpdateIndex)
        {
            const FMOD_RESULT UpdateResult = StudioSystem->update();
            if (UpdateResult != FMOD_OK) { FailActiveCapture(UpdateResult); return; }
            if (ActiveCapture->EventChannelGroup == nullptr)
            {
                ++ActiveCapture->PrepareUpdateCount;
                FMOD::ChannelGroup* EventChannelGroup = nullptr;
                const FMOD_RESULT ChannelGroupResult = ActiveCapture->EventInstance->getChannelGroup(&EventChannelGroup);
                if (ChannelGroupResult == FMOD_ERR_STUDIO_NOT_LOADED)
                {
                    if (ActiveCapture->PrepareUpdateCount >= MaxPrepareUpdates) { FailActiveCapture(ChannelGroupResult); }
                    continue;
                }
                if (ChannelGroupResult != FMOD_OK || EventChannelGroup == nullptr) { FailActiveCapture(ChannelGroupResult == FMOD_OK ? FMOD_ERR_INVALID_PARAM : ChannelGroupResult); return; }
                ActiveCapture->EventChannelGroup = EventChannelGroup;
                ActiveCapture->DspDescription.pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
                FCStringAnsi::Strncpy(ActiveCapture->DspDescription.name, "UEWaveformCapture", UE_ARRAY_COUNT(ActiveCapture->DspDescription.name));
                ActiveCapture->DspDescription.version = 0x00010000;
                ActiveCapture->DspDescription.numinputbuffers = 1;
                ActiveCapture->DspDescription.numoutputbuffers = 1;
                ActiveCapture->DspDescription.read = &FWaveformCaptureService::CaptureDspRead;
                ActiveCapture->DspDescription.userdata = ActiveCapture.Get();
                FMOD_RESULT Result = CoreSystem->createDSP(&ActiveCapture->DspDescription, &ActiveCapture->CaptureDsp);
                if (Result != FMOD_OK) { FailActiveCapture(Result); return; }
                Result = ActiveCapture->CaptureDsp->setUserData(ActiveCapture.Get());
                if (Result != FMOD_OK) { FailActiveCapture(Result); return; }
                ActiveCapture->Peaks.SetNumZeroed(ActiveCapture->ExpectedDurationMs);
                Result = ActiveCapture->EventChannelGroup->addDSP(FMOD_CHANNELCONTROL_DSP_TAIL, ActiveCapture->CaptureDsp);
                if (Result != FMOD_OK) { FailActiveCapture(Result); return; }
                ActiveCapture->bCaptureArmed = true;
                Result = ActiveCapture->EventInstance->setPaused(false);
                if (Result != FMOD_OK) { ActiveCapture->bCaptureArmed = false; FailActiveCapture(Result); return; }
                ActiveCapture->LastObservedFrameCount = ActiveCapture->CapturedFrameCount;
                ActiveCapture->ConsecutiveNoProgressUpdates = 0;
                States.FindOrAdd(ActiveCapture->EventGuid) = EWaveformCaptureState::Capturing;
                continue;
            }
            if (ActiveCapture->bLoopPreview && ActiveCapture->CapturedFrameCount >= ActiveCapture->TargetFrameCount)
            {
                ActiveCapture->bCaptureArmed = false;
                SucceedLoopPreviewCapture();
                return;
            }
            FMOD_STUDIO_PLAYBACK_STATE PlaybackState = FMOD_STUDIO_PLAYBACK_STOPPED;
            const FMOD_RESULT PlaybackResult = ActiveCapture->EventInstance->getPlaybackState(&PlaybackState);
            if (PlaybackResult != FMOD_OK) { FailActiveCapture(PlaybackResult); return; }
            const uint64 RequiredFrameCount =
                (static_cast<uint64>(ActiveCapture->ExpectedDurationMs) *
                 static_cast<uint64>(ActiveCapture->CaptureSampleRate) + 999ull) / 1000ull;
            if (ActiveCapture->CapturedFrameCount >= RequiredFrameCount)
            {
                ActiveCapture->bCaptureArmed = false;
                SucceedActiveCapture();
                return;
            }
            if (PlaybackState == FMOD_STUDIO_PLAYBACK_STOPPED)
            {
                FailActiveCapture(FMOD_ERR_INTERNAL);
                return;
            }
            const uint64 CurrentFrameCount = ActiveCapture->CapturedFrameCount;
            if (CurrentFrameCount > ActiveCapture->LastObservedFrameCount)
            {
                ActiveCapture->LastObservedFrameCount = CurrentFrameCount;
                ActiveCapture->ConsecutiveNoProgressUpdates = 0;
            }
            else
            {
                ++ActiveCapture->ConsecutiveNoProgressUpdates;
                if (ActiveCapture->ConsecutiveNoProgressUpdates >= MaxConsecutiveNoProgressUpdates)
                {
                    FailActiveCapture(FMOD_ERR_INTERNAL);
                    return;
                }
            }
            const uint64 CapturedDurationMs = ActiveCapture->CapturedFrameCount * 1000ull / static_cast<uint64>(ActiveCapture->CaptureSampleRate);
            if (CapturedDurationMs > static_cast<uint64>(ActiveCapture->ExpectedDurationMs + CaptureWatchdogPaddingMs)) { FailActiveCapture(FMOD_ERR_INTERNAL); return; }
        }
    }
    void SucceedActiveCapture()
    {
float MaxPeak = 0.0f;
        float NormalizeScale = 1.0f;
        for (const float Peak : ActiveCapture->Peaks)
        {
            MaxPeak = FMath::Max(MaxPeak, Peak);
        }
        if (MaxPeak > KINDA_SMALL_NUMBER)
        {
            NormalizeScale = 0.92f / MaxPeak;
            for (float& Peak : ActiveCapture->Peaks)
            {
                Peak *= NormalizeScale;
            }
        }

        FFMODEventWaveformData& Waveform = ReadyWaveforms.Add(ActiveCapture->EventGuid);
        Waveform.Peaks = MoveTemp(ActiveCapture->Peaks);
        Waveform.DurationMs = ActiveCapture->ExpectedDurationMs;
        Waveform.BucketDurationMs = 1;
        Waveform.bLoopPreview = false;
        States.FindOrAdd(ActiveCapture->EventGuid) = EWaveformCaptureState::Ready;
        CleanupActiveCapture();
    }

    void SucceedLoopPreviewCapture()
    {
        ActiveCapture->bCaptureArmed = false;
        if (ActiveCapture->EventInstance != nullptr)
        {
            ActiveCapture->EventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
        }
        if (ActiveCapture->EventChannelGroup != nullptr && ActiveCapture->CaptureDsp != nullptr)
        {
            ActiveCapture->EventChannelGroup->removeDSP(ActiveCapture->CaptureDsp);
        }
        if (ActiveCapture->CaptureDsp != nullptr)
        {
            ActiveCapture->CaptureDsp->release();
            ActiveCapture->CaptureDsp = nullptr;
        }
        if (ActiveCapture->EventInstance != nullptr)
        {
            ActiveCapture->EventInstance->release();
            ActiveCapture->EventInstance = nullptr;
        }
        ActiveCapture->EventChannelGroup = nullptr;
        if (StudioSystem != nullptr)
        {
            StudioSystem->flushCommands();
            StudioSystem->update();
        }

        ActiveCapture->Peaks.SetNum(FMath::Min(ActiveCapture->Peaks.Num(), ActiveCapture->ExpectedDurationMs));
        float MaxPeak = 0.0f;
        for (const float Peak : ActiveCapture->Peaks)
        {
            MaxPeak = FMath::Max(MaxPeak, Peak);
        }
        if (MaxPeak > KINDA_SMALL_NUMBER)
        {
            const float NormalizeScale = 0.92f / MaxPeak;
            for (float& Peak : ActiveCapture->Peaks)
            {
                Peak *= NormalizeScale;
            }
        }

        FFMODEventWaveformData& Waveform = ReadyWaveforms.Add(ActiveCapture->EventGuid);
        Waveform.Peaks = MoveTemp(ActiveCapture->Peaks);
        Waveform.DurationMs = ActiveCapture->ExpectedDurationMs;
        Waveform.BucketDurationMs = 1;
        Waveform.bLoopPreview = true;
        States.FindOrAdd(ActiveCapture->EventGuid) = EWaveformCaptureState::Ready;
        ActiveCapture.Reset();
    }

    void FailActiveCapture(FMOD_RESULT)
    {
        if (ActiveCapture.IsValid())
        {
            States.FindOrAdd(ActiveCapture->EventGuid) = EWaveformCaptureState::Failed;
        }
        CleanupActiveCapture();
    }

private:
    void CleanupActiveEventResources()
    {
        if (!ActiveCapture.IsValid())
        {
            return;
        }


        ActiveCapture->bCaptureArmed = false;
        if (ActiveCapture->EventChannelGroup != nullptr && ActiveCapture->CaptureDsp != nullptr)
        {
            ActiveCapture->EventChannelGroup->removeDSP(ActiveCapture->CaptureDsp);
        }
        if (StudioSystem != nullptr)
        {
            StudioSystem->update();
        }
        if (ActiveCapture->CaptureDsp != nullptr)
        {
            ActiveCapture->CaptureDsp->release();
            ActiveCapture->CaptureDsp = nullptr;
        }
        if (ActiveCapture->EventInstance != nullptr)
        {
            ActiveCapture->EventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            ActiveCapture->EventInstance->release();
            ActiveCapture->EventInstance = nullptr;
        }
        if (StudioSystem != nullptr)
        {
            StudioSystem->flushCommands();
            StudioSystem->update();
        }
        ActiveCapture->EventChannelGroup = nullptr;
    }

    void CleanupActiveSystemResources()
    {
        for (FMOD::Studio::Bank* Bank : LoadedBanks)
        {
            if (Bank != nullptr)
            {
                Bank->unload();
            }
        }
        LoadedBanks.Reset();
        if (StudioSystem != nullptr)
        {
            StudioSystem->flushCommands();
            StudioSystem->update();
            StudioSystem->release();
        }
        StudioSystem = nullptr;
        CoreSystem = nullptr;
    }

    void CleanupActiveCapture()
    {
        if (!ActiveCapture.IsValid())
        {
            return;
        }

        CleanupActiveEventResources();
        ActiveCapture.Reset();
    }

    TMap<FGuid, EWaveformCaptureState> States;
    TMap<FGuid, FFMODEventWaveformData> ReadyWaveforms;
    TArray<FPendingRequest> PendingRequests;
    TUniquePtr<FActiveWaveformCapture> ActiveCapture;
    FMOD::Studio::System* StudioSystem = nullptr;
    FMOD::System* CoreSystem = nullptr;
    TArray<FMOD::Studio::Bank*> LoadedBanks;
    EWaveformSessionState SessionState = EWaveformSessionState::Uninitialized;
    FTSTicker::FDelegateHandle TickerHandle;
};

FWaveformCaptureService& GetWaveformCaptureService()
{
    static FWaveformCaptureService Service;
    return Service;
}
}

void FFMODEventWaveformCapture::Request(const UFMODEvent* Event, int32 EventLengthMs, bool bIsOneShot)
{
    GetWaveformCaptureService().Request(Event, EventLengthMs, bIsOneShot);
}

const FFMODEventWaveformData* FFMODEventWaveformCapture::FindReady(const FGuid& EventGuid)
{
    return GetWaveformCaptureService().FindReady(EventGuid);
}

bool FFMODEventWaveformCapture::IsTerminal(const FGuid& EventGuid)
{
    return GetWaveformCaptureService().IsTerminal(EventGuid);
}

void FFMODEventWaveformCapture::Shutdown()
{
    GetWaveformCaptureService().Shutdown();
}
