// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

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
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IPAddress.h"

#include "fmod_studio.hpp"

#define LOCTEXT_NAMESPACE "FMODStudio"

DEFINE_LOG_CATEGORY(LogFMOD);

class FFMODStudioLink
{
public:
	FFMODStudioLink() 
	:	SocketSubsystem(nullptr),
		Socket(nullptr)
	{ 
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	}

	~FFMODStudioLink()
	{
		Disconnect();
	}

	bool Connect()
	{
		if (!SocketSubsystem) return false;

		Disconnect();
		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("FMOD Studio Connection"), false);

		TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
		bool Valid = false;
		Addr->SetIp(TEXT("127.0.0.1"), Valid);
		if (!Valid) return false;

		Addr->SetPort(3663);
		return Socket->Connect(*Addr);
	}

	void Disconnect()
	{
		if (SocketSubsystem && Socket)
		{
			SocketSubsystem->DestroySocket(Socket);
			Socket = nullptr;
		}
	}

	bool Execute(const TCHAR* Message, FString& OutMessage)
	{
		OutMessage = TEXT("");
		if (!Socket)
		{
			return false;
		}

		UE_LOG(LogFMOD, Log, TEXT("Sent studio message: %s"), Message);

		FTCHARToUTF8 MessageChars(Message);
		int32 BytesSent = 0;
		if (!Socket->Send((const uint8*)MessageChars.Get(), MessageChars.Length(), BytesSent))
		{
			return false;
		}

		while (1)
		{
			FString BackMessage;
			if (!ReadMessage(BackMessage))
			{
				return false;
			}
			UE_LOG(LogFMOD, Log, TEXT("Received studio message: %s"), *BackMessage);
			if (BackMessage.StartsWith(TEXT("out(): ")))
			{
				OutMessage = BackMessage.Mid(7).TrimTrailing();
				break;
			}
			else
			{
				// Keep going 
			}
		}
		return true;
	}
private:
	bool ReadMessage(FString& OutMessage)
	{
		while (1)
		{
			for (int32 i=0; i<ReceivedMessage.Num(); ++i)
			{
				if (ReceivedMessage[i] == '\0')
				{
					OutMessage = FString(UTF8_TO_TCHAR(ReceivedMessage.GetData()));
					ReceivedMessage.RemoveAt(0, i + 1);
					return true;
				}
			}

			int32 ExtraSpace = 64;
			int32 CurrentSize = ReceivedMessage.Num();
			ReceivedMessage.SetNum(CurrentSize + ExtraSpace);
			int32 ActualRead = 0;
			if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(10)))
			{
				return false;
			}
			else if (!Socket->Recv((uint8*)ReceivedMessage.GetData() + CurrentSize, ExtraSpace, ActualRead))
			{
				return false;
			}
			ReceivedMessage.SetNum(CurrentSize + ActualRead);
		}
	}

	ISocketSubsystem* SocketSubsystem;
	FSocket* Socket;
	TArray<char> ReceivedMessage;
};

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

	/** Show notification */
	void ShowNotification(const FText& Text, SNotificationItem::ECompletionState State);

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
	/** Set Studio build path */
	void ValidateFMOD();

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

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FMODSetStudioBuildTitle", "Validate FMOD"),
		LOCTEXT("FMODSetStudioBuildToolTip", "Verifies that FMOD and FMOD Studio are working as expected."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FFMODStudioEditorModule::ValidateFMOD)));

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

unsigned int GetDLLVersion()
{
	// Just grab it from the audition context which is always valid
	unsigned int DLLVersion = 0;
	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Auditioning);
	if (StudioSystem)
	{
		FMOD::System* LowLevelSystem = nullptr;
		if (StudioSystem->getLowLevelSystem(&LowLevelSystem) == FMOD_OK)
		{
			LowLevelSystem->getVersion(&DLLVersion);
		}
	}
	return DLLVersion;
}
unsigned int StripPatch(unsigned int FullVersion)
{
	return FullVersion & ~0xFF;
}
FString VersionToString(unsigned int FullVersion)
{
	return FString::Printf(TEXT("%x.%02x.%02x"), (FullVersion>>16), (FullVersion>>8) & 0xFF, FullVersion & 0xFF);
}

