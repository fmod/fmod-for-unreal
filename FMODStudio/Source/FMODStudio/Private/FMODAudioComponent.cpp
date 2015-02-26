// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#include "FMODStudioPrivatePCH.h"
#include "FMODAudioComponent.h"
#include "FMODStudioModule.h"
#include "FMODUtils.h"
#include "FMODEvent.h"

#include "fmod_studio.hpp"

UFMODAudioComponent::UFMODAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoDestroy = false;
	bAutoActivate = true;
	bStopWhenOwnerDestroyed = true;
	bNeverNeedsRenderUpdate = true;
	bVisualizeComponent = true;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	StudioInstance = nullptr;
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

	if (bVisualizeComponent && SpriteComponent == NULL && GetOwner() && !GetWorld()->IsGameWorld())
	{
		SpriteComponent = ConstructObject<UBillboardComponent>(UBillboardComponent::StaticClass(), GetOwner(), NAME_None, RF_Transactional | RF_TextExportTransient);

		UpdateSpriteTexture();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->AttachTo(this);
		SpriteComponent->AlwaysLoadOnClient = false;
		SpriteComponent->AlwaysLoadOnServer = false;
		SpriteComponent->SpriteInfo.Category = TEXT("Misc");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Misc", "Misc");
		SpriteComponent->bCreatedByConstructionScript = bCreatedByConstructionScript;
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->bUseInEditorScaling = true;

		SpriteComponent->RegisterComponent();
	}
}

void UFMODAudioComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		if (bAutoActivate)
		{
			SpriteComponent->Sprite = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent_AutoActivate.S_AudioComponent_AutoActivate"));
		}
		else
		{
			SpriteComponent->Sprite = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent.S_AudioComponent"));
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

void UFMODAudioComponent::OnUpdateTransform(bool bSkipPhysicsMove)
{
	Super::OnUpdateTransform(bSkipPhysicsMove);
	if (StudioInstance)
	{
		FMOD_3D_ATTRIBUTES attr = {{0}};
		attr.position = FMODUtils::ConvertWorldVector(ComponentToWorld.GetLocation());
		StudioInstance->set3DAttributes(&attr);
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
		StudioInstance->release();
		StudioInstance = nullptr;
	}
}

void UFMODAudioComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (bIsActive)
	{
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

void UFMODAudioComponent::Play()
{
	Stop();
	StudioInstance = nullptr;

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
		StudioInstance->release();
	}
}

void UFMODAudioComponent::TriggerCue()
{
	UE_LOG(LogFMOD, Verbose, TEXT("UFMODAudioComponent %p TriggerCue"), this);
	if (StudioInstance)
	{
		// Studio only supports a single cue so try to get it
		FMOD::Studio::CueInstance* Cue = nullptr;
		StudioInstance->getCueByIndex(0, &Cue);
		if (Cue)
		{
			Cue->trigger();
		}
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