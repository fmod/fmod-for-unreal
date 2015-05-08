// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#include "FMODStudioEditorPrivatePCH.h"

#include "FMODStudioEditorModule.h"
#include "FMODStudioModule.h"
#include "FMODStudioStyle.h"
#include "FMODAudioComponent.h"
#include "FMODAssetBroker.h"
#include "FMODSettings.h"
#include "FMODUtils.h"

#include "FMODEventEditor.h"
#include "FMODAudioComponentVisualizer.h"
#include "FMODAmbientSoundDetails.h"

#include "SlateBasics.h"
#include "AssetTypeActions_FMODEvent.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Editor.h"
#include "SceneViewport.h"
#include "LevelEditor.h"

#include "fmod_studio.hpp"

#define LOCTEXT_NAMESPACE "FMODStudio"

DEFINE_LOG_CATEGORY(LogFMOD);

class FFMODStudioEditorModule : public IFMODStudioEditorModule
{
public:
	/** IModuleInterface implementation */
	FFMODStudioEditorModule() :
		bSimulating(false),
		bIsInPIE(false),
		bRegisteredComponentVisualizers(false)
	{
	}

	virtual void StartupModule() override;
	virtual void PostLoadCallback() override;
	virtual void ShutdownModule() override;

	bool HandleSettingsSaved();

	/** Called after all banks were reloaded by the studio module */
	void HandleBanksReloaded();

	void BeginPIE(bool simulating);
	void EndPIE(bool simulating);
	void PausePIE(bool simulating);
	void ResumePIE(bool simulating);

	void ViewportDraw(UCanvas* Canvas, APlayerController*);

	bool Tick( float DeltaTime );

	/** Add extensions to menu */
	void AddHelpMenuExtension(FMenuBuilder& MenuBuilder);
	void AddFileMenuExtension(FMenuBuilder& MenuBuilder);

	/** Show FMOD version */
	void ShowVersion();
	/** Open CHM */
	void OpenCHM();
	/** Open web page to online docs */
	void OpenOnlineDocs();
	/** Open Video tutorials page */
	void OpenVideoTutorials();

	/** Reload banks */
	void ReloadBanks();

	TArray<FName> RegisteredComponentClassNames;
	void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);

	/** The delegate to be invoked when this profiler manager ticks. */
	FTickerDelegate OnTick;

	/** Handle for registered delegates. */
	FDelegateHandle TickDelegateHandle;
	FDelegateHandle BeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;
	FDelegateHandle PausePIEDelegateHandle;
	FDelegateHandle ResumePIEDelegateHandle;
	FDelegateHandle HandleBanksReloadedDelegateHandle;

	/** Hook for drawing viewport */
	FDebugDrawDelegate ViewportDrawingDelegate;
	FDelegateHandle ViewportDrawingDelegateHandle;

	TSharedPtr<IComponentAssetBroker> AssetBroker;

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MainMenuExtender;

	/** Asset type actions for events (edit, play, stop) */
	TSharedPtr<FAssetTypeActions_FMODEvent> FMODEventAssetTypeActions;

	bool bSimulating;
	bool bIsInPIE;
	bool bRegisteredComponentVisualizers;
};

IMPLEMENT_MODULE( FFMODStudioEditorModule, FMODStudioEditor )

