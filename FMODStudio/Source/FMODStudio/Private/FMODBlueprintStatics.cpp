// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioPrivatePCH.h"
#include "FMODBlueprintStatics.h"
#include "FMODAudioComponent.h"
#include "FMODSettings.h"
#include "FMODStudioModule.h"
#include "FMODUtils.h"
#include "FMODBank.h"
#include "FMODEvent.h"
#include "FMODBus.h"
#include "fmod_studio.hpp"

/////////////////////////////////////////////////////
// UFMODBlueprintStatics

UFMODBlueprintStatics::UFMODBlueprintStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FFMODEventInstance UFMODBlueprintStatics::PlayEvent2D(UObject* WorldContextObject, class UFMODEvent* Event, bool bAutoPlay)
{
	return PlayEventAtLocation(WorldContextObject, Event, FTransform(), bAutoPlay);
}

FFMODEventInstance UFMODBlueprintStatics::PlayEventAtLocation(UObject* WorldContextObject, class UFMODEvent* Event, const FTransform& Location, bool bAutoPlay)
{
	FFMODEventInstance Instance;
	Instance.Instance = nullptr;
	
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (FMODUtils::IsWorldAudible(ThisWorld))
	{
		FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(Event);
		if (EventDesc != nullptr)
		{
			FMOD::Studio::EventInstance* EventInst = nullptr;
			EventDesc->createInstance(&EventInst);
			if (EventInst != nullptr)
			{
				FMOD_3D_ATTRIBUTES EventAttr = { { 0 } };
				FMODUtils::Assign(EventAttr, Location);
				EventInst->set3DAttributes(&EventAttr);

				if (bAutoPlay)
				{
					EventInst->start();
					EventInst->release();
				}
				Instance.Instance = EventInst;
			}
		}
	}
	return Instance;
}

class UFMODAudioComponent* UFMODBlueprintStatics::PlayEventAttached(class UFMODEvent* Event, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, EAttachLocation::Type LocationType, bool bStopWhenAttachedToDestroyed, bool bAutoPlay)
{
	if (Event == nullptr)
	{
		return nullptr;
	}
	if (AttachToComponent == nullptr)
	{
		UE_LOG(LogFMOD, Warning, TEXT("UFMODBlueprintStatics::PlayEventAttached: NULL AttachComponent specified!"));
		return nullptr;
	}

	AActor* Actor = AttachToComponent->GetOwner();

	// Avoid creating component if we're trying to play a sound on an already destroyed actor.
	if (Actor && Actor->IsPendingKill())
	{
		return nullptr;
	}

	UFMODAudioComponent* AudioComponent;
	if( Actor )
	{
		// Use actor as outer if we have one.
		AudioComponent = NewObject<UFMODAudioComponent>(Actor);
	}
	else
	{
		// Let engine pick the outer (transient package).
		AudioComponent = NewObject<UFMODAudioComponent>();
	}
	check( AudioComponent );
	AudioComponent->Event = Event;
	AudioComponent->bAutoActivate = false;
	AudioComponent->bAutoDestroy = true;
	AudioComponent->bStopWhenOwnerDestroyed = bStopWhenAttachedToDestroyed;
#if WITH_EDITORONLY_DATA
	AudioComponent->bVisualizeComponent = false;
#endif
	AudioComponent->RegisterComponentWithWorld(AttachToComponent->GetWorld());

	AudioComponent->AttachTo(AttachToComponent, AttachPointName);
	if (LocationType == EAttachLocation::KeepWorldPosition)
	{
		AudioComponent->SetWorldLocation(Location);
	}
	else
	{
		AudioComponent->SetRelativeLocation(Location);
	}

	if (bAutoPlay)
	{
		AudioComponent->Play();
	}
	return AudioComponent;
}

UFMODAsset* UFMODBlueprintStatics::FindAssetByName(const FString& Name)
{
	return IFMODStudioModule::Get().FindAssetByName(Name);
}

UFMODEvent* UFMODBlueprintStatics::FindEventByName(const FString& Name)
{
	return IFMODStudioModule::Get().FindEventByName(Name);
}

