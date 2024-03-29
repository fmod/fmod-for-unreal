
#include "FMODNiagaraEventPlayer.h"

#include "FMODBlueprintStatics.h"
#include "FMODSettings.h"

#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "Internationalization/Internationalization.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraWorldManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogFMODNiagara, Log, All);

const FName UFMODNiagaraEventPlayer::PlayAudioName(TEXT("PlayEventAtLocation"));
const FName UFMODNiagaraEventPlayer::PlayPersistentAudioName(TEXT("PlayPersistentEvent"));
const FName UFMODNiagaraEventPlayer::SetPersistentAudioLocationName(TEXT("UpdateEventLocation"));
const FName UFMODNiagaraEventPlayer::SetPersistentAudioRotationName(TEXT("UpdateEventRotation"));
const FName UFMODNiagaraEventPlayer::SetPersistentAudioFloatParamName(TEXT("SetFloatParameter"));
const FName UFMODNiagaraEventPlayer::PausePersistentAudioName(TEXT("SetPaused"));

class FNiagaraWorldManager;
/**
Async task to play the audio on the game thread and isolate from the niagara tick
*/
class FNiagaraAudioPlayerAsyncTask
{
    TWeakObjectPtr<UFMODEvent> WeakSound;
    TArray<FEventParticleData> Data;
    TWeakObjectPtr<UWorld> WeakWorld;

public:
    FNiagaraAudioPlayerAsyncTask(TWeakObjectPtr<UFMODEvent> InSound, TArray<FEventParticleData>& Data, TWeakObjectPtr<UWorld> InWorld)
        : WeakSound(InSound)
        , Data(Data)
        , WeakWorld(InWorld)
    {
    }

    FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraAudioPlayerAsyncTask, STATGROUP_TaskGraphTasks); }
    static FORCEINLINE ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
    static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }

    void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
    {
        UWorld* World = WeakWorld.Get();
        if (World == nullptr)
        {
            UE_LOG(LogFMODNiagara, Warning, TEXT("Invalid world reference in event player, skipping play"));
            return;
        }

        UFMODEvent* Sound = WeakSound.Get();
        if (Sound == nullptr)
        {
            UE_LOG(LogFMODNiagara, Warning, TEXT("Invalid sound reference in event player, skipping play"));
            return;
        }

        for (const FEventParticleData& ParticleData : Data)
        {
            UFMODBlueprintStatics::PlayEventAtLocation(World, Sound, FTransform(ParticleData.Rotation, ParticleData.Position), true);
        }
    }
};

UFMODNiagaraEventPlayer::UFMODNiagaraEventPlayer(FObjectInitializer const& ObjectInitializer)
    : Super(ObjectInitializer)
{
    EventToPlay = nullptr;
    bLimitPlaysPerTick = true;
    MaxPlaysPerTick = 10;
}

void UFMODNiagaraEventPlayer::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
        FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
    }
}

bool UFMODNiagaraEventPlayer::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    FEventPlayerInterface_InstanceData* PIData = new (PerInstanceData) FEventPlayerInterface_InstanceData;
    if (bLimitPlaysPerTick)
    {
        PIData->MaxPlaysPerTick = MaxPlaysPerTick;
    }
    PIData->bStopWhenComponentIsDestroyed = bStopWhenComponentIsDestroyed;
#if WITH_EDITORONLY_DATA
    PIData->bOnlyActiveDuringGameplay = bOnlyActiveDuringGameplay;
#endif
    return true;
}

#if WITH_EDITOR
void UFMODNiagaraEventPlayer::PostEditChangeProperty(FPropertyChangedEvent& e)
{
    FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UFMODNiagaraEventPlayer, EventToPlay))
    {
        ParameterNames.Empty();
        bDefaultParameterValuesCached = false;
    }
    CacheDefaultParameterValues();

    Super::PostEditChangeProperty(e);
}

