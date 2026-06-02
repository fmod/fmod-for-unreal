// Copyright (c), Firelight Technologies Pty, Ltd. 2023-2026.

#include "FMODAudioLinkInputClient.h"
#include "FMODAudioLinkLog.h"
#include "FMODAudioLinkSettings.h"
#include "FMODAudioLinkFactory.h"
#include "FMODAudioLinkComponent.h"
#include "FMODEvent.h"

#include "FMODStudioModule.h"
#include "FMODBlueprintStatics.h"

#include <inttypes.h>
#include "Async/Async.h"
#include "Templates/SharedPointer.h"

class InputClientRef
{
public:
    TSharedRef<FFMODAudioLinkInputClient> InputClient;

    InputClientRef(TSharedRef<FFMODAudioLinkInputClient> InputSP)
        : InputClient(InputSP)
    {
    }
};

FMOD::Studio::System* GetStudioSystem()
{
    if (IFMODStudioModule::IsAvailable())
    {
        auto* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
        if (!StudioSystem)
        {
            StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Auditioning);
        }
        return StudioSystem;
    }
    return nullptr;
}

void FFMODAudioLinkInputClient::Register(const FName& NameOfProducingSource)
{
    const auto Name = NameOfProducingSource.GetPlainNameString();

    if (UNLIKELY(!Settings.IsValid()))
    {
        UE_LOG(LogFMODAudioLink, Warning, TEXT("FFMODAudioLinkInputClient::Register: FMODAudioLinkSettings are not valid."));
        return;
    }

    if (UNLIKELY(!GetStudioSystem()))
    {
        UE_LOG(LogFMODAudioLink, Warning, TEXT("FFMODAudioLinkInputClient::Register: Unable to get FMOD Studio System."));
        return;
    }

    AsyncTask(ENamedThreads::GameThread, []
    {
        const auto AudioDeviceManager = FAudioDeviceManager::Get();
        if (UNLIKELY(!AudioDeviceManager))
        {
            UE_LOG(LogFMODAudioLink, Warning, TEXT("FFMODAudioLinkInputClient::Register: No AudioDeviceManager at registration."));
            return;
        }
        const auto AudioDevice = AudioDeviceManager->GetActiveAudioDevice();
        if (UNLIKELY(!AudioDevice))
        {
            UE_LOG(LogFMODAudioLink, Warning, TEXT("FFMODAudioLinkInputClient::Register: No active AudioDevice at registration."));
            return;
        }
        UE_CLOG(UNLIKELY(AudioDevice->GetMaxChannels() == 0), LogFMODAudioLink, Verbose,
            TEXT("FMODAudioLink: The current AudioDevice %d has 0 MaxChannels. Consider setting AudioMaxChannels to a sensible value in the Engine config file's TargetSettings for your platform."),
            AudioDevice->DeviceID);

        UE_CLOG(!FFMODAudioLinkFactory::bHasSubmix,
            LogFMODAudioLink, Verbose, TEXT("FMODAudioLink: No initial submix got routed to AudioLink. Consider creating custom versions of global submixes in Project Settings Audio, and Enable Audio Link in their advanced settings."));
    });
}

void FFMODAudioLinkInputClient::Unregister()
{
    UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkInputClient::Unregister."));
}

FFMODAudioLinkInputClient::FFMODAudioLinkInputClient(const FSharedBufferedOutputPtr& ToConsumeFrom, const UAudioLinkSettingsAbstract::FSharedSettingsProxyPtr& Settings, FName NameOfProducingSource)
    : WeakProducer(ToConsumeFrom)
    , Settings(Settings)
    , ProducerName(NameOfProducingSource)
{
    check(Settings.IsValid());
    Register(NameOfProducingSource);
    UnrealFormat = {};
}

FFMODAudioLinkInputClient::~FFMODAudioLinkInputClient()
{
    Unregister();
}

FMOD_RESULT F_CALL pcmreadcallback(FMOD_SOUND* inSound, void* data, unsigned int datalen)
{
    FMOD::Sound* sound = (FMOD::Sound*)inSound;
    FFMODAudioLinkInputClient* ConsumerSP;
    sound->getUserData((void**)&ConsumerSP);

    ConsumerSP->GetSamples((float*)data, datalen / sizeof(float));

    return FMOD_OK;
}

