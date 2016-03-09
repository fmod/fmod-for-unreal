// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioPrivatePCH.h"
#include "FMODAudioComponent.h"
#include "FMODStudioModule.h"
#include "FMODUtils.h"
#include "FMODEvent.h"
#include "FMODListener.h"
#include "fmod_studio.hpp"

UFMODAudioComponent::UFMODAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoDestroy = false;
	bAutoActivate = true;
	bEnableTimelineCallbacks = false; // Default OFF for efficiency
	bStopWhenOwnerDestroyed = true;
	bNeverNeedsRenderUpdate = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
	bApplyAmbientVolumes = false;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	StudioInstance = nullptr;
	ProgrammerSound = nullptr;

	InteriorLastUpdateTime = 0.0;
	SourceInteriorVolume = 0.0f;
	SourceInteriorLPF = 0.0f;
	CurrentInteriorVolume = 0.0f;
	CurrentInteriorLPF = 0.0f;
}

FString UFMODAudioComponent::GetDetailedInfoInternal(void) const
{
	FString Result;

	if(Event)
	{
		Result = Event->GetPathName(NULL);
	}
	else
	{
		Result = TEXT("No_Event");
	}

	return Result;
}

#if WITH_EDITORONLY_DATA
void UFMODAudioComponent::OnRegister()
{
	Super::OnRegister();

	UpdateSpriteTexture();
}

void UFMODAudioComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		if (bAutoActivate)
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent_AutoActivate.S_AudioComponent_AutoActivate")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent.S_AudioComponent")));
		}
	}
}
#endif

#if WITH_EDITOR
void UFMODAudioComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (IsPlaying())
	{
		Stop();
		Play();
	}

#if WITH_EDITORONLY_DATA
	UpdateSpriteTexture();
#endif

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

#if ENGINE_MINOR_VERSION >= 9
void UFMODAudioComponent::OnUpdateTransform(bool bSkipPhysicsMove, ETeleportType Teleport)
#else
void UFMODAudioComponent::OnUpdateTransform(bool bSkipPhysicsMove)
#endif
{
	Super::OnUpdateTransform(bSkipPhysicsMove);
	if (StudioInstance)
	{
		FMOD_3D_ATTRIBUTES attr = {{0}};
		attr.position = FMODUtils::ConvertWorldVector(ComponentToWorld.GetLocation());
		attr.up = FMODUtils::ConvertUnitVector(ComponentToWorld.GetUnitAxis(EAxis::Z));
		attr.forward = FMODUtils::ConvertUnitVector(ComponentToWorld.GetUnitAxis(EAxis::X));

		StudioInstance->set3DAttributes(&attr);

		if (bApplyAmbientVolumes)
		{
			UpdateInteriorVolumes();
		}
	}
}