void UFMODNiagaraEventPlayer::CacheDefaultParameterValues()
{
    if (EventToPlay)
    {
        if (!bDefaultParameterValuesCached)
        {
            TArray<FMOD_STUDIO_PARAMETER_DESCRIPTION> ParameterDescriptions;
            EventToPlay->GetParameterDescriptions(ParameterDescriptions);
            for (const FMOD_STUDIO_PARAMETER_DESCRIPTION& ParameterDescription : ParameterDescriptions)
            {
                if (ShouldCacheParameter(ParameterDescription))
                {
                    ParameterNames.Add(ParameterDescription.name);
                }
            }
            bDefaultParameterValuesCached = true;
        }
    }
}

bool UFMODNiagaraEventPlayer::ShouldCacheParameter(const FMOD_STUDIO_PARAMETER_DESCRIPTION& ParameterDescription)
{
    const UFMODSettings& Settings = *GetDefault<UFMODSettings>();

    if (((ParameterDescription.flags & FMOD_STUDIO_PARAMETER_GLOBAL) == 0) &&
        (ParameterDescription.type == FMOD_STUDIO_PARAMETER_GAME_CONTROLLED) &&
        ParameterDescription.name != Settings.OcclusionParameter &&
        ParameterDescription.name != Settings.AmbientVolumeParameter &&
        ParameterDescription.name != Settings.AmbientLPFParameter)
    {
        return true;
    }
    return false;
}
#endif // WITH_EDITOR

void UFMODNiagaraEventPlayer::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    FEventPlayerInterface_InstanceData* InstData = (FEventPlayerInterface_InstanceData*)PerInstanceData;
    for (const auto& Entry : InstData->PersistentAudioMapping)
    {
        if (Entry.Value.IsValid())
        {
            Entry.Value->Stop();
        }
    }
    InstData->~FEventPlayerInterface_InstanceData();
}

bool UFMODNiagaraEventPlayer::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
    FEventPlayerInterface_InstanceData* PIData = (FEventPlayerInterface_InstanceData*)PerInstanceData;
    if (!PIData)
    {
        return true;
    }

    if (IsValid(EventToPlay) && SystemInstance)
    {
        PIData->EventToPlay = EventToPlay;
        PIData->ParameterNames = ParameterNames;
    }
    else
    {
        PIData->EventToPlay.Reset();
        PIData->ParameterNames.Reset();
    }

    return false;
}

bool UFMODNiagaraEventPlayer::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
    FEventPlayerInterface_InstanceData* PIData = (FEventPlayerInterface_InstanceData*)PerInstanceData;
    UNiagaraSystem* System = SystemInstance->GetSystem();
    UWorld* World = SystemInstance->GetWorld();

#if WITH_EDITORONLY_DATA
    if (World->HasBegunPlay() == false && PIData->bOnlyActiveDuringGameplay)
    {
        PIData->PlayAudioQueue.Empty();
        PIData->PersistentAudioMapping.Empty();
        return false;
    }
#endif

    if (!PIData->PlayAudioQueue.IsEmpty() && System)
    {
        //Drain the queue into an array here
        TArray<FEventParticleData> Data;
        FEventParticleData Value;
        while (PIData->PlayAudioQueue.Dequeue(Value))
        {
            Data.Add(Value);
            if (PIData->MaxPlaysPerTick > 0 && Data.Num() >= PIData->MaxPlaysPerTick)
            {
                // discard the rest of the queue if over the tick limit
                PIData->PlayAudioQueue.Empty();
                break;
            }
        }
        TGraphTask<FNiagaraAudioPlayerAsyncTask>::CreateTask().ConstructAndDispatchWhenReady(PIData->EventToPlay, Data, SystemInstance->GetWorld());
    }

    // process the persistent audio updates
    FPersistentEventParticleData Value;
    while (PIData->PersistentAudioActionQueue.Dequeue(Value))
    {
        UFMODAudioComponent* AudioComponent = nullptr;
        if (Value.AudioHandle > 0)
        {
            auto MappedValue = PIData->PersistentAudioMapping.Find(Value.AudioHandle);
            if (MappedValue && MappedValue->IsValid())
            {
                AudioComponent = MappedValue->Get();
            }
        }
        // since we are in the game thread here, it is safe for the callback to access the audio component
        if (Value.UpdateCallback)
        {
            Value.UpdateCallback(PIData, AudioComponent, SystemInstance);
        }
    }
    return false;
}