FMOD_RESULT F_CALL SoundCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* event, void* parameters)
{
    FMOD_RESULT result = FMOD_OK;
    FMOD::Studio::EventInstance* eventInstance = (FMOD::Studio::EventInstance*)event;

    if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)
    {
        InputClientRef* ClientRef;
        result = eventInstance->getUserData((void**)&ClientRef);

        FFMODAudioLinkInputClient* ConsumerPtr = &ClientRef->InputClient.Get();
        auto formatInfo = ConsumerPtr->GetFormat();

        FMOD::System* CoreSystem = nullptr;
        GetStudioSystem()->getCoreSystem(&CoreSystem);

        // Create sound info
        FMOD_CREATESOUNDEXINFO exinfo;
        memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
        exinfo.cbsize               = sizeof(FMOD_CREATESOUNDEXINFO);                                           /* Required. */
        exinfo.numchannels          = formatInfo->NumChannels;                                                  /* Number of channels in the sound. */
        exinfo.defaultfrequency     = formatInfo->NumSamplesPerSec;                                             /* Default playback rate of sound. */
        exinfo.decodebuffersize     = formatInfo->NumSamplesPerBlock / exinfo.numchannels;                      /* Chunk size of stream update in samples. Should match the FMOD System. */
        exinfo.length               = exinfo.defaultfrequency * exinfo.numchannels * sizeof(signed short) * 5;  /* Length of PCM data in bytes of whole song (for Sound::getLength) */
        exinfo.format               = FMOD_SOUND_FORMAT_PCMFLOAT;                                               /* Data format of sound. */
        exinfo.pcmreadcallback      = pcmreadcallback;                                                          /* User callback for reading. */
        exinfo.userdata             = ConsumerPtr;

        FMOD::Sound* ProgrammerSound = NULL;
        FString sourceName = ConsumerPtr->GetProducerName().ToString();
        result = CoreSystem->createSound(TCHAR_TO_ANSI(*sourceName), FMOD_OPENUSER | FMOD_CREATESTREAM, &exinfo, &ProgrammerSound);
        if (result != FMOD_OK)
        {
            UE_LOG(LogFMODAudioLink, Error, TEXT("CreateSound failed: %s , Result = %d."), *sourceName, result);
        }

        // Pass the sound to FMOD
        FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props = (FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*)parameters;
        props->sound = (FMOD_SOUND*)ProgrammerSound;
        props->subsoundIndex = -1;
        UE_LOG(LogFMODAudioLink, Verbose, TEXT("Sound Created: %s , Consumer = %p."), *sourceName, ConsumerPtr);
    }
    else if (type == FMOD_STUDIO_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)
    {
        // Obtain the sound
        FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props = (FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*)parameters;
        FMOD::Sound* sound = (FMOD::Sound*)props->sound;

        // Release the sound
        UE_LOG(LogFMODAudioLink, Verbose, TEXT("Sound Release: %p."), sound);
        result = sound->release();
    }
    else if (type == FMOD_STUDIO_EVENT_CALLBACK_DESTROYED)
    {
        InputClientRef* ClientRef = nullptr;
        result = eventInstance->getUserData((void**)&ClientRef);

        UE_LOG(LogFMODAudioLink, Verbose, TEXT("Event Destroyed: ClientRef = %p."), ClientRef);
        if (ClientRef)
        {
            delete ClientRef;
        }
    }

    return result;
}

void FFMODAudioLinkInputClient::Start(USceneComponent* InComponent)
{
    Stop();
    check(!IsLoadedHandle.IsValid());

    FFMODAudioLinkSettingsProxy* FMODSettings = static_cast<FFMODAudioLinkSettingsProxy*>(Settings.Get());
    const auto LinkEvent = FMODSettings->GetLinkEvent();

    auto SelfSP = AsShared();
    auto PlayLambda = [SelfSP, LinkEvent, InComponent]()
        {
            UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkInputClient::Start: SelfSP = %p, LinkEvent = %s, InComponent = %p.")
                    , &SelfSP, *LinkEvent.Get()->GetName(), &InComponent);

            FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(LinkEvent.Get());
            if (EventDesc != nullptr)
            {
                FMOD::Studio::EventInstance* EventInst = NULL;
                EventDesc->createInstance(&EventInst);
                SelfSP->EventInstance = EventInst;
                if (EventInst != nullptr)
                {
                    FTransform EventTransform = InComponent ? InComponent->GetComponentTransform() : FTransform();
                    FMOD_3D_ATTRIBUTES EventAttr = { { 0 } };
                    FMODUtils::Assign(EventAttr, EventTransform);
                    EventInst->set3DAttributes(&EventAttr);

                    EventInst->setCallback(SoundCallback, FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND | FMOD_STUDIO_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND | FMOD_STUDIO_EVENT_CALLBACK_DESTROYED);

                    InputClientRef* callbackMemory = new InputClientRef(SelfSP);

                    EventInst->setUserData(callbackMemory);

                    bool bIs3d = 0;
                    EventDesc->is3D(&bIs3d);
                    if (bIs3d)
                    {
                        // delay start
                        SelfSP->bShouldDelayStart = true;
                        UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkInputClient::Start: Delaying start of 3D EventInstance."));
                    }
                    else
                    {
                        SelfSP->bShouldDelayStart = false;
                        EventInst->start();
                    }
                }
            }
        };

    FMODSettings->IsEventDataLoaded() ? PlayLambda() : FMODSettings->RegisterCallback(PlayLambda, IsLoadedHandle);
}