void UFMODBlueprintStatics::LoadBank(class UFMODBank* Bank, bool bBlocking, bool bLoadSampleData)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bank != nullptr)
	{
		UE_LOG(LogFMOD, Log, TEXT("LoadBank %s"), *Bank->GetName());

		const UFMODSettings& Settings = *GetDefault<UFMODSettings>();
		FString BankPath = Settings.GetFullBankPath() / (Bank->GetName() + TEXT(".bank"));

		FMOD::Studio::Bank* bank = nullptr;
		FMOD_STUDIO_LOAD_BANK_FLAGS flags = bBlocking ? FMOD_STUDIO_LOAD_BANK_NORMAL : FMOD_STUDIO_LOAD_BANK_NONBLOCKING;
		FMOD_RESULT result = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*BankPath), flags, &bank);
		if ( result == FMOD_OK && bank != nullptr && bLoadSampleData )
		{
			// Make sure bank is ready to load sample data from
			StudioSystem->flushCommands();
			bank->loadSampleData();
		}
	}
}

void UFMODBlueprintStatics::UnloadBank(class UFMODBank* Bank)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bank != nullptr)
	{
		UE_LOG(LogFMOD, Log, TEXT("UnloadBank %s"), *Bank->GetName());

		FMOD::Studio::ID guid = FMODUtils::ConvertGuid(Bank->AssetGuid);
		FMOD::Studio::Bank* bank = nullptr;
		FMOD_RESULT result = StudioSystem->getBankByID(&guid, &bank);
		if (result == FMOD_OK && bank != nullptr)
		{
			bank->unload();
		}
	}
}

void UFMODBlueprintStatics::LoadBankSampleData(class UFMODBank* Bank)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bank != nullptr)
	{
		FMOD::Studio::ID guid = FMODUtils::ConvertGuid(Bank->AssetGuid);
		FMOD::Studio::Bank* bank = nullptr;
		FMOD_RESULT result = StudioSystem->getBankByID(&guid, &bank);
		if (result == FMOD_OK && bank != nullptr)
		{
			bank->loadSampleData();
		}
	}
}

void UFMODBlueprintStatics::UnloadBankSampleData(class UFMODBank* Bank)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bank != nullptr)
	{
		FMOD::Studio::ID guid = FMODUtils::ConvertGuid(Bank->AssetGuid);
		FMOD::Studio::Bank* bank = nullptr;
		FMOD_RESULT result = StudioSystem->getBankByID(&guid, &bank);
		if (result == FMOD_OK && bank != nullptr)
		{
			bank->unloadSampleData();
		}
	}
}

void UFMODBlueprintStatics::LoadEventSampleData(UObject* WorldContextObject, class UFMODEvent* Event)
{
	FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(Event);
	if (EventDesc != nullptr)
	{
		EventDesc->loadSampleData();
	}
}

void UFMODBlueprintStatics::UnloadEventSampleData(UObject* WorldContextObject, class UFMODEvent* Event)
{
	FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(Event);
	if (EventDesc != nullptr)
	{
		EventDesc->unloadSampleData();
	}
}

TArray<FFMODEventInstance> UFMODBlueprintStatics::FindEventInstances(UObject* WorldContextObject, UFMODEvent* Event)
{
	FMOD::Studio::EventDescription* EventDesc = IFMODStudioModule::Get().GetEventDescription(Event);
	TArray<FFMODEventInstance> Instances;
	if (EventDesc != nullptr)
	{
		int Capacity = 0;
		EventDesc->getInstanceCount(&Capacity);
		TArray<FMOD::Studio::EventInstance*> InstancePointers;
		InstancePointers.SetNum(Capacity, true);
		int Count = 0;
		EventDesc->getInstanceList(InstancePointers.GetData(), Capacity, &Count);
		Instances.SetNum(Count, true);
		for (int i=0; i<Count; ++i)
		{
			Instances[i].Instance = InstancePointers[i];
		}
	}
	return Instances;
}

void UFMODBlueprintStatics::BusSetFaderLevel(class UFMODBus* Bus, float Level)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bus != nullptr)
	{
		FMOD::Studio::ID guid = FMODUtils::ConvertGuid(Bus->AssetGuid);
		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = StudioSystem->getBusByID(&guid, &bus);
		if (result == FMOD_OK && bus != nullptr)
		{
			bus->setFaderLevel(Level);
		}
	}
}