bool UFMODNiagaraEventPlayer::Equals(const UNiagaraDataInterface* Other) const
{
    if (!Super::Equals(Other))
    {
        return false;
    }

    const UFMODNiagaraEventPlayer* OtherPlayer = CastChecked<UFMODNiagaraEventPlayer>(Other);
    return OtherPlayer->EventToPlay == EventToPlay && OtherPlayer->bLimitPlaysPerTick == bLimitPlaysPerTick && OtherPlayer->MaxPlaysPerTick == MaxPlaysPerTick;
}

void UFMODNiagaraEventPlayer::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    FNiagaraFunctionSignature Sig;
    Sig.Name = PlayAudioName;
#if WITH_EDITORONLY_DATA
    Sig.Description = NSLOCTEXT("FMODStudio", "PlayEventFunctionDescription", "This function plays an event at the given location after the simulation has ticked.");
    Sig.ExperimentalMessage = NSLOCTEXT("FMODStudio", "PlayEventFunctionExperimental", "The return value of the event function call currently needs to be wired to a particle parameter, because otherwise it will be removed by the compiler.");
#endif
    Sig.bMemberFunction = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsGPU = false;
    Sig.bExperimental = true;
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Event Player")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Play Event")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("PositionWS")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("RotationWS")));
    Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")));
    OutFunctions.Add(Sig);

    Sig = FNiagaraFunctionSignature();
    Sig.Name = PlayPersistentAudioName;
#if WITH_EDITORONLY_DATA
    Sig.Description = NSLOCTEXT("FMODStudio", "PlayPersistentEventFunctionDescription", "This function plays an Event at the given location after the simulation has ticked. The returned handle can be used to control the sound in subsequent ticks.");
#endif
    Sig.bMemberFunction = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsGPU = false;
    Sig.bRequiresExecPin = true;
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Event Player")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Play Event")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Existing Event Handle")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position WS")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Rotation WS")));
    Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Event Handle")));
    OutFunctions.Add(Sig);

    Sig = FNiagaraFunctionSignature();
    Sig.Name = SetPersistentAudioFloatParamName;
#if WITH_EDITORONLY_DATA
    Sig.Description = NSLOCTEXT("FMODStudio", "SetPersistentEventFloatParamFunctionDescription", "If an active event effect can be found for the given handle then the given sound cue parameter will be set on it.");
#endif
    Sig.bMemberFunction = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsGPU = false;
    Sig.bRequiresExecPin = true;
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Event Player")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Event Handle")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Parameter Name Index")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Parameter Value")));
    OutFunctions.Add(Sig);

    Sig = FNiagaraFunctionSignature();
    Sig.Name = SetPersistentAudioLocationName;
#if WITH_EDITORONLY_DATA
    Sig.Description = NSLOCTEXT("FMODStudio", "SetPersistentEventLocationFunctionDescription", "If an active event effect can be found for the given handle then the this will adjusts its world position.");
#endif
    Sig.bMemberFunction = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsGPU = false;
    Sig.bRequiresExecPin = true;
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Event Player")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Event Handle")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position WS")));
    OutFunctions.Add(Sig);

    Sig = FNiagaraFunctionSignature();
    Sig.Name = SetPersistentAudioRotationName;
#if WITH_EDITORONLY_DATA
    Sig.Description = NSLOCTEXT("FMODStudio", "SetPersistentEventRotationFunctionDescription", "If an active event effect can be found for the given handle then the this will adjusts its rotation in the world.");
#endif
    Sig.bMemberFunction = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsGPU = false;
    Sig.bRequiresExecPin = true;
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Event Player")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Event Handle")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Rotation WS")));
    OutFunctions.Add(Sig);

    Sig = FNiagaraFunctionSignature();
    Sig.Name = PausePersistentAudioName;
#if WITH_EDITORONLY_DATA
    Sig.Description = NSLOCTEXT("FMODStudio", "SetPersistentEventPausedDescription", "If an active event effect can be found for the given handle then the this will either pause or unpause the effect.");
