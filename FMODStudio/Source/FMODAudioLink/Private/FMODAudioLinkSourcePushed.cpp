// Copyright (c), Firelight Technologies Pty, Ltd. 2023-2026.

#include "FMODAudioLinkSourcePushed.h"
#include "FMODAudioLinkSettings.h"
#include "FMODAudioLinkInputClient.h"
#include "FMODAudioLinkLog.h"

#include "Async/Async.h"

#include "FMODEvent.h"

FFMODAudioLinkSourcePushed::FFMODAudioLinkSourcePushed(const IAudioLinkFactory::FAudioLinkSourcePushedCreateArgs& InArgs, IAudioLinkFactory* InFactory)
    : CreateArgs(InArgs)
{
    const FFMODAudioLinkSettingsProxy* FMODSettingsSP = static_cast<FFMODAudioLinkSettingsProxy*>(InArgs.Settings.Get());

    IAudioLinkFactory::FPushedBufferListenerCreateParams Params;
    Params.SizeOfBufferInFrames = CreateArgs.NumFramesPerBuffer;
    Params.bShouldZeroBuffer = FMODSettingsSP->ShouldClearBufferOnReceipt();

    ProducerSP = InFactory->CreatePushableBufferListener(Params);
    ConsumerSP = MakeShared<FFMODAudioLinkInputClient, ESPMode::ThreadSafe>(ProducerSP, CreateArgs.Settings, CreateArgs.OwnerName);

    // Unreal uses samples for 'Channels x samples' and frames for 'samples'
    int32 BufferSizeInChannelSamples = FMODSettingsSP->GetReceivingBufferSizeInFrames() * InArgs.NumChannels;
    int32 ReserveSizeInChannelSamples = (float)BufferSizeInChannelSamples * FMODSettingsSP->GetProducerConsumerBufferRatio();
    int32 SilenceToAddToFirstBuffer = FMath::Min((float)BufferSizeInChannelSamples * FMODSettingsSP->GetInitialSilenceFillRatio(), ReserveSizeInChannelSamples);

    // Start the FMOD input object.
    IBufferedAudioOutput::FBufferFormat format = {};
    format.NumSamplesPerBlock = BufferSizeInChannelSamples;
    format.NumChannels = InArgs.NumChannels;
    format.NumSamplesPerSec = InArgs.SampleRate;

    ConsumerSP->SetFormat(&format);

    // Set circular buffer ahead of first buffer.
    ProducerSP->Reserve(ReserveSizeInChannelSamples, SilenceToAddToFirstBuffer);

    ProducerSP->Start(nullptr);

    UE_LOG(LogFMODAudioLink, Verbose,
        TEXT("FFMODAudioLinkSourcePushed::Ctor() Name=%s, Producer=0x%p, Consumer=0x%p, p2c%%=%2.2f, PlayEvent=%s, TotalFramesForSource=%d, This=0x%p"),
        *CreateArgs.OwnerName.GetPlainNameString(), ProducerSP.Get(),
            ConsumerSP.Get(), FMODSettingsSP->GetProducerConsumerBufferRatio(), *FMODSettingsSP->GetLinkEvent()->GetName(), 
            CreateArgs.TotalNumFramesInSource, this);
}

FFMODAudioLinkSourcePushed::~FFMODAudioLinkSourcePushed()
{
    UE_LOG(LogFMODAudioLink, Verbose,
        TEXT("FFMODAudioLinkSourcePushed::Dtor() Name=%s, Producer=0x%p, Consumer=0x%p, RecievedFrames=%d/%d, This=0x%p"),
        *CreateArgs.OwnerName.GetPlainNameString(), ProducerSP.Get(), ConsumerSP.Get(), NumFramesReceivedSoFar, 
        CreateArgs.TotalNumFramesInSource,this);

    if (ConsumerSP.IsValid())
    {
        ConsumerSP->Stop();
    }
}

void FFMODAudioLinkSourcePushed::OnNewBuffer(const FOnNewBufferParams& InArgs)
{
    UE_LOG(LogFMODAudioLink, VeryVerbose,
        TEXT("FFMODAudioLinkSourcePushed::OnNewBuffer() Name=%s, Producer=0x%p, Consumer=0x%p, SourceID=%d, RecievedFrames=%d/%d, NumSamples=%d, This=0x%p"),
        *CreateArgs.OwnerName.GetPlainNameString(), ProducerSP.Get(), ConsumerSP.Get(), SourceId, NumFramesReceivedSoFar, 
        CreateArgs.TotalNumFramesInSource, InArgs.Buffer.Num(), this);

    if (SourceId == INDEX_NONE)
    {
        SourceId = InArgs.SourceId;
        ConsumerSP->Start();
    }
    IPushableAudioOutput* Pushable = ProducerSP->GetPushableInterface();
    if (ensure(Pushable))
    {
        IPushableAudioOutput::FOnNewBufferParams Params;
        Params.AudioData = InArgs.Buffer.GetData();
        Params.NumSamples = InArgs.Buffer.Num();
        Params.Id = InArgs.SourceId;
        Params.NumChannels = CreateArgs.NumChannels;
        Params.SampleRate = CreateArgs.SampleRate;
        Pushable->PushNewBuffer(Params);

        NumFramesReceivedSoFar += Params.NumSamples;
        NumFramesReceivedSoFar %= CreateArgs.TotalNumFramesInSource;
    }
}

void FFMODAudioLinkSourcePushed::OnSourceDone(const int32 InSourceId)
{
    UE_LOG(LogFMODAudioLink, Verbose,
        TEXT("FFMODAudioLinkSourcePushed::OnSourceDone() Name=%s, Producer=0x%p, Consumer=0x%p, RecievedFrames=%d/%d, This=0x%p"),
        *CreateArgs.OwnerName.GetPlainNameString(), ProducerSP.Get(), ConsumerSP.Get(), NumFramesReceivedSoFar, 
        CreateArgs.TotalNumFramesInSource, this);

    check(SourceId == InSourceId);
    IPushableAudioOutput* Pushable = ProducerSP->GetPushableInterface();
    if (ensure(Pushable))
    {
        Pushable->LastBuffer(SourceId);
    }
    SourceId = INDEX_NONE;
}

void FFMODAudioLinkSourcePushed::OnSourceReleased(const int32 InSourceId)
{
    UE_LOG(LogFMODAudioLink, Verbose,
        TEXT("FFMODAudioLinkSourcePushed::OnSourceReleased() Name=%s, Producer=0x%p, Consumer=0x%p, RecievedFrames=%d/%d, This=0x%p"),
        *CreateArgs.OwnerName.GetPlainNameString(), ProducerSP.Get(), ConsumerSP.Get(), NumFramesReceivedSoFar, 
        CreateArgs.TotalNumFramesInSource,this);
}

// Called by the AudioThread, not the AudioRenderThread
void FFMODAudioLinkSourcePushed::OnUpdateWorldState(const FOnUpdateWorldStateParams& InParams)
{
    FFMODAudioLinkInputClient::FWorldState UpdateParams;
    UpdateParams.WorldTransform = InParams.WorldTransform;
    ConsumerSP->UpdateWorldState(UpdateParams);
}