void FFMODStudioEditorModule::StartupModule()
{
	UE_LOG(LogFMOD, Log, TEXT("FFMODStudioEditorModule startup"));

	AssetBroker = MakeShareable(new FFMODAssetBroker);
	FComponentAssetBrokerage::RegisterBroker(AssetBroker, UFMODAudioComponent::StaticClass(), true, true);

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "FMODStudio",
			LOCTEXT("FMODStudioSettingsName", "FMOD Studio"),
			LOCTEXT("FMODStudioDescription", "Configure the FMOD Studio plugin"),
			GetMutableDefault<UFMODSettings>()
		);

		if (SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FFMODStudioEditorModule::HandleSettingsSaved);
		}
	}

	// Register the details customizations
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("FMODAmbientSound", FOnGetDetailCustomizationInstance::CreateStatic(&FFMODAmbientSoundDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	// Need to load the editor module since it gets created after us, and we can't re-order ourselves otherwise our asset registration stops working!
	// It only works if we are running the editor, not a commandlet
	if (!IsRunningCommandlet())
	{
		MainMenuExtender = MakeShareable(new FExtender);
		MainMenuExtender->AddMenuExtension("HelpBrowse", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FFMODStudioEditorModule::AddHelpMenuExtension));
		MainMenuExtender->AddMenuExtension("FileLoadAndSave", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FFMODStudioEditorModule::AddFileMenuExtension));

		FLevelEditorModule* LevelEditor = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		if (LevelEditor)
		{
			LevelEditor->GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
		}
	}

	// Register AssetTypeActions
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FMODEventAssetTypeActions = MakeShareable(new FAssetTypeActions_FMODEvent);
	AssetTools.RegisterAssetTypeActions(FMODEventAssetTypeActions.ToSharedRef());

	// Register slate style overrides
	FFMODStudioStyle::Initialize();

	BeginPIEDelegateHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FFMODStudioEditorModule::BeginPIE);
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(this, &FFMODStudioEditorModule::EndPIE);
	PausePIEDelegateHandle = FEditorDelegates::PausePIE.AddRaw(this, &FFMODStudioEditorModule::PausePIE);
	ResumePIEDelegateHandle = FEditorDelegates::ResumePIE.AddRaw(this, &FFMODStudioEditorModule::ResumePIE);

	ViewportDrawingDelegate = FDebugDrawDelegate::CreateRaw(this, &FFMODStudioEditorModule::ViewportDraw);
	ViewportDrawingDelegateHandle = UDebugDrawService::Register(TEXT("Editor"), ViewportDrawingDelegate);

	OnTick = FTickerDelegate::CreateRaw( this, &FFMODStudioEditorModule::Tick );
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker( OnTick );

	// This module is loaded after FMODStudioModule
	HandleBanksReloadedDelegateHandle = IFMODStudioModule::Get().BanksReloadedEvent().AddRaw(this, &FFMODStudioEditorModule::HandleBanksReloaded);

}

void FFMODStudioEditorModule::AddHelpMenuExtension(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("FMODHelp", LOCTEXT("FMODHelpLabel", "FMOD Help"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODVersionMenuEntryTitle", "About FMOD Studio"),
		LOCTEXT("FMODVersionMenuEntryToolTip", "Shows the informationa about FMOD Studio."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FFMODStudioEditorModule::ShowVersion)));

#if PLATFORM_WINDOWS
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODHelpCHMTitle", "FMOD Documentation..."),
		LOCTEXT("FMODHelpCHMToolTip", "Opens the local FMOD documentation."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.BrowseAPIReference"),
		FUIAction(FExecuteAction::CreateRaw(this, &FFMODStudioEditorModule::OpenCHM)));
#endif
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODHelpOnlineTitle", "FMOD Online Documentation..."),
		LOCTEXT("FMODHelpOnlineToolTip", "Go to the online FMOD documentation."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.BrowseDocumentation"),
		FUIAction(FExecuteAction::CreateRaw(this, &FFMODStudioEditorModule::OpenOnlineDocs)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODHelpVideosTitle", "FMOD Tutorial Videos..."),
		LOCTEXT("FMODHelpVideosToolTip", "Go to the online FMOD tutorial videos."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tutorials"),
		FUIAction(FExecuteAction::CreateRaw(this, &FFMODStudioEditorModule::OpenVideoTutorials)));

	MenuBuilder.EndSection();
}

void FFMODStudioEditorModule::AddFileMenuExtension(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("FMODFile", LOCTEXT("FMODFileLabel", "FMOD"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODFileMenuEntryTitle", "Reload Banks"),
		LOCTEXT("FMODFileMenuEntryToolTip", "Force a manual reload of all FMOD Studio banks."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FFMODStudioEditorModule::ReloadBanks)));
	MenuBuilder.EndSection();
}

void FFMODStudioEditorModule::ShowVersion()
{
	unsigned int HeaderVersion = FMOD_VERSION;
	unsigned int DLLVersion = 0;
	// Just grab it from the audition context which is always valid
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Auditioning);
	if (StudioSystem)
	{
		FMOD::System* LowLevelSystem = nullptr;
		if (StudioSystem->getLowLevelSystem(&LowLevelSystem) == FMOD_OK)
		{
			LowLevelSystem->getVersion(&DLLVersion);
		}
	}

	FString Message = FString::Printf(TEXT("FMOD Studio\nHeader API Version: %x.%02x.%02x\nDLL Version: %x.%02x.%02x\nCopyright Firelight Technologies Pty Ltd"), 
			(HeaderVersion>>16), (HeaderVersion>>8) & 0xFF, HeaderVersion & 0xFF,
			(DLLVersion>>16), (DLLVersion>>8) & 0xFF, DLLVersion & 0xFF);
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Message, TEXT("About"));
}