void FFMODAudioLinkInputClient::Stop()
{
    if (EventInstance->isValid())
    {
        UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkInputClient::Stop: Stopping EventInstance."));
        bExitEarly = true;
        EventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
        EventInstance->release();
        EventInstance->setCallback(nullptr);
    }

    if (IsLoadedHandle.IsValid())
    {
        FFMODAudioLinkSettingsProxy* FMODSettings = static_cast<FFMODAudioLinkSettingsProxy*>(Settings.Get());
        check(FMODSettings);

        FMODSettings->UnRegisterCallback(IsLoadedHandle);
        IsLoadedHandle.Reset();
    }
}

void FFMODAudioLinkInputClient::UpdateWorldState(const FWorldState& InParams)
{
    if (EventInstance->isValid())
    {
        const FTransform& T = InParams.WorldTransform;
        FMOD_3D_ATTRIBUTES attr = { { 0 } };
        FMODUtils::Assign(attr, T);

        // TODO: velocity

        EventInstance->set3DAttributes(&attr);
        if (bShouldDelayStart)
        {
            EventInstance->start();
            UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkInputClient::UpdateWorldState: Starting EventInstance."));
            bShouldDelayStart = false;
        }
    }
}

bool FFMODAudioLinkInputClient::GetSamples(float* data, unsigned int dataLenSamples)
{
    FSharedBufferedOutputPtr StrongBufferProducer{ WeakProducer.Pin() };
    FMemory::Memset(data, 0, dataLenSamples);
    if (!StrongBufferProducer.IsValid())
    {
        // No audio source
        UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkInputClient::GetSamples: Producer is not valid, This=0x%p"), this);
        return false;
    }

    static const int NumStarvedBuffersBeforeStop = 5;
    int NumStarvedBuffersInARow = 0;

    int NumSamplesPopped = 0;
    int NumSamplesReceived = 0;
    bool bMoreDataRemaining = false;
    do 
    {
        bMoreDataRemaining = StrongBufferProducer->PopBuffer(data, dataLenSamples, NumSamplesPopped);
        NumSamplesReceived += NumSamplesPopped;

        UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkInputClient::GetSamples: (post-pop), SamplesPopped=%d, SamplesNeeded=%d, NumSamplesReceived=%d, MoreRemaining=%d This=0x%p"),
            NumSamplesPopped, dataLenSamples - NumSamplesPopped, NumSamplesReceived, bMoreDataRemaining, this);

        if (NumSamplesPopped <= 0)
        {
            NumStarvedBuffersInARow++;
        }
        else
        {
            NumStarvedBuffersInARow = 0;
        }

        if (!bMoreDataRemaining || bExitEarly || NumStarvedBuffersInARow > NumStarvedBuffersBeforeStop)
        {
            break;
        }
    }
    while (NumSamplesReceived < (int)dataLenSamples);

    return true;
}

IBufferedAudioOutput::FBufferFormat* FFMODAudioLinkInputClient::GetFormat()
{
    // Ensure we're still listening to a sub mix that exists.
    FSharedBufferedOutputPtr StrongPtr{ WeakProducer.Pin() };
    if (!StrongPtr.IsValid())
    {
        UE_LOG(LogFMODAudioLink, Verbose, TEXT("FMODAudioLinkInputClient::GetFormat: FSharedBufferedOutputPtr not valid."));
        return nullptr;
    }
    else if (UnrealFormat.NumChannels == 0)
    {
        ensure(StrongPtr->GetFormat(UnrealFormat));
    }

    return &UnrealFormat;
}

void FFMODAudioLinkInputClient::SetFormat(const IBufferedAudioOutput::FBufferFormat *AudioFormat)
{
    UnrealFormat.NumChannels = AudioFormat->NumChannels;
    UnrealFormat.NumSamplesPerBlock = AudioFormat->NumSamplesPerBlock;
    UnrealFormat.NumSamplesPerSec = AudioFormat->NumSamplesPerSec;
}