// Taken mostly from ActiveSound.cpp
void UFMODAudioComponent::UpdateInteriorVolumes()
{
	// Result of the ambient calculations to apply to the instance
	float AmbientVolumeMultiplier = 1.0f;
	float AmbientHighFrequencyGain = 1.0f;

	FInteriorSettings Ambient;
	const FVector& Location = GetOwner()->GetTransform().GetTranslation();
	AAudioVolume* AudioVolume = GetWorld()->GetAudioSettings(Location, NULL, &Ambient);

	const FFMODListener& Listener = IFMODStudioModule::Get().GetNearestListener(Location);
	if( InteriorLastUpdateTime < Listener.InteriorStartTime )
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		InteriorLastUpdateTime = FApp::GetCurrentTime();
	}


	bool bAllowSpatialization = true;
	if( Listener.Volume == AudioVolume || !bAllowSpatialization )
	{
		// Ambient and listener in same ambient zone
		CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - Listener.InteriorVolumeInterp ) ) + Listener.InteriorVolumeInterp;
		AmbientVolumeMultiplier *= CurrentInteriorVolume;

		CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - Listener.InteriorLPFInterp ) ) + Listener.InteriorLPFInterp;
		AmbientHighFrequencyGain *= CurrentInteriorLPF;

		//UE_LOG(LogFMOD, Verbose, TEXT( "Ambient in same volume. Volume *= %g LPF *= %g" ), CurrentInteriorVolume, CurrentInteriorLPF);
	}
	else
	{
		// Ambient and listener in different ambient zone
		if( Ambient.bIsWorldSettings )
		{
			// The ambient sound is 'outside' - use the listener's exterior volume
			CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - Listener.ExteriorVolumeInterp ) ) + ( Listener.InteriorSettings.ExteriorVolume * Listener.ExteriorVolumeInterp );
			AmbientVolumeMultiplier *= CurrentInteriorVolume;

			CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - Listener.ExteriorLPFInterp ) ) + ( Listener.InteriorSettings.ExteriorLPF * Listener.ExteriorLPFInterp );
			AmbientHighFrequencyGain *= CurrentInteriorLPF;

			//UE_LOG(LogFMOD, Verbose, TEXT( "Ambient in diff volume, ambient outside. Volume *= %g LPF *= %g" ), CurrentInteriorVolume, CurrentInteriorLPF);
		}
		else
		{
			// The ambient sound is 'inside' - use the ambient sound's interior volume multiplied with the listeners exterior volume
			CurrentInteriorVolume = (( SourceInteriorVolume * ( 1.0f - Listener.InteriorVolumeInterp ) ) + ( Ambient.InteriorVolume * Listener.InteriorVolumeInterp ))
										* (( SourceInteriorVolume * ( 1.0f - Listener.ExteriorVolumeInterp ) ) + ( Listener.InteriorSettings.ExteriorVolume * Listener.ExteriorVolumeInterp ));
			AmbientVolumeMultiplier *= CurrentInteriorVolume;

			CurrentInteriorLPF = (( SourceInteriorLPF * ( 1.0f - Listener.InteriorLPFInterp ) ) + ( Ambient.InteriorLPF * Listener.InteriorLPFInterp ))
										* (( SourceInteriorLPF * ( 1.0f - Listener.ExteriorLPFInterp ) ) + ( Listener.InteriorSettings.ExteriorLPF * Listener.ExteriorLPFInterp ));
			AmbientHighFrequencyGain *= CurrentInteriorLPF;

			//UE_LOG(LogFMOD, Verbose, TEXT( "Ambient in diff volume, ambient inside. Volume *= %g LPF *= %g" ), CurrentInteriorVolume, CurrentInteriorLPF);
		}
	}

	StudioInstance->setVolume(AmbientVolumeMultiplier);

	FMOD::ChannelGroup* ChanGroup = nullptr;
	StudioInstance->getChannelGroup(&ChanGroup);
	if (ChanGroup)
	{
		int NumDSP = 0;
		ChanGroup->getNumDSPs(&NumDSP);
		for (int Index=0; Index<NumDSP; ++Index)
		{
			FMOD::DSP* ChanDSP = nullptr;
			ChanGroup->getDSP(Index, &ChanDSP);
			if (ChanDSP)
			{
				FMOD_DSP_TYPE DSPType = FMOD_DSP_TYPE_UNKNOWN;
				ChanDSP->getType(&DSPType);
				if (DSPType == FMOD_DSP_TYPE_LOWPASS || DSPType == FMOD_DSP_TYPE_LOWPASS_SIMPLE)
				{
					static float MAX_FREQUENCY = 8000.0f;
					float Frequency = MAX_FREQUENCY * AmbientHighFrequencyGain;
					ChanDSP->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, MAX_FREQUENCY * AmbientHighFrequencyGain);
					break;
				}
			}
		}
	}
}

void UFMODAudioComponent::OnUnregister()
{
	// Route OnUnregister event.
	Super::OnUnregister();

	// Don't stop audio and clean up component if owner has been destroyed (default behaviour). This function gets
	// called from AActor::ClearComponents when an actor gets destroyed which is not usually what we want for one-
	// shot sounds.
	AActor* Owner = GetOwner();
	if (!Owner || bStopWhenOwnerDestroyed )
	{
		Stop();
	}

	if (StudioInstance)
	{
		StudioInstance->setCallback(nullptr);
		StudioInstance->release();
		StudioInstance = nullptr;
	}
}

void UFMODAudioComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (bIsActive)
	{
		if (bApplyAmbientVolumes && IFMODStudioModule::Get().HasListenerMoved())
		{
			UpdateInteriorVolumes();
		}

		TArray<FTimelineMarkerProperties> LocalMarkerQueue;
		TArray<FTimelineBeatProperties> LocalBeatQueue;
		{
			FScopeLock Lock(&CallbackLock);
			Swap(LocalMarkerQueue, CallbackMarkerQueue);
			Swap(LocalBeatQueue, CallbackBeatQueue);
		}

		for(const FTimelineMarkerProperties& EachProps : LocalMarkerQueue)
		{
			OnTimelineMarker.Broadcast(EachProps.Name, EachProps.Position);
		}
		for(const FTimelineBeatProperties& EachProps : LocalBeatQueue)
		{
			OnTimelineBeat.Broadcast(EachProps.Bar, EachProps.Beat, EachProps.Position, EachProps.Tempo, EachProps.TimeSignatureUpper, EachProps.TimeSignatureLower);
		}

		FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_STOPPED;
		StudioInstance->getPlaybackState(&state);
		if (state == FMOD_STUDIO_PLAYBACK_STOPPED)
		{
			OnPlaybackCompleted();
		}
	}
}

void UFMODAudioComponent::SetEvent(UFMODEvent* NewEvent)
{
	const bool bPlay = IsPlaying();

	Stop();
	Event = NewEvent;

	if (bPlay)
	{
		Play();
	}
}

void UFMODAudioComponent::PostLoad()
{
	Super::PostLoad();
}

void UFMODAudioComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate()==true)
	{
		Play();
	}
}

void UFMODAudioComponent::Deactivate()
{
	if (ShouldActivate()==false)
	{
		Stop();
	}
}

FMOD_RESULT F_CALLBACK UFMODAudioComponent_EventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE *event, void *parameters)
{
	UFMODAudioComponent* Component = nullptr;
	FMOD::Studio::EventInstance* Instance = (FMOD::Studio::EventInstance*)event;
	if (Instance->getUserData((void**)&Component) == FMOD_OK && Component != nullptr)
	{
		if (type == FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_MARKER && Component->bEnableTimelineCallbacks)
		{
			Component->EventCallbackAddMarker((FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES*)parameters);
		}
		else if (type == FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_BEAT && Component->bEnableTimelineCallbacks)
		{
			Component->EventCallbackAddBeat((FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES*)parameters);
		}
		else if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)
		{
			Component->EventCallbackCreateProgrammerSound((FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*)parameters);
		}
		else if (type == FMOD_STUDIO_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)
		{
			Component->EventCallbackDestroyProgrammerSound((FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*)parameters);
		}
	}
	return FMOD_OK;
}

void UFMODAudioComponent::EventCallbackAddMarker(FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES* props)
{
	FScopeLock Lock(&CallbackLock);
	FTimelineMarkerProperties info;
	info.Name = props->name;
	info.Position = props->position;
	CallbackMarkerQueue.Push(info);
}

void UFMODAudioComponent::EventCallbackAddBeat(FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES* props)
{
	FScopeLock Lock(&CallbackLock);
	FTimelineBeatProperties info;
	info.Bar = props->bar;
	info.Beat = props->beat;
	info.Position = props->position;
	info.Tempo = props->tempo;
	info.TimeSignatureUpper = props->timeSignatureUpper;
	info.TimeSignatureLower = props->timeSignatureLower;
	CallbackBeatQueue.Push(info);
}