void FFMODStudioEditorModule::OpenCHM()
{
	FString APIPath = FPaths::Combine(*FPaths::EngineDir(), TEXT("Plugins/FMODStudio/Docs/FMOD UE4 Integration.chm"));
	if( IFileManager::Get().FileSize( *APIPath ) != INDEX_NONE )
	{
		FString AbsoluteAPIPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*APIPath);
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*AbsoluteAPIPath);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Documentation", "CannotFindFMODIntegration", "Cannot open FMOD Studio Integration CHM reference; help file not found."));
	}
}

void FFMODStudioEditorModule::OpenOnlineDocs()
{
	FPlatformProcess::LaunchFileInDefaultExternalApplication(TEXT("http://www.fmod.org/documentation"));
}

void FFMODStudioEditorModule::OpenVideoTutorials()
{
	FPlatformProcess::LaunchFileInDefaultExternalApplication(TEXT("http://www.youtube.com/user/FMODTV"));
}

void FFMODStudioEditorModule::ReloadBanks()
{
	// Pretend settings have changed which will force a reload
	IFMODStudioModule::Get().RefreshSettings();
}

bool FFMODStudioEditorModule::Tick( float DeltaTime )
{
	if (!bRegisteredComponentVisualizers && GUnrealEd != nullptr)
	{
		// Register component visualizers (GUnrealED is required for this, but not initialized before this module loads, so we have to wait until GUnrealEd is available)
		RegisterComponentVisualizer(UFMODAudioComponent::StaticClass()->GetFName(), MakeShareable(new FFMODAudioComponentVisualizer));

		bRegisteredComponentVisualizers = true;
	}

	return true;
}

void FFMODStudioEditorModule::BeginPIE(bool simulating)
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioEditorModule BeginPIE: %d"), simulating);
	bSimulating = simulating;
	bIsInPIE = true;
	IFMODStudioModule::Get().SetInPIE(true, simulating);
}

void FFMODStudioEditorModule::EndPIE(bool simulating)
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioEditorModule EndPIE: %d"), simulating);
	bSimulating = false;
	bIsInPIE = false;
	IFMODStudioModule::Get().SetInPIE(false, simulating);
}

void FFMODStudioEditorModule::PausePIE(bool simulating)
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioEditorModule PausePIE%d"));
	IFMODStudioModule::Get().SetSystemPaused(true);
}

void FFMODStudioEditorModule::ResumePIE(bool simulating)
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioEditorModule ResumePIE"));
	IFMODStudioModule::Get().SetSystemPaused(false);
}

void FFMODStudioEditorModule::PostLoadCallback()
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioEditorModule PostLoadCallback"));
}