#endif
    Sig.bMemberFunction = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsGPU = false;
    Sig.bRequiresExecPin = true;
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Event Player")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Event Handle")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Pause Event")));
    OutFunctions.Add(Sig);
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UFMODNiagaraEventPlayer, PlayOneShotAudio);
DEFINE_NDI_DIRECT_FUNC_BINDER(UFMODNiagaraEventPlayer, PlayPersistentAudio);
DEFINE_NDI_DIRECT_FUNC_BINDER(UFMODNiagaraEventPlayer, SetParameterFloat);
DEFINE_NDI_DIRECT_FUNC_BINDER(UFMODNiagaraEventPlayer, UpdateLocation);
DEFINE_NDI_DIRECT_FUNC_BINDER(UFMODNiagaraEventPlayer, UpdateRotation);
DEFINE_NDI_DIRECT_FUNC_BINDER(UFMODNiagaraEventPlayer, SetPausedState);
void UFMODNiagaraEventPlayer::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
    if (BindingInfo.Name == PlayAudioName)
    {
        NDI_FUNC_BINDER(UFMODNiagaraEventPlayer, PlayOneShotAudio)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == PlayPersistentAudioName)
    {
        NDI_FUNC_BINDER(UFMODNiagaraEventPlayer, PlayPersistentAudio)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == SetPersistentAudioFloatParamName)
    {
        NDI_FUNC_BINDER(UFMODNiagaraEventPlayer, SetParameterFloat)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == SetPersistentAudioLocationName)
    {
        NDI_FUNC_BINDER(UFMODNiagaraEventPlayer, UpdateLocation)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == SetPersistentAudioRotationName)
    {
        NDI_FUNC_BINDER(UFMODNiagaraEventPlayer, UpdateRotation)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == PausePersistentAudioName)
    {
        NDI_FUNC_BINDER(UFMODNiagaraEventPlayer, SetPausedState)::Bind(this, OutFunc);
    }
    else
    {
        UE_LOG(LogFMODNiagara, Error, TEXT("Could not find event player external function. Expected Name: %s  Actual Name: %s"), *PlayAudioName.ToString(), *BindingInfo.Name.ToString());
    }
}

void UFMODNiagaraEventPlayer::SetParameterFloat(FVectorVMContext& Context)
{
    VectorVM::FUserPtrHandler<FEventPlayerInterface_InstanceData> InstData(Context);

    FNDIInputParam<int32> AudioHandleInParam(Context);
    FNDIInputParam<int32> NameIndexParam(Context);
    FNDIInputParam<float> ValueParam(Context);
    checkfSlow(InstData.Get(), TEXT("Event player has invalid instance data. %s"), *GetPathName());

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
        int32 Handle = AudioHandleInParam.GetAndAdvance();
        int32 NameIndex = NameIndexParam.GetAndAdvance();
        float Value = ValueParam.GetAndAdvance();

        if (Handle > 0 && InstData->ParameterNames.IsValidIndex(NameIndex))
        {
            FName ParameterName = InstData->ParameterNames[NameIndex];
            FPersistentEventParticleData AudioData;
            AudioData.AudioHandle = Handle;
            AudioData.UpdateCallback = [ParameterName, Value](FEventPlayerInterface_InstanceData*, UFMODAudioComponent* AudioComponent, FNiagaraSystemInstance*)
            {
                if (AudioComponent && AudioComponent->IsPlaying())
                {
                    AudioComponent->SetParameter(ParameterName, Value);
                }
            };
            InstData->PersistentAudioActionQueue.Enqueue(AudioData);
        }
    }
}

void UFMODNiagaraEventPlayer::UpdateLocation(FVectorVMContext& Context)
{
    VectorVM::FUserPtrHandler<FEventPlayerInterface_InstanceData> InstData(Context);

    FNDIInputParam<int32> AudioHandleInParam(Context);
    FNDIInputParam<FVector> LocationParam(Context);
    checkfSlow(InstData.Get(), TEXT("Audio player interface has invalid instance data. %s"), *GetPathName());

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
        int32 Handle = AudioHandleInParam.GetAndAdvance();
        FVector Location = LocationParam.GetAndAdvance();

        if (Handle > 0)
        {
            FPersistentEventParticleData AudioData;
            AudioData.AudioHandle = Handle;
            AudioData.UpdateCallback = [Location](FEventPlayerInterface_InstanceData*, UFMODAudioComponent* AudioComponent, FNiagaraSystemInstance*)
            {
                if (AudioComponent && AudioComponent->IsPlaying())
                {
                    AudioComponent->SetWorldLocation(Location);
                }
            };
            InstData->PersistentAudioActionQueue.Enqueue(AudioData);
        }
    }
}

void UFMODNiagaraEventPlayer::UpdateRotation(FVectorVMContext& Context)
{
    VectorVM::FUserPtrHandler<FEventPlayerInterface_InstanceData> InstData(Context);

    FNDIInputParam<int32> AudioHandleInParam(Context);
    FNDIInputParam<FVector> RotationParam(Context);
    checkfSlow(InstData.Get(), TEXT("Event player has invalid instance data. %s"), *GetPathName());

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
        int32 Handle = AudioHandleInParam.GetAndAdvance();
        FVector Rotation = RotationParam.GetAndAdvance();

        if (Handle > 0)
        {
            FPersistentEventParticleData AudioData;
            AudioData.AudioHandle = Handle;
            AudioData.UpdateCallback = [Rotation](FEventPlayerInterface_InstanceData*, UFMODAudioComponent* AudioComponent, FNiagaraSystemInstance*)
            {
                if (AudioComponent && AudioComponent->IsPlaying())
                {
                    FRotator NewRotator(Rotation.X, Rotation.Y, Rotation.Z);
                    AudioComponent->SetWorldRotation(NewRotator);
                }
            };
            InstData->PersistentAudioActionQueue.Enqueue(AudioData);
        }
    }
}

void UFMODNiagaraEventPlayer::SetPausedState(FVectorVMContext& Context)
{
    VectorVM::FUserPtrHandler<FEventPlayerInterface_InstanceData> InstData(Context);

    FNDIInputParam<int32> AudioHandleInParam(Context);
    FNDIInputParam<FNiagaraBool> PausedParam(Context);
    checkfSlow(InstData.Get(), TEXT("Event player has invalid instance data. %s"), *GetPathName());

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
        int32 Handle = AudioHandleInParam.GetAndAdvance();
        bool IsPaused = PausedParam.GetAndAdvance();

        if (Handle > 0)
        {
            FPersistentEventParticleData AudioData;
            AudioData.AudioHandle = Handle;
            AudioData.UpdateCallback = [IsPaused](FEventPlayerInterface_InstanceData*, UFMODAudioComponent* AudioComponent, FNiagaraSystemInstance*)
            {
                if (AudioComponent)
                {
                    AudioComponent->SetPaused(IsPaused);
                }
            };
            InstData->PersistentAudioActionQueue.Enqueue(AudioData);
        }
    }
}

void UFMODNiagaraEventPlayer::PlayOneShotAudio(FVectorVMContext& Context)
{
    VectorVM::FUserPtrHandler<FEventPlayerInterface_InstanceData> InstData(Context);

    VectorVM::FExternalFuncInputHandler<FNiagaraBool> PlayDataParam(Context);

    VectorVM::FExternalFuncInputHandler<float> PositionParamX(Context);
    VectorVM::FExternalFuncInputHandler<float> PositionParamY(Context);
    VectorVM::FExternalFuncInputHandler<float> PositionParamZ(Context);

    VectorVM::FExternalFuncInputHandler<float> RotationParamX(Context);
    VectorVM::FExternalFuncInputHandler<float> RotationParamY(Context);
    VectorVM::FExternalFuncInputHandler<float> RotationParamZ(Context);

    VectorVM::FExternalFuncRegisterHandler<FNiagaraBool> OutSample(Context);

    checkfSlow(InstData.Get(), TEXT("Event player has invalid instance data. %s"), *GetPathName());
    bool ValidSoundData = InstData->EventToPlay.IsValid();

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
        FNiagaraBool ShouldPlay = PlayDataParam.GetAndAdvance();
        FEventParticleData Data;
        Data.Position = FVector(PositionParamX.GetAndAdvance(), PositionParamY.GetAndAdvance(), PositionParamZ.GetAndAdvance());
        Data.Rotation = FRotator(RotationParamX.GetAndAdvance(), RotationParamY.GetAndAdvance(), RotationParamZ.GetAndAdvance());

        FNiagaraBool Valid;
        if (ValidSoundData && ShouldPlay)
        {
            Valid.SetValue(InstData->PlayAudioQueue.Enqueue(Data));
        }
        *OutSample.GetDestAndAdvance() = Valid;
    }
}