void UFMODAudioComponent::EventCallbackCreateProgrammerSound(FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props)
{
	// Make sure name isn't being changed as we are reading it
	FString ProgrammerSoundNameCopy;
	{
		FScopeLock Lock(&CallbackLock);
		ProgrammerSoundNameCopy = ProgrammerSoundName;
	}

	if (ProgrammerSound)
	{
		props->sound = (FMOD_SOUND*)ProgrammerSound;
		props->subsoundIndex = -1;
	}
	else if (ProgrammerSoundNameCopy.Len() || strlen(props->name) != 0)
	{
		FMOD::Studio::System* System = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
		FMOD::System* LowLevelSystem = nullptr;
		System->getLowLevelSystem(&LowLevelSystem);
		FString SoundName = ProgrammerSoundNameCopy.Len() ? ProgrammerSoundNameCopy : UTF8_TO_TCHAR(props->name);

		if (SoundName.Contains(TEXT(".")))
		{
			// Load via file
			FString SoundPath = SoundName;
			if (FPaths::IsRelative(SoundPath))
			{
				SoundPath = FPaths::GameContentDir() / SoundPath;
			}

			FMOD::Sound* Sound = nullptr;
			if (LowLevelSystem->createSound(TCHAR_TO_UTF8(*SoundPath), FMOD_DEFAULT, nullptr, &Sound) == FMOD_OK)
			{
				UE_LOG(LogFMOD, Verbose, TEXT("Creating programmer sound from file '%s'"), *SoundPath);
				props->sound = (FMOD_SOUND*)Sound;
				props->subsoundIndex = -1;
			}
			else
			{
				UE_LOG(LogFMOD, Warning, TEXT("Failed to load programmer sound file '%s'"), *SoundPath);
			}
		}
		else
		{
			// Load via FMOD Studio asset table
			FMOD_STUDIO_SOUND_INFO SoundInfo = {0};
			FMOD_RESULT Result = System->getSoundInfo(TCHAR_TO_UTF8(*SoundName), &SoundInfo);
			if (Result == FMOD_OK)
			{
				FMOD::Sound* Sound = nullptr;
				Result = LowLevelSystem->createSound(SoundInfo.name_or_data, SoundInfo.mode, &SoundInfo.exinfo, &Sound);
				if (Result == FMOD_OK)
				{
					UE_LOG(LogFMOD, Verbose, TEXT("Creating programmer sound using audio entry '%s'"), *SoundName);

					props->sound = (FMOD_SOUND*)Sound;
					props->subsoundIndex = SoundInfo.subsoundIndex;
				}
				else
				{
					UE_LOG(LogFMOD, Warning, TEXT("Failed to load FMOD audio entry '%s'"), *SoundName);
				}
			}
			else
			{
				UE_LOG(LogFMOD, Warning, TEXT("Failed to find FMOD audio entry '%s'"), *SoundName);
			}
		}
	}
}

void UFMODAudioComponent::EventCallbackDestroyProgrammerSound(FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props)
{
	if (props->sound && ProgrammerSound == nullptr)
	{
		UE_LOG(LogFMOD, Verbose, TEXT("Destroying programmer sound"));
		FMOD_RESULT Result = ((FMOD::Sound*)props->sound)->release();
		verifyfmod(Result);
	}
}

void UFMODAudioComponent::SetProgrammerSoundName(FString Value)
{
	FScopeLock Lock(&CallbackLock);
	ProgrammerSoundName = Value;
}

void UFMODAudioComponent::SetProgrammerSound(FMOD::Sound* Sound)
{
	FScopeLock Lock(&CallbackLock);
	ProgrammerSound = Sound;
}

void UFMODAudioComponent::Play()
{
	Stop();

	if (!FMODUtils::IsWorldAudible(GetWorld()))
	{
		return;
	}

	UE_LOG(LogFMOD, Verbose, TEXT("UFMODAudioComponent %p Play"), this);
	
	// Only play events in PIE/game, not when placing them in the editor
	FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(Event.Get());
	if (EventDesc != nullptr)
	{
		FMOD_RESULT result = EventDesc->createInstance(&StudioInstance);
		if (StudioInstance != nullptr)
		{
			FMOD_STUDIO_USER_PROPERTY UserProp = {0};
			if (EventDesc->getUserProperty("Ambient", &UserProp) == FMOD_OK)
			{
				if (UserProp.type == FMOD_STUDIO_USER_PROPERTY_TYPE_FLOAT) // All numbers are stored as float
				{
					bApplyAmbientVolumes = (UserProp.floatValue != 0.0f);
				}
			}
			OnUpdateTransform(true);
			// Set initial parameters
			for (auto Kvp : StoredParameters)
			{
				FMOD_RESULT Result = StudioInstance->setParameterValue(TCHAR_TO_UTF8(*Kvp.Key.ToString()), Kvp.Value);
				if (Result != FMOD_OK)
				{
					UE_LOG(LogFMOD, Warning, TEXT("Failed to set initial parameter %s"), *Kvp.Key.ToString());
				}
			}

			verifyfmod(StudioInstance->setUserData(this));
			verifyfmod(StudioInstance->setCallback(UFMODAudioComponent_EventCallback));
			verifyfmod(StudioInstance->start());
			UE_LOG(LogFMOD, Verbose, TEXT("Playing component %p"), this);
			bIsActive = true;
			SetComponentTickEnabled(true);
		}
	}
}

