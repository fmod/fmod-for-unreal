// Copyright (c), Firelight Technologies Pty, Ltd. 2025-2025.

#include "FMODAudioLinkFactory.h"
#include "FMODAudioLinkSynchronizer.h"
#include "FMODAudioLinkSourcePushed.h"
#include "FMODAudioLinkSettings.h"
#include "FMODAudioLinkLog.h"
#include "FMODAudioLinkComponent.h"
#include "FMODStudioModule.h"

#include "Async/Async.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Sound/SoundSubmix.h"
#include "Templates/SharedPointer.h"
#include "AudioDevice.h"

bool FFMODAudioLinkFactory::bHasSubmix = false;

FName FFMODAudioLinkFactory::GetFactoryNameStatic()
{
    static const FName FactoryName(TEXT("FMOD"));
    return FactoryName;
}

FName FFMODAudioLinkFactory::GetFactoryName() const
{
	return GetFactoryNameStatic();
}

TSubclassOf<UAudioLinkSettingsAbstract> FFMODAudioLinkFactory::GetSettingsClass() const
{
	return UFMODAudioLinkSettings::StaticClass();
}

TUniquePtr<IAudioLink> FFMODAudioLinkFactory::CreateSubmixAudioLink(const FAudioLinkSubmixCreateArgs& InArgs)
{
    if (!IFMODStudioModule::IsAvailable())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSubmixAudioLink: No FMODStudio module."));
        return {};
    }

    if (!InArgs.Settings.IsValid())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSubmixAudioLink: Invalid FMODAudioLinkSettings."));
        return {};
    }

    if (!InArgs.Submix.IsValid())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSubmixAudioLink: Invalid Submix."));
        return {};
    }

    UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkFactory::CreateSubmixAudioLink: Creating AudioLink %s for Submix %s."), *InArgs.Settings->GetName(), *InArgs.Submix->GetName());
    bHasSubmix = true;

    // Downcast to settings proxy
    const FSharedFMODAudioLinkSettingsProxyPtr FMODSettingsSP = InArgs.Settings->GetCastProxy<FFMODAudioLinkSettingsProxy>();

    // Make buffer listener first, which is our producer.
    IAudioLinkFactory::FSubmixBufferListenerCreateParams SubmixListenerCreateArgs;
    SubmixListenerCreateArgs.SizeOfBufferInFrames = FMODSettingsSP->GetReceivingBufferSizeInFrames();
    SubmixListenerCreateArgs.bShouldZeroBuffer = FMODSettingsSP->ShouldClearBufferOnReceipt();
    FSharedBufferedOutputPtr ProducerSP = CreateSubmixBufferListener(SubmixListenerCreateArgs);

    // Create consumer.
    FSharedFMODAudioLinkInputClientPtr ConsumerSP = MakeShared<FFMODAudioLinkInputClient, ESPMode::ThreadSafe>(
        ProducerSP, InArgs.Settings->GetProxy(), InArgs.Submix->GetFName());

    // Setup a delegate to establish the link when we know the format.
    ProducerSP->SetFormatKnownDelegate(
        IBufferedAudioOutput::FOnFormatKnown::CreateLambda(
            [ProducerSP, ConsumerSP, FMODSettingsSP](const IBufferedAudioOutput::FBufferFormat& InFormat)
            {
                // Unreal uses samples for 'Channels x samples' and frames for 'samples'
                int32 BufferSizeInChannelSamples = FMODSettingsSP->GetReceivingBufferSizeInFrames() * InFormat.NumChannels;
                int32 ReserveSizeInChannelSamples = (float)BufferSizeInChannelSamples * FMODSettingsSP->GetProducerConsumerBufferRatio();
                int32 SilenceToAddToFirstBuffer = FMath::Min((float)BufferSizeInChannelSamples * FMODSettingsSP->GetInitialSilenceFillRatio(), ReserveSizeInChannelSamples);

                ProducerSP->Reserve(ReserveSizeInChannelSamples, SilenceToAddToFirstBuffer);

                ConsumerSP->Stop();

                // Start the FMOD input object.
                UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkFactory::CreateSubmixAudioLink: Start consumer."));
                ConsumerSP->Start();
            }));

    // Start producer.
    UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkFactory::CreateSubmixAudioLink: Start producer."));
    ProducerSP->Start(InArgs.Device);

    // Build a link, which owns both the consumer and producer.
    return MakeUnique<FFMODAudioLink>(ProducerSP, ConsumerSP);
}