void FFMODStudioEditorModule::ViewportDraw(UCanvas* Canvas, APlayerController*)
{
	// Only want to update based on viewport in simulate mode.
	// In PIE/game, we update from the player controller instead.
	if (!bSimulating)
	{
		return;
	}

	const FSceneView* View = Canvas->SceneView;
	
	if (View->Drawer == GCurrentLevelEditingViewportClient)
	{
		UWorld* World = GCurrentLevelEditingViewportClient->GetWorld();
		const FVector& ViewLocation = GCurrentLevelEditingViewportClient->GetViewLocation();

		FMatrix CameraToWorld = View->ViewMatrices.ViewMatrix.InverseFast();
		FVector ProjUp = CameraToWorld.TransformVector(FVector(0, 1000, 0));
		FVector ProjRight = CameraToWorld.TransformVector(FVector(1000, 0, 0));

		FTransform ListenerTransform(FRotationMatrix::MakeFromZY(ProjUp, ProjRight));
		ListenerTransform.SetTranslation(ViewLocation);
		ListenerTransform.NormalizeRotation();

		IFMODStudioModule::Get().SetListenerPosition(0, World, ListenerTransform, 0.0f);
		IFMODStudioModule::Get().FinishSetListenerPosition(1, 0.0f);
	}
}

void FFMODStudioEditorModule::ShutdownModule()
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioEditorModule shutdown"));

	if (UObjectInitialized())
	{
		// Unregister tick function.
		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

		FEditorDelegates::BeginPIE.Remove(BeginPIEDelegateHandle);
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
		FEditorDelegates::PausePIE.Remove(PausePIEDelegateHandle);
		FEditorDelegates::ResumePIE.Remove(ResumePIEDelegateHandle);

		if (ViewportDrawingDelegate.IsBound())
		{
			UDebugDrawService::Unregister(ViewportDrawingDelegateHandle);
		}

		FComponentAssetBrokerage::UnregisterBroker(AssetBroker);

		if (MainMenuExtender.IsValid())
		{
			FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>( "LevelEditor" );
			if (LevelEditorModule)
			{
				LevelEditorModule->GetMenuExtensibilityManager()->RemoveExtender(MainMenuExtender);
			}
		}
	}

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "FMODStudio");
	}

	// Unregister AssetTypeActions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		AssetTools.UnregisterAssetTypeActions(FMODEventAssetTypeActions.ToSharedRef());
	}

	// Unregister component visualizers
	if (GUnrealEd != nullptr)
	{
		// Iterate over all class names we registered for
		for (FName ClassName : RegisteredComponentClassNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(ClassName);
		}
	}

	IFMODStudioModule::Get().BanksReloadedEvent().Remove(HandleBanksReloadedDelegateHandle);
}

bool FFMODStudioEditorModule::HandleSettingsSaved()
{
	IFMODStudioModule::Get().RefreshSettings();
	
	return true;
}

void FFMODStudioEditorModule::HandleBanksReloaded()
{
	// Show a reload notification
	FNotificationInfo Info(LOCTEXT("FMODBanksReloaded", "FMOD Banks Reloaded"));
	Info.Image = FEditorStyle::GetBrush(TEXT("NoBrush"));
	Info.FadeInDuration = 0.1f;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 1.5f;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = true;
	Info.bUseLargeFont = true;
	Info.bFireAndForget = false;
	Info.bAllowThrottleWhenFrameRateIsLow = false;
	auto NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
	NotificationItem->ExpireAndFadeout();

	if (GCurrentLevelEditingViewportClient)
	{
		// Refresh any 3d event visualization
		GCurrentLevelEditingViewportClient->bNeedsRedraw = true;
	}
}

void FFMODStudioEditorModule::RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
{
	if (GUnrealEd != nullptr)
	{
		GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
	}

	RegisteredComponentClassNames.Add(ComponentClassName);

	if (Visualizer.IsValid())
	{
		Visualizer->OnRegister();
	}
}