void UFMODAudioComponent::Stop()
{
	UE_LOG(LogFMOD, Verbose, TEXT("UFMODAudioComponent %p Stop"), this);
	if (StudioInstance)
	{
		StudioInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
		StudioInstance->setCallback(nullptr);
		StudioInstance->release();
		StudioInstance = nullptr;
	}
	InteriorLastUpdateTime = 0.0;
}

void UFMODAudioComponent::TriggerCue()
{
	UE_LOG(LogFMOD, Verbose, TEXT("UFMODAudioComponent %p TriggerCue"), this);
	if (StudioInstance)
	{
		// Studio only supports a single cue so try to get it
#if FMOD_VERSION >= 0x00010800
		StudioInstance->triggerCue();
#else
		FMOD::Studio::CueInstance* Cue = nullptr;
		StudioInstance->getCueByIndex(0, &Cue);
		if (Cue)
		{
			Cue->trigger();
		}
#endif
	}
}

void UFMODAudioComponent::OnPlaybackCompleted()
{
	// Mark inactive before calling destroy to avoid recursion
	UE_LOG(LogFMOD, Verbose, TEXT("UFMODAudioComponent %p PlaybackCompleted"), this);
	bIsActive = false;
	SetComponentTickEnabled(false);

	OnEventStopped.Broadcast();

	if (StudioInstance)
	{
		StudioInstance->setCallback(nullptr);
		StudioInstance->release();
		StudioInstance = nullptr;
	}

	// Auto destruction is handled via marking object for deletion.
	if (bAutoDestroy)
	{
		DestroyComponent();
	}
}

bool UFMODAudioComponent::IsPlaying( void )
{
	return bIsActive;
}


void UFMODAudioComponent::SetVolume(float Volume)
{
	if (StudioInstance)
	{
		FMOD_RESULT Result = StudioInstance->setVolume(Volume);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set volume"));
		}
	}
}

void UFMODAudioComponent::SetPitch(float Pitch)
{
	if (StudioInstance)
	{
		FMOD_RESULT Result = StudioInstance->setPitch(Pitch);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set pitch"));
		}
	}
}

void UFMODAudioComponent::SetPaused(bool Paused)
{
	if (StudioInstance)
	{
		FMOD_RESULT Result = StudioInstance->setPaused(Paused);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to pause"));
		}
	}
}

void UFMODAudioComponent::SetParameter(FName Name, float Value)
{
	if (StudioInstance)
	{
		FMOD_RESULT Result = StudioInstance->setParameterValue(TCHAR_TO_UTF8(*Name.ToString()), Value);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set parameter %s"), *Name.ToString());
		}
	}
	StoredParameters.FindOrAdd(Name) = Value;
}

void UFMODAudioComponent::SetTimelinePosition(int32 Time)
{
	if (StudioInstance)
	{
		FMOD_RESULT Result = StudioInstance->setTimelinePosition(Time);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set timeline position"));
		}
	}
}

int32 UFMODAudioComponent::GetTimelinePosition()
{
	int Time = 0;
	if (StudioInstance)
	{
		FMOD_RESULT Result = StudioInstance->getTimelinePosition(&Time);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to get timeline position"));
		}
	}
	return Time;
}


float UFMODAudioComponent::GetParameter(FName Name)
{
	float Value = 0.0f;
	float* StoredParam = StoredParameters.Find(Name);
	if (StoredParam)
	{
		Value = *StoredParam;
	}
	if (StudioInstance)
	{
		FMOD::Studio::ParameterInstance* ParamInst = nullptr;
		FMOD_RESULT Result = StudioInstance->getParameter(TCHAR_TO_UTF8(*Name.ToString()), &ParamInst);
		if (Result == FMOD_OK)
		{
			float QueryValue;
			Result = ParamInst->getValue(&QueryValue);
			if (Result == FMOD_OK)
			{
				Value = QueryValue;
			}
		}
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to get parameter %s"), *Name.ToString());
		}
	}
	return Value;
}