void UFMODNiagaraEventPlayer::PlayPersistentAudio(FVectorVMContext& Context)
{
    VectorVM::FUserPtrHandler<FEventPlayerInterface_InstanceData> InstData(Context);

    FNDIInputParam<FNiagaraBool> PlayAudioParam(Context);
    FNDIInputParam<int32> AudioHandleInParam(Context);
    FNDIInputParam<FVector> PositionParam(Context);
    FNDIInputParam<FVector> RotationParam(Context);

    FNDIOutputParam<int32> AudioHandleOutParam(Context);

    checkfSlow(InstData.Get(), TEXT("Event player has invalid instance data. %s"), *GetPathName());

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
        bool ShouldPlay = PlayAudioParam.GetAndAdvance();
        int32 Handle = AudioHandleInParam.GetAndAdvance();
        FVector Position = PositionParam.GetAndAdvance();
        FVector InRot = RotationParam.GetAndAdvance();
        FRotator Rotation = FRotator(InRot.X, InRot.Y, InRot.Z);

        FPersistentEventParticleData AudioData;
        if (ShouldPlay)
        {
            if (Handle <= 0)
            {
                // play a new sound
                Handle = InstData->HandleCount.Increment();
                AudioData.AudioHandle = Handle;
                AudioData.UpdateCallback = [Handle, Position, Rotation](FEventPlayerInterface_InstanceData* InstanceData, UFMODAudioComponent*, FNiagaraSystemInstance* SystemInstance)
                {
                    USceneComponent* NiagaraComponent = SystemInstance->GetAttachComponent();
                    TWeakObjectPtr<UFMODEvent> Sound = InstanceData->EventToPlay;
                    if (NiagaraComponent && Sound.IsValid())
                    {
                        bool bStopWithEffect = InstanceData->bStopWhenComponentIsDestroyed;
                        TWeakObjectPtr<UFMODAudioComponent> AudioComponent = UFMODBlueprintStatics::PlayEventAttached(Sound.Get(), NiagaraComponent, NAME_None, Position, EAttachLocation::KeepWorldPosition, bStopWithEffect, true, true);

                        AudioComponent->Play();

                        InstanceData->PersistentAudioMapping.Add(Handle, AudioComponent);
                    }
                };
                InstData->PersistentAudioActionQueue.Enqueue(AudioData);
            }
            AudioHandleOutParam.SetAndAdvance(Handle);
            continue;
        }

        if (Handle > 0)
        {
            AudioData.AudioHandle = Handle;

            InstData->PersistentAudioActionQueue.Enqueue(AudioData);
        }
        AudioHandleOutParam.SetAndAdvance(0);
    }
}

bool UFMODNiagaraEventPlayer::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination))
    {
        return false;
    }

    UFMODNiagaraEventPlayer* OtherTyped = CastChecked<UFMODNiagaraEventPlayer>(Destination);
    OtherTyped->EventToPlay = EventToPlay;
    OtherTyped->bLimitPlaysPerTick = bLimitPlaysPerTick;
    OtherTyped->MaxPlaysPerTick = MaxPlaysPerTick;
    OtherTyped->ParameterNames = ParameterNames;
    OtherTyped->bStopWhenComponentIsDestroyed = bStopWhenComponentIsDestroyed;

#if WITH_EDITORONLY_DATA
    OtherTyped->bOnlyActiveDuringGameplay = bOnlyActiveDuringGameplay;
#endif
    return true;
}