TUniquePtr<IAudioLink> FFMODAudioLinkFactory::CreateSourceAudioLink(const FAudioLinkSourceCreateArgs& InArgs)
{
    if (!IFMODStudioModule::IsAvailable())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: No FMODStudio module."));
        return {};
    }

    if (!InArgs.Settings.IsValid())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Invalid FMODAudioLinkSettings."));
        return {};
    }

    if (!InArgs.OwningComponent.IsValid())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Invalid Owning Component."));
        return {};
    }

    if (!InArgs.AudioComponent.IsValid())
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Invalid Audio Component."));
        return {};
    }

    const UWorld* World = InArgs.OwningComponent->GetWorld();
    if (UNLIKELY(!IsValid(World)))
    {
        UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Invalid World in Owning Component."));
        return {};
    }

    const FAudioDeviceHandle Handle = World->GetAudioDevice();

    // Downcast to settings proxy.
    const FSharedFMODAudioLinkSettingsProxyPtr FMODSettingsSP = InArgs.Settings->GetCastProxy<FFMODAudioLinkSettingsProxy>();

    // Make buffer listener first, which is our producer.
    FSourceBufferListenerCreateParams SourceBufferCreateArgs;
    SourceBufferCreateArgs.SizeOfBufferInFrames = FMODSettingsSP->GetReceivingBufferSizeInFrames();
    SourceBufferCreateArgs.bShouldZeroBuffer = true;
    SourceBufferCreateArgs.OwningComponent = InArgs.OwningComponent;
    SourceBufferCreateArgs.AudioComponent = InArgs.AudioComponent;
    FSharedBufferedOutputPtr ProducerSP = CreateSourceBufferListener(SourceBufferCreateArgs);

    static const FName UnknownOwner(TEXT("Unknown"));
    FName OwnerName = InArgs.OwningComponent.IsValid() ? InArgs.OwningComponent->GetFName() : UnknownOwner;

    // Create consumer.
    FSharedFMODAudioLinkInputClientPtr ConsumerSP = MakeShared<FFMODAudioLinkInputClient, ESPMode::ThreadSafe>(ProducerSP, FMODSettingsSP, OwnerName);

    ProducerSP->SetFormatKnownDelegate(
        IBufferedAudioOutput::FOnFormatKnown::CreateLambda(
            [ProducerSP, ConsumerSP, FMODSettingsSP, WeakThis = InArgs.OwningComponent](const IBufferedAudioOutput::FBufferFormat& InFormat)
            {
                // Unreal uses samples for 'Channels x samples' and frames for 'samples'
                int32 BufferSizeInChannelSamples = FMODSettingsSP->GetReceivingBufferSizeInFrames() * InFormat.NumChannels;
                int32 ReserveSizeInChannelSamples = (float)BufferSizeInChannelSamples * FMODSettingsSP->GetProducerConsumerBufferRatio();
                int32 SilenceToAddToFirstBuffer = FMath::Min((float)BufferSizeInChannelSamples * FMODSettingsSP->GetInitialSilenceFillRatio(), ReserveSizeInChannelSamples);


                // Set circular buffer ahead of first buffer.
                ProducerSP->Reserve(ReserveSizeInChannelSamples, SilenceToAddToFirstBuffer);

                AsyncTask(ENamedThreads::GameThread, [ConsumerSP, WeakThis]()
                    {
                        if (WeakThis.IsValid())
                        {
                            // Stop ahead of starting to play. This might not be necessary for submixes, but in case we get a format change.
                            // As our link can remain open, stop anything playing on a format change.
                            // This won't do anything if we're already stopped.
                            ConsumerSP->Stop();

                            // Start the FMOD input object.
                            UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Start consumer."));
                            ConsumerSP->Start(Cast<UFMODAudioLinkComponent>(WeakThis.Get()));
                        }
                    });
            }));
    ProducerSP->SetBufferStreamEndDelegate(
        IBufferedAudioOutput::FOnBufferStreamEnd::CreateLambda(
            [ConsumerSP](const IBufferedAudioOutput::FBufferStreamEnd&)
            {
                UE_LOG(LogFMODAudioLink, Verbose, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Stop consumer."));
                ConsumerSP->Stop();
            }));

    // Tell the Producer to Start receiving buffers from Sources.
    // Pass a Lambda to do the some work when we know the Format, which starts FMOD up.
    UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkFactory::CreateSourceAudioLink: Start producer."));
    ProducerSP->Start(Handle ? Handle.GetAudioDevice() : nullptr);

    // Make the link.
    return MakeUnique<FFMODAudioLink>(ProducerSP, ConsumerSP);
}

IAudioLinkFactory::FAudioLinkSourcePushedSharedPtr FFMODAudioLinkFactory::CreateSourcePushedAudioLink(const FAudioLinkSourcePushedCreateArgs& InArgs)
{
    if (IFMODStudioModule::IsAvailable())
    {
        UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkFactory::CreateSourcePushedAudioLink: Create AudioLink SourcePushed."));
        return MakeShared<FFMODAudioLinkSourcePushed, ESPMode::ThreadSafe>(InArgs,this);
    }
    UE_LOG(LogFMODAudioLink, Error, TEXT("FFMODAudioLinkFactory::CreateSourcePushedAudioLink: IFMODStudioModule not available."));
    return nullptr;
}

IAudioLinkFactory::FAudioLinkSynchronizerSharedPtr FFMODAudioLinkFactory::CreateSynchronizerAudioLink()
{
    UE_LOG(LogFMODAudioLink, VeryVerbose, TEXT("FFMODAudioLinkFactory::CreateSynchronizerAudioLink: Create AudioLink Synchronizer."));
    auto SynchronizerSP = MakeShared<FFMODAudioLinkSynchronizer, ESPMode::ThreadSafe>();
    return SynchronizerSP;
}
