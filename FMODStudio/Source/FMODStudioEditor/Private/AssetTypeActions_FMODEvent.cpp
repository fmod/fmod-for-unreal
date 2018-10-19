// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "AssetTypeActions_FMODEvent.h"
#include "AssetTypeActions_Base.h"
#include "FMODEventEditor.h"
#include "FMODEvent.h"
#include "FMODUtils.h"
#include "FMODStudioModule.h"
#include "FMODStudioEditorModule.h"
#include "UnrealEd/Public/Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyle/Public/EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_FMODEvent::FAssetTypeActions_FMODEvent()
	: CurrentPreviewEventInstance(nullptr)
{
	BeginPIEDelegateHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FAssetTypeActions_FMODEvent::HandleBeginPIE);
	IFMODStudioModule::Get().BanksReloadedEvent().AddRaw(this, &FAssetTypeActions_FMODEvent::HandleBanksReloaded);
}

FAssetTypeActions_FMODEvent::~FAssetTypeActions_FMODEvent()
{
	FEditorDelegates::BeginPIE.Remove(BeginPIEDelegateHandle);
	IFMODStudioModule::Get().BanksReloadedEvent().RemoveAll(this);
	IFMODStudioModule::Get().StopAuditioningInstance();
}

UClass* FAssetTypeActions_FMODEvent::GetSupportedClass() const
{
	return UFMODEvent::StaticClass();
}

void FAssetTypeActions_FMODEvent::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Events = GetTypedWeakObjectPtrs<UFMODEvent>(InObjects);


	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODEvent_Play", "Play"),
		LOCTEXT("FMODEvent_PlayTooltip", "Plays the selected FMOD event."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Play"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_FMODEvent::ExecutePlay, Events),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_FMODEvent::CanExecutePlayCommand, Events)
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODEvent_Stop", "Stop"),
		LOCTEXT("FMODEvent_StopTooltip", "Stops the currently playing FMOD event."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Stop"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_FMODEvent::ExecuteStop, Events),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_FMODEvent::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Event = Cast<UFMODEvent>(*ObjIt);
		if (Event != nullptr)
		{
			TSharedRef<FFMODEventEditor> NewFMODEventEditor(new FFMODEventEditor());
			NewFMODEventEditor->InitFMODEventEditor(Mode, EditWithinLevelEditor, Event);
		}
	}
}

bool FAssetTypeActions_FMODEvent::CanExecutePlayCommand(TArray<TWeakObjectPtr<UFMODEvent>> Objects) const
{
	return Objects.Num() == 1;
}

void FAssetTypeActions_FMODEvent::AssetsActivated(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			UFMODEvent* Event = Cast<UFMODEvent>(*ObjIt);
			if (Event != nullptr)
			{
				// Only play the first valid event
				PlayEvent(Event);
				break;
			}
		}
	}
	else
	{
		FAssetTypeActions_Base::AssetsActivated(InObjects, ActivationType);
	}
}

void FAssetTypeActions_FMODEvent::ExecuteEdit(TArray<TWeakObjectPtr<UFMODEvent>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object != nullptr)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Object);
		}
	}
}

void FAssetTypeActions_FMODEvent::ExecutePlay(TArray<TWeakObjectPtr<UFMODEvent>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UFMODEvent* Event = (*ObjIt).Get();
		if (Event != nullptr)
		{
			// Only play the first valid event
			PlayEvent(Event);
			break;
		}
	}
}

void FAssetTypeActions_FMODEvent::ExecuteStop(TArray<TWeakObjectPtr<UFMODEvent>> Objects)
{
	IFMODStudioModule::Get().StopAuditioningInstance();
}

void FAssetTypeActions_FMODEvent::PlayEvent(UFMODEvent* Event)
{
	CurrentPreviewEventInstance = IFMODStudioModule::Get().CreateAuditioningInstance(Event);
	if (CurrentPreviewEventInstance != nullptr)
	{
		CurrentPreviewEventInstance->start();
	}
}

void FAssetTypeActions_FMODEvent::HandleBeginPIE(bool bSimulating)
{
	// Studio module will handle its own auditioning, just clear the handle
	CurrentPreviewEventInstance = nullptr;
}

void FAssetTypeActions_FMODEvent::HandleBanksReloaded()
{
	// Studio module will handle its own auditioning, just clear the handle
	CurrentPreviewEventInstance = nullptr;
}

#undef LOCTEXT_NAMESPACE