void UFMODBlueprintStatics::BusSetPaused(class UFMODBus* Bus, bool bPaused)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bus != nullptr)
	{
		FMOD::Studio::ID guid = FMODUtils::ConvertGuid(Bus->AssetGuid);
		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = StudioSystem->getBusByID(&guid, &bus);
		if (result == FMOD_OK && bus != nullptr)
		{
			bus->setPaused(bPaused);
		}
	}
}

void UFMODBlueprintStatics::BusSetMute(class UFMODBus* Bus, bool bMute)
{
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (StudioSystem != nullptr && Bus != nullptr)
	{
		FMOD::Studio::ID guid = FMODUtils::ConvertGuid(Bus->AssetGuid);
		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = StudioSystem->getBusByID(&guid, &bus);
		if (result == FMOD_OK && bus != nullptr)
		{
			bus->setMute(bMute);
		}
	}
}

bool UFMODBlueprintStatics::EventInstanceIsValid(FFMODEventInstance EventInstance)
{
	if (EventInstance.Instance)
	{
		return EventInstance.Instance->isValid();
	}
	return false;
}

void UFMODBlueprintStatics::EventInstanceSetVolume(FFMODEventInstance EventInstance, float Volume)
{
	if (EventInstance.Instance)
	{
		FMOD_RESULT Result = EventInstance.Instance->setVolume(Volume);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set event instance volume"));
		}
	}
}

void UFMODBlueprintStatics::EventInstanceSetPitch(FFMODEventInstance EventInstance, float Pitch)
{
	if (EventInstance.Instance)
	{
		FMOD_RESULT Result = EventInstance.Instance->setPitch(Pitch);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set event instance pitch"));
		}
	}
}

void UFMODBlueprintStatics::EventInstanceSetPaused(FFMODEventInstance EventInstance, bool Paused)
{
	if (EventInstance.Instance)
	{
		FMOD_RESULT Result = EventInstance.Instance->setPaused(Paused);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to pause event instance"));
		}
	}
}

void UFMODBlueprintStatics::EventInstanceSetParameter(FFMODEventInstance EventInstance, FName Name, float Value)
{
	if (EventInstance.Instance)
	{
		FMOD_RESULT Result = EventInstance.Instance->setParameterValue(TCHAR_TO_UTF8(*Name.ToString()), Value);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set event instance parameter %s"), *Name.ToString());
		}
	}
}

float UFMODBlueprintStatics::EventInstanceGetParameter(FFMODEventInstance EventInstance, FName Name)
{
	float Value = 0.0f;
	if (EventInstance.Instance)
	{
		FMOD::Studio::ParameterInstance* ParamInst = nullptr;
		FMOD_RESULT Result = EventInstance.Instance->getParameter(TCHAR_TO_UTF8(*Name.ToString()), &ParamInst);
		if (Result == FMOD_OK)
		{
			Result = ParamInst->getValue(&Value);
		}
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to get event instance parameter %s"), *Name.ToString());
		}
	}
	return Value;
}

void UFMODBlueprintStatics::EventInstancePlay(FFMODEventInstance EventInstance)
{
	if (EventInstance.Instance)
	{
		FMOD_RESULT Result = EventInstance.Instance->start();
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to play event instance"));
		}
		// Once we start playing, allow instance to be cleaned up when it finishes
		EventInstance.Instance->release();
	}
}

void UFMODBlueprintStatics::EventInstanceStop(FFMODEventInstance EventInstance)
{
	if (EventInstance.Instance)
	{
		FMOD_RESULT Result = EventInstance.Instance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to stop event instance"));
		}
	}
}

void UFMODBlueprintStatics::EventInstanceTriggerCue(FFMODEventInstance EventInstance)
{
	if (EventInstance.Instance)
	{
		// Studio only supports a single cue so try to get it
#if FMOD_VERSION >= 0x00010800
		EventInstance.Instance->triggerCue();
#else
		FMOD::Studio::CueInstance* Cue = nullptr;
		EventInstance.Instance->getCueByIndex(0, &Cue);
		if (Cue)
		{
			Cue->trigger();
		}
#endif
	}
}

void UFMODBlueprintStatics::EventInstanceSetTransform(FFMODEventInstance EventInstance, const FTransform& Location)
{
	if (EventInstance.Instance)
	{
		FMOD_3D_ATTRIBUTES attr = {{0}};
		FMODUtils::Assign(attr, Location);
		FMOD_RESULT Result = EventInstance.Instance->set3DAttributes(&attr);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to set transform on event instance"));
		}
	}


}