void FFMODStudioEditorModule::ShowVersion()
{
	unsigned int HeaderVersion = FMOD_VERSION;
	unsigned int DLLVersion = GetDLLVersion();

	FText VersionMessage = FText::Format(
		LOCTEXT("FMODStudio_About", "FMOD Studio\n\nBuilt Version: {0}\nDLL Version: {1}\n\nCopyright Firelight Technologies Pty Ltd"),
			FText::FromString(VersionToString(HeaderVersion)),
			FText::FromString(VersionToString(DLLVersion)));
	FMessageDialog::Open(EAppMsgType::Ok, VersionMessage);
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

void FFMODStudioEditorModule::ValidateFMOD()
{
	int ProblemsFound = 0;

	FFMODStudioLink StudioLink;
	bool Connected = StudioLink.Connect();
	if (!Connected)
	{
		if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SetStudioBuildStudioNotRunning", "FMODStudio does not appear to be running.  Only some validation will occur.  Do you want to continue anyway?")))
		{
			return;
		}
	}
	unsigned int HeaderVersion = FMOD_VERSION;
	unsigned int DLLVersion = GetDLLVersion();
	unsigned int StudioVersion = 0;
	if (Connected)
	{
		FString StudioVersionString;
		if (StudioLink.Execute(TEXT("studio.version"), StudioVersionString))
		{
			// We expect something like "Version xx.yy.zz, 32/64, Some build number"
			UE_LOG(LogFMOD, Log, TEXT("Received studio version: %s"), *StudioVersionString);
			TArray<FString> VersionParts;
			if (StudioVersionString.StartsWith(TEXT("Version ")) &&
				StudioVersionString.ParseIntoArray(VersionParts, TEXT(",")) >= 1)
			{
				TArray<FString> VersionFields;
				VersionParts[0].RightChop(8).ParseIntoArray(VersionFields, TEXT("."));
				if (VersionFields.Num() == 3)
				{
					int Super = FCString::Atoi(*VersionFields[0]);
					int Major = FCString::Atoi(*VersionFields[1]);
					int Minor = FCString::Atoi(*VersionFields[2]);
					StudioVersion = (Super << 16) | (Major << 8) | Minor;
				}
			}
		}
	}
	if (StripPatch(HeaderVersion) != StripPatch(DLLVersion))
	{
		FText VersionMessage = FText::Format(
			LOCTEXT("SetStudioBuildStudio_Status", "The FMOD DLL version is different to the version the integration was built against.  This may cause problems running the game.\nBuilt Version: {0}\nDLL Version: {1}\n"),
				FText::FromString(VersionToString(HeaderVersion)),
				FText::FromString(VersionToString(DLLVersion)));
		FMessageDialog::Open(EAppMsgType::Ok, VersionMessage);
		ProblemsFound++;
	}
	if (StudioVersion > DLLVersion)
	{
		FText VersionMessage = FText::Format(
			LOCTEXT("SetStudioBuildStudio_Version", "The Studio tool is newer than the version the integration was built against.  The integration may not be able to load the banks that the tool builds.\n\nBuilt Version: {0}\nDLL Version: {1}\nStudio Version: {2}\n\nWe recommend using the Studio tool that matches the integration.\n\nDo you want to continue with the validation?"),
				FText::FromString(VersionToString(HeaderVersion)),
				FText::FromString(VersionToString(DLLVersion)),
				FText::FromString(VersionToString(StudioVersion)));
		if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, VersionMessage))
		{
			return;
		}
		ProblemsFound++;
	}

	const UFMODSettings& Settings = *GetDefault<UFMODSettings>();
	FString FullBankPath = Settings.BankOutputDirectory.Path;
	if (FPaths::IsRelative(FullBankPath))
	{
		FullBankPath = FPaths::GameContentDir() / FullBankPath;
	}
	FString PlatformBankPath = Settings.GetFullBankPath();
	FullBankPath = FPaths::ConvertRelativePathToFull(FullBankPath);
	PlatformBankPath = FPaths::ConvertRelativePathToFull(PlatformBankPath);

	if (Connected)
	{
		// File path was added in FMOD Studio 1.07.00
		FString StudioProjectPath;
		FString StudioProjectDir;
		if (StudioVersion >= 0x00010700)
		{
			StudioLink.Execute(TEXT("studio.project.filePath"), StudioProjectPath);
			if (StudioProjectPath.IsEmpty() || StudioProjectPath == TEXT("undefined"))
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetStudioBuildStudio_NewProject", "FMOD Studio has an empty project.  Please go to FMOD Studio, and press Save to create your new project."));
				// Just try to save anyway
				FString Result;
				StudioLink.Execute(TEXT("studio.project.save()"), Result);
			}
			StudioLink.Execute(TEXT("studio.project.filePath"), StudioProjectPath);
			if (StudioProjectPath != TEXT("undefined"))
			{
				StudioProjectDir = FPaths::GetPath(StudioProjectPath);
			}
		}

		FString StudioPathString;
		StudioLink.Execute(TEXT("studio.project.workspace.builtBanksOutputDirectory"), StudioPathString);
		if (StudioPathString == TEXT("undefined"))
		{
			StudioPathString = TEXT("");
		}

		FString CanonicalBankPath = FullBankPath;
		FPaths::CollapseRelativeDirectories(CanonicalBankPath);
		FPaths::NormalizeDirectoryName(CanonicalBankPath);
		FPaths::RemoveDuplicateSlashes(CanonicalBankPath);
		FPaths::NormalizeDirectoryName(CanonicalBankPath);

		FString CanonicalStudioPath = StudioPathString;
		if (FPaths::IsRelative(CanonicalStudioPath) && !StudioProjectDir.IsEmpty() && !StudioPathString.IsEmpty())
		{
			CanonicalStudioPath = FPaths::Combine(*StudioProjectDir, *CanonicalStudioPath);
		}
		FPaths::CollapseRelativeDirectories(CanonicalStudioPath);
		FPaths::NormalizeDirectoryName(CanonicalStudioPath);
		FPaths::RemoveDuplicateSlashes(CanonicalStudioPath);
		FPaths::NormalizeDirectoryName(CanonicalStudioPath);
		if (!FPaths::IsSamePath(CanonicalBankPath, CanonicalStudioPath))
		{
			FString BankPathToSet = FullBankPath;
			// Extra logic - if we have put the studio project inside the game project, then make it relative
			if (!StudioProjectDir.IsEmpty())
			{
				FString GameBaseDir = FPaths::ConvertRelativePathToFull(FPaths::GameDir());
				FString BankPathFromGameProject = FullBankPath;
				FString StudioProjectFromGameProject = StudioProjectDir;
				if (FPaths::MakePathRelativeTo(BankPathFromGameProject, *GameBaseDir) && !BankPathFromGameProject.Contains(TEXT("..")) &&
					FPaths::MakePathRelativeTo(StudioProjectFromGameProject, *GameBaseDir) && !StudioProjectFromGameProject.Contains(TEXT("..")))
				{
					FPaths::MakePathRelativeTo(BankPathToSet, *(StudioProjectDir + TEXT("/")));
				}
			}
			ProblemsFound++;

			FText AskMessage = FText::Format(
					LOCTEXT("SetStudioBuildStudio_Ask", "FMOD Studio build path should be set up.\n\nCurrent Studio build path: {0}\nNew build path: {1}\n\nDo you want to fix up the project now?"),
					FText::FromString(StudioPathString),
					FText::FromString(BankPathToSet));
			if (EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, AskMessage))
			{
				FString Result;
				StudioLink.Execute(*FString::Printf(TEXT("studio.project.workspace.builtBanksOutputDirectory = \"%s\";"), *BankPathToSet), Result);
				StudioLink.Execute(TEXT("studio.project.workspace.builtBanksOutputDirectory"), Result);
				if (Result != BankPathToSet)
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetStudioBuildStudio_Save", "Failed to set bank directory.  Please go to FMOD Studio, and set the bank path in FMOD Studio project settings."));
				}
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetStudioBuildStudio_Save", "Please go to FMOD Studio, save your project and build banks."));
				// Just try to do it again anyway
				StudioLink.Execute(TEXT("studio.project.save()"), Result);
				StudioLink.Execute(TEXT("studio.project.build()"), Result);
				// Pretend settings have changed which will force a reload
				IFMODStudioModule::Get().RefreshSettings();
			}
		}
	}
	bool AnyBankFiles = false;

	if (!FPaths::DirectoryExists(FullBankPath) || !FPaths::DirectoryExists(PlatformBankPath))
	{
		FText DirMessage = FText::Format(
				LOCTEXT("SetStudioBuildStudio_Dir", "The FMOD Content directory does not exist.  Please make sure FMOD Studio is exporting banks into the correct location.\n\nBanks should be exported to: {0}\nBanks files should exist in: {1}\n"),
				FText::FromString(FullBankPath),
				FText::FromString(PlatformBankPath));
		FMessageDialog::Open(EAppMsgType::Ok, DirMessage);
		ProblemsFound++;
	}
	else
	{
		TArray<FString> BankFiles;
		Settings.GetAllBankPaths(BankFiles, true);
		if (BankFiles.Num() != 0)
		{
			AnyBankFiles = true;
		}
		else
		{
			FText EmptyBankDirMessage = FText::Format(
					LOCTEXT("SetStudioBuildStudio_EmptyBankDir", "The FMOD Content directory does not have any bank files in them.  Please make sure FMOD Studio is exporting banks into the correct location.\n\nBanks should be exported to: {0}\nBanks files should exist in: {1}\n"),
					FText::FromString(FullBankPath),
					FText::FromString(PlatformBankPath));
			FMessageDialog::Open(EAppMsgType::Ok, EmptyBankDirMessage);
			ProblemsFound++;
		}
	}
	if (AnyBankFiles)
	{
		FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Auditioning);
		int BankCount = 0;
		StudioSystem->getBankCount(&BankCount);
		TArray<FString> FailedBanks = IFMODStudioModule::Get().GetFailedBankLoads(EFMODSystemContext::Auditioning);
		if (BankCount == 0 || FailedBanks.Num() != 0)
		{
			FString CombinedBanks;
			for (auto Bank : FailedBanks)
			{
				CombinedBanks += Bank;
				CombinedBanks += TEXT("\n");
			}
			FText BankLoadMessage;
			if (BankCount == 0 && FailedBanks.Num() == 0)
			{
				BankLoadMessage = LOCTEXT("SetStudioBuildStudio_BankLoad", "Failed to load banks\n");
			}
			else if (BankCount == 0)
			{
				BankLoadMessage = FText::Format(
						LOCTEXT("SetStudioBuildStudio_BankLoad", "Failed to load banks:\n{0}\n"),
						FText::FromString(CombinedBanks));
			}
			else
			{
				BankLoadMessage = FText::Format(
						LOCTEXT("SetStudioBuildStudio_BankLoad", "Some banks failed to load:\n{0}\n"),
						FText::FromString(CombinedBanks));
			}
			FMessageDialog::Open(EAppMsgType::Ok, BankLoadMessage);
			ProblemsFound++;
		}
		else
		{
			int TotalEventCount = 0;
			TArray<FMOD::Studio::Bank*> Banks;
			Banks.SetNum(BankCount);
			StudioSystem->getBankList(Banks.GetData(), BankCount, &BankCount);
			for (FMOD::Studio::Bank* Bank : Banks)
			{
				int EventCount = 0;
				Bank->getEventCount(&EventCount);
				TotalEventCount += EventCount;
			}
			if (TotalEventCount == 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetStudioBuildStudio_NoEvents", "Banks have been loaded but they didn't have any events in them.  Please make sure you have added some events to banks."));
				ProblemsFound++;
			}
		}
	}
	TArray<FString> RequiredPlugins = IFMODStudioModule::Get().GetRequiredPlugins();
	if (RequiredPlugins.Num() != 0 && Settings.PluginFiles.Num() == 0)
	{
		FString CombinedPlugins;
		for (auto Name : RequiredPlugins)
		{
			CombinedPlugins += Name;
			CombinedPlugins += TEXT("\n");
		}
		FText PluginMessage = FText::Format(
				LOCTEXT("SetStudioBuildStudio_Plugins", "The banks require the following plugins, but no plugin filenames are listed in the settings:\n{0}\n"),
				FText::FromString(CombinedPlugins));
		FMessageDialog::Open(EAppMsgType::Ok, PluginMessage);
		ProblemsFound++;
	}

	if (ProblemsFound)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetStudioBuildStudio_FinishedBad", "Finished validation.  Problems were detected.\n"));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetStudioBuildStudio_FinishedGood", "Finished validation.  No problems detected.\n"));
	}
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
	TArray<FString> FailedBanks = IFMODStudioModule::Get().GetFailedBankLoads(EFMODSystemContext::Auditioning);
	FText Message;
	SNotificationItem::ECompletionState State;
	if (FailedBanks.Num() == 0)
	{
		Message = LOCTEXT("FMODBanksReloaded", "Reloaded FMOD Banks\n");
		State = SNotificationItem::CS_Success;
	}
	else
	{
		FString CombinedMessage = "Problem loading FMOD Banks:";
		for (auto Entry : FailedBanks)
		{
			CombinedMessage += TEXT("\n");
			CombinedMessage += Entry;

			UE_LOG(LogFMOD, Warning, TEXT("Problem loading FMOD Bank: %s"), *Entry);
		}

		Message = FText::Format(
				LOCTEXT("FMODBanksReloaded", "{0}"),
				FText::FromString(CombinedMessage));
		State = SNotificationItem::CS_Fail;
	}
	ShowNotification(Message, State);
}

void FFMODStudioEditorModule::ShowNotification(const FText& Text, SNotificationItem::ECompletionState State)
{
	FNotificationInfo Info(Text);
	Info.Image = FEditorStyle::GetBrush(TEXT("NoBrush"));
	Info.FadeInDuration = 0.1f;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = State == SNotificationItem::CS_Fail ? 6.0f : 1.5f;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = true;
	Info.bUseLargeFont = true;
	Info.bFireAndForget = false;
	Info.bAllowThrottleWhenFrameRateIsLow = false;
	auto NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationItem->SetCompletionState(State);
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
