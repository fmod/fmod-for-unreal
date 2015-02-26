// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#include "FMODStudioPrivatePCH.h"

#include "FMODSettings.h"
#include "FMODStudioModule.h"
#include "FMODAudioComponent.h"
#include "FMODBlueprintStatics.h"
#include "FMODAssetTable.h"
#include "FMODFileCallbacks.h"
#include "FMODBankUpdateNotifier.h"
#include "FMODUtils.h"

#include "fmod_studio.hpp"

#if PLATFORM_PS4
#include "FMODPlatformLoadDll_PS4.h"
#elif PLATFORM_XBOXONE
#include "FMODPlatformLoadDll_XBoxOne.h"
#else
#include "FMODPlatformLoadDll_Generic.h"
#endif

#define LOCTEXT_NAMESPACE "FMODStudio"

DEFINE_LOG_CATEGORY(LogFMOD);

class FFMODStudioModule : public IFMODStudioModule
{
public:
	/** IModuleInterface implementation */
	FFMODStudioModule()
	:	AuditioningInstance(nullptr),
		bSimulating(false),
		bIsInPIE(false),
		bUseSound(true),
		bAllowLiveUpdate(true),
		LowLevelLibHandle(nullptr),
		StudioLibHandle(nullptr)
	{
		for (int i=0; i<EFMODSystemContext::Max; ++i)
		{
			StudioSystem[i] = nullptr;
		}
	}

	virtual void StartupModule() override;
	virtual void PostLoadCallback() override;
	virtual void ShutdownModule() override;

	FString GetDllPath(const TCHAR* ShortName);
	void* LoadDll(const TCHAR* ShortName);

	bool LoadLibraries();

	void LoadBanks(EFMODSystemContext::Type Type);

	/** Called when a newer version of the bank files was detected */
	void HandleBanksUpdated();

	void CreateStudioSystem(EFMODSystemContext::Type Type);
	void DestroyStudioSystem(EFMODSystemContext::Type Type);

	bool Tick( float DeltaTime );

	void UpdateViewportPosition();

	virtual FMOD::Studio::System* GetStudioSystem(EFMODSystemContext::Type Context) override;
	virtual FMOD::Studio::EventDescription* GetEventDescription(const UFMODEvent* Event, EFMODSystemContext::Type Type) override;
	virtual FMOD::Studio::EventInstance* CreateAuditioningInstance(const UFMODEvent* Event) override;
	virtual void StopAuditioningInstance() override;

	virtual void RefreshSettings();

	virtual void SetSystemPaused(bool paused) override;

	virtual void SetInPIE(bool bInPIE, bool simulating) override;

	virtual UFMODAsset* FindAssetByName(const FString& Name) override;
	virtual UFMODEvent* FindEventByName(const FString& Name) override;

	FSimpleMulticastDelegate BanksReloaded;
	virtual FSimpleMulticastDelegate& BanksReloadedEvent() override
	{
		return BanksReloaded;
	}

	virtual bool UseSound() override
	{
		return bUseSound;
	}


	/** The studio system handle. */
	FMOD::Studio::System* StudioSystem[EFMODSystemContext::Max];
	FMOD::Studio::EventInstance* AuditioningInstance;

	/** The delegate to be invoked when this profiler manager ticks. */
	FTickerDelegate OnTick;

	/** Handle for registered TickDelegate. */
	FDelegateHandle TickDelegateHandle;

	/** Table of assets with name and guid */
	FFMODAssetTable AssetTable;

	/** Periodically checks for updates of the strings.bank file */
	FFMODBankUpdateNotifier BankUpdateNotifier;

	/** True if simulating */
	bool bSimulating;
	
	/** True if in PIE */
	bool bIsInPIE;

	/** True if we want sound enabled */
	bool bUseSound;

	/** True if we allow live update */
	bool bAllowLiveUpdate;
	
	/** Dynamic library handles */
	void* LowLevelLibHandle;
	void* StudioLibHandle;
};

IMPLEMENT_MODULE( FFMODStudioModule, FMODStudio )

void* FFMODStudioModule::LoadDll(const TCHAR* ShortName)
{
	FString LibPath = GetDllPath(ShortName);

	void* Handle = nullptr;
	UE_LOG(LogFMOD, Log, TEXT("FFMODStudioModule::LoadDll: Loading %s"), *LibPath);
	// Unfortunately Unreal's platform loading code hasn't been implemented on all platforms so we wrap it
	Handle = FMODPlatformLoadDll(*LibPath);
#if WITH_EDITOR
	if (!Handle)
	{
		FString Message = TEXT("Couldn't load FMOD DLL ") + LibPath;
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Message, TEXT("Error"));
	}
#endif
	if (!Handle)
	{
		UE_LOG(LogFMOD, Error, TEXT("Failed to load FMOD DLL '%s', FMOD sounds will not play!"), *LibPath);
	}
	return Handle;
}

FString FFMODStudioModule::GetDllPath(const TCHAR* ShortName)
{
#if PLATFORM_MAC
	return FString::Printf(TEXT("%s/Binaries/ThirdParty/FMODStudio/Mac/lib%s.dylib"), *FPaths::EngineDir(), ShortName);
#elif PLATFORM_PS4
	return FString::Printf(TEXT("/app0/sce_sys/lib%s.prx"), ShortName);
#elif PLATFORM_XBOXONE
	return FString::Printf(TEXT("%s.dll"), ShortName);
#elif PLATFORM_64BITS
	return FString::Printf(TEXT("%s/Binaries/ThirdParty/FMODStudio/Win64/%s64.dll"), *FPaths::EngineDir(), ShortName);
#else
	return FString::Printf(TEXT("%s/Binaries/ThirdParty/FMODStudio/Win32/%s.dll"), *FPaths::EngineDir(), ShortName);
#endif
}

bool FFMODStudioModule::LoadLibraries()
{
#if PLATFORM_IOS || PLATFORM_ANDROID
	return true; // Nothing to do on those platforms
#else
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioModule::LoadLibraries"));

#if defined(FMODSTUDIO_LINK_DEBUG)
	FString ConfigName = TEXT("D");
#elif defined(FMODSTUDIO_LINK_LOGGING)
	FString ConfigName = TEXT("L");
#elif defined(FMODSTUDIO_LINK_RELEASE)
	FString ConfigName = TEXT("");
#else
	#error FMODSTUDIO_LINK not defined
#endif

	FString LowLevelName = FString(TEXT("fmod")) + ConfigName;
	FString StudioName = FString(TEXT("fmodstudio")) + ConfigName;
	LowLevelLibHandle = LoadDll(*LowLevelName);
	StudioLibHandle = LoadDll(*StudioName);
	return (LowLevelLibHandle != nullptr && StudioLibHandle != nullptr);
#endif
}

void FFMODStudioModule::StartupModule()
{
	UE_LOG(LogFMOD, Log, TEXT("FFMODStudioModule startup"));

	if(FParse::Param(FCommandLine::Get(),TEXT("nosound")) || FApp::IsBenchmarking() || IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		bUseSound = false;
	}

	if(FParse::Param(FCommandLine::Get(),TEXT("noliveupdate")))
	{
		bAllowLiveUpdate = false;
	}

	if (LoadLibraries())
	{
		// Create sandbox system just for asset loading
		AssetTable.Create();
		RefreshSettings();
		
		if (!GIsEditor)
		{
			SetInPIE(true, false);
		}
	}

	OnTick = FTickerDelegate::CreateRaw( this, &FFMODStudioModule::Tick );
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker( OnTick );

	if (GIsEditor)
	{
		BankUpdateNotifier.BanksUpdatedEvent.AddRaw(this, &FFMODStudioModule::HandleBanksUpdated);
	}
}

inline FMOD_SPEAKERMODE ConvertSpeakerMode(EFMODSpeakerMode::Type Mode)
{
	switch (Mode)
	{
		case EFMODSpeakerMode::Stereo:
			return FMOD_SPEAKERMODE_STEREO;
		case EFMODSpeakerMode::Surround_5_1:
			return FMOD_SPEAKERMODE_5POINT1;
		case EFMODSpeakerMode::Surround_7_1:
			return FMOD_SPEAKERMODE_7POINT1;
		default:
			check(0);
			return FMOD_SPEAKERMODE_DEFAULT;
	};
}

void FFMODStudioModule::CreateStudioSystem(EFMODSystemContext::Type Type)
{
	DestroyStudioSystem(Type);
	if (!bUseSound)
	{
		return;
	}

	UE_LOG(LogFMOD, Verbose, TEXT("CreateStudioSystem"));

	const UFMODSettings& Settings = *GetDefault<UFMODSettings>();

	FMOD_SPEAKERMODE OutputMode = ConvertSpeakerMode(Settings.OutputFormat);
	FMOD_STUDIO_INITFLAGS StudioInitFlags = FMOD_STUDIO_INIT_NORMAL;
	FMOD_INITFLAGS InitFlags = FMOD_INIT_NORMAL;
	if (Type == EFMODSystemContext::Auditioning)
	{
		StudioInitFlags |= FMOD_STUDIO_INIT_ALLOW_MISSING_PLUGINS;
	}
	else if (Type == EFMODSystemContext::Runtime && Settings.bEnableLiveUpdate && bAllowLiveUpdate)
	{
#if (defined(FMODSTUDIO_LINK_DEBUG) ||  defined(FMODSTUDIO_LINK_LOGGING))
		UE_LOG(LogFMOD, Verbose, TEXT("Enabling live update"));
		StudioInitFlags |= FMOD_STUDIO_INIT_LIVEUPDATE;
#endif
	}
	
	FMOD::Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_CALLBACK, FMODLogCallback);

	verifyfmod(FMOD::Studio::System::create(&StudioSystem[Type]));
	FMOD::System* lowLevelSystem = nullptr;
	verifyfmod(StudioSystem[Type]->getLowLevelSystem(&lowLevelSystem));
	verifyfmod(lowLevelSystem->setSoftwareFormat(0, OutputMode, 0));
	verifyfmod(lowLevelSystem->setFileSystem(FMODOpen, FMODClose, FMODRead, FMODSeek, 0, 0, 2048));
	verifyfmod(StudioSystem[Type]->initialize(256, StudioInitFlags, InitFlags, 0));

	// Don't bother loading plugins during editor, only during PIE or in game
	if (Type == EFMODSystemContext::Runtime)
	{
		for (FString PluginName : Settings.PluginFiles)
		{
			FString PluginPath = GetDllPath(*PluginName);
			UE_LOG(LogFMOD, Log, TEXT("Loading plugin '%s'"), *PluginPath);
			unsigned int Handle = 0;
			verifyfmod(lowLevelSystem->loadPlugin(TCHAR_TO_UTF8(*PluginPath), &Handle, 0));
		}
	}
}

void FFMODStudioModule::DestroyStudioSystem(EFMODSystemContext::Type Type)
{
	UE_LOG(LogFMOD, Verbose, TEXT("DestroyStudioSystem"));

	if (StudioSystem[Type])
	{
		verifyfmod(StudioSystem[Type]->release());
		StudioSystem[Type] = nullptr;
	}
}

bool FFMODStudioModule::Tick( float DeltaTime )
{
	if (GIsEditor)
	{
		BankUpdateNotifier.Update();
	}

	if (StudioSystem[EFMODSystemContext::Auditioning])
	{
		verifyfmod(StudioSystem[EFMODSystemContext::Auditioning]->update());
	}
	if (StudioSystem[EFMODSystemContext::Runtime])
	{
		UpdateViewportPosition();

		verifyfmod(StudioSystem[EFMODSystemContext::Runtime]->update());
	}

	return true;
}

void FFMODStudioModule::UpdateViewportPosition()
{
	int ListenerIndex = 0;

	UWorld* ViewportWorld = nullptr;
	if(GEngine && GEngine->GameViewport)
	{
		ViewportWorld = GEngine->GameViewport->GetWorld();
	}

	if (ViewportWorld)
	{
		for( FConstPlayerControllerIterator Iterator = ViewportWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = *Iterator;
			if( PlayerController )
			{
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
				if (LocalPlayer)
				{
					FVector Location;
					FVector ProjFront;
					FVector ProjRight;
					PlayerController->GetAudioListenerPosition(/*out*/ Location, /*out*/ ProjFront, /*out*/ ProjRight);
					FVector ProjUp = FVector::CrossProduct(ProjFront, ProjRight);

					FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));
					ListenerTransform.SetTranslation(Location);
					ListenerTransform.NormalizeRotation();

					FMOD_3D_ATTRIBUTES Attributes = {{0}};
					Attributes.position = FMODUtils::ConvertWorldVector(Location);
					Attributes.forward = FMODUtils::ConvertUnitVector(ProjFront);
					Attributes.up = FMODUtils::ConvertUnitVector(ProjUp);
					// For now, only apply the first listener position
					if (ListenerIndex == 0)
					{
						verifyfmod(StudioSystem[EFMODSystemContext::Runtime]->setListenerAttributes(&Attributes));
						break;
					}

					ListenerIndex++;
				}
			}
		}
	}
}

void FFMODStudioModule::RefreshSettings()
{
	AssetTable.Refresh();
	if (GIsEditor)
	{
		const UFMODSettings& Settings = *GetDefault<UFMODSettings>();
		BankUpdateNotifier.SetFilePath(Settings.GetMasterStringsBankPath());
	}
}


void FFMODStudioModule::SetInPIE(bool bInPIE, bool simulating)
{
	bIsInPIE = bInPIE;
	bSimulating = simulating;

	if (GIsEditor)
	{
		BankUpdateNotifier.EnableUpdate(!bInPIE);
	}

	if (bInPIE)
	{
		if (StudioSystem[EFMODSystemContext::Auditioning])
		{
			// We currently don't tear down auditioning system but we do stop the playing event.
			if (AuditioningInstance)
			{
				AuditioningInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
				AuditioningInstance = nullptr;
			}
			// Also make sure banks are finishing loading so they aren't grabbing file handles.
			StudioSystem[EFMODSystemContext::Auditioning]->flushCommands();
		}
		CreateStudioSystem(EFMODSystemContext::Runtime);
		LoadBanks(EFMODSystemContext::Runtime);
	}
	else
	{
		DestroyStudioSystem(EFMODSystemContext::Runtime);
	}
}

UFMODAsset* FFMODStudioModule::FindAssetByName(const FString& Name)
{
	return AssetTable.FindByName(Name);
}

UFMODEvent* FFMODStudioModule::FindEventByName(const FString& Name)
{
	UFMODAsset* Asset = AssetTable.FindByName(Name);
	return Cast<UFMODEvent>(Asset);
}

void FFMODStudioModule::SetSystemPaused(bool paused)
{
	if (StudioSystem[EFMODSystemContext::Runtime])
	{
		FMOD::System* LowLevelSystem = nullptr;
		verifyfmod(StudioSystem[EFMODSystemContext::Runtime]->getLowLevelSystem(&LowLevelSystem));
		FMOD::ChannelGroup* MasterChannelGroup = nullptr;
		verifyfmod(LowLevelSystem->getMasterChannelGroup(&MasterChannelGroup));
		verifyfmod(MasterChannelGroup->setPaused(paused));
	}
}

void FFMODStudioModule::PostLoadCallback()
{
}

void FFMODStudioModule::ShutdownModule()
{
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioModule shutdown"));

	DestroyStudioSystem(EFMODSystemContext::Auditioning);
	DestroyStudioSystem(EFMODSystemContext::Runtime);

	if (GIsEditor)
	{
		BankUpdateNotifier.BanksUpdatedEvent.RemoveAll(this);
	}

	if (UObjectInitialized())
	{
		// Unregister tick function.
		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}

	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioModule unloading dynamic libraries"));
	if (StudioLibHandle)
	{
		FPlatformProcess::FreeDllHandle(StudioLibHandle);
		StudioLibHandle = nullptr;
	}
	if (LowLevelLibHandle)
	{
		FPlatformProcess::FreeDllHandle(LowLevelLibHandle);
		LowLevelLibHandle = nullptr;
	}
	UE_LOG(LogFMOD, Verbose, TEXT("FFMODStudioModule finished unloading"));
}

void FFMODStudioModule::LoadBanks(EFMODSystemContext::Type Type)
{
	const UFMODSettings& Settings = *GetDefault<UFMODSettings>();
	if (StudioSystem[Type] != nullptr && Settings.IsBankPathSet())
	{
		/*
			If auditioning in editor, we load all banks asynchronously in the background and return immediately.  We force a flush
			whenever we want to audition anything, thus making sure they are all loaded at that point.

			For the actual runtime, we load the banks immediately because the game may want to query events at any time.  If this
			causes any stalls the user can always turn off the bank loading in the settings and do it via blueprints.
		*/
		FMOD_STUDIO_LOAD_BANK_FLAGS BankFlags = ((Type == EFMODSystemContext::Auditioning) ? FMOD_STUDIO_LOAD_BANK_NONBLOCKING : FMOD_STUDIO_LOAD_BANK_NORMAL);
		bool bLoadAllBanks = ((Type == EFMODSystemContext::Auditioning) || Settings.bLoadAllBanks);
		bool bLoadSampleData = ((Type == EFMODSystemContext::Runtime) && Settings.bLoadAllSampleData);

		// Always load the master bank at startup
		FString MasterBankPath = Settings.GetMasterBankPath();
		UE_LOG(LogFMOD, Verbose, TEXT("Loading master bank: %s"), *MasterBankPath);
		FMOD::Studio::Bank* MasterBank = nullptr;
		FMOD_RESULT Result;
		Result = StudioSystem[Type]->loadBankFile(TCHAR_TO_UTF8(*MasterBankPath), BankFlags, &MasterBank);
		if (Result != FMOD_OK)
		{
			UE_LOG(LogFMOD, Warning, TEXT("Failed to master bank: %s"), *MasterBankPath);
			return;
		}
		if (bLoadSampleData)
		{
			verifyfmod(MasterBank->loadSampleData());
		}

		// Auditioning needs string bank to get back full paths from events
		if (Type == EFMODSystemContext::Auditioning)
		{
			FString StringsBankPath = Settings.GetMasterStringsBankPath();
			UE_LOG(LogFMOD, Verbose, TEXT("Loading strings bank: %s"), *StringsBankPath);
			FMOD::Studio::Bank* StringsBank = nullptr;
			FMOD_RESULT Result;
			Result = StudioSystem[Type]->loadBankFile(TCHAR_TO_UTF8(*StringsBankPath), BankFlags, &StringsBank);
			if (Result != FMOD_OK)
			{
				UE_LOG(LogFMOD, Warning, TEXT("Failed to strings bank: %s"), *MasterBankPath);
			}
		}

		// Optionally load all banks in the directory
		if (bLoadAllBanks)
		{
			UE_LOG(LogFMOD, Verbose, TEXT("Loading all banks"));
			TArray<FString> BankFiles;
			Settings.GetAllBankPaths(BankFiles);
			for ( const FString& OtherFile : BankFiles )
			{
				FMOD::Studio::Bank* OtherBank;
				FMOD_RESULT Result = StudioSystem[Type]->loadBankFile(TCHAR_TO_UTF8(*OtherFile), BankFlags, &OtherBank);
				if (Result == FMOD_OK)
				{
					if (bLoadSampleData)
					{
						verifyfmod(OtherBank->loadSampleData());
					}
				}
				else
				{
					UE_LOG(LogFMOD, Warning, TEXT("Failed to load bank (Error %d): %s"), (int32)Result, *OtherFile);
				}
			}
		}
	}
}

void FFMODStudioModule::HandleBanksUpdated()
{
	DestroyStudioSystem(EFMODSystemContext::Auditioning);

	AssetTable.Refresh();

	CreateStudioSystem(EFMODSystemContext::Auditioning);
	LoadBanks(EFMODSystemContext::Auditioning);

	BanksReloaded.Broadcast();
}

FMOD::Studio::System* FFMODStudioModule::GetStudioSystem(EFMODSystemContext::Type Context)
{
	if (Context == EFMODSystemContext::Max)
	{
		Context = (bIsInPIE ? EFMODSystemContext::Runtime : EFMODSystemContext::Auditioning);
	}
	return StudioSystem[Context];
}


FMOD::Studio::EventDescription* FFMODStudioModule::GetEventDescription(const UFMODEvent* Event, EFMODSystemContext::Type Context)
{
	if (Context == EFMODSystemContext::Max)
	{
		Context = (bIsInPIE ? EFMODSystemContext::Runtime : EFMODSystemContext::Auditioning);
	}
	if (StudioSystem[Context] != nullptr && Event != nullptr && Event->AssetGuid.IsValid())
	{
		if (Context == EFMODSystemContext::Auditioning)
		{
			// Because we loaded auditioning system asynchronously, do a flush here to make sure banks have 
			// actually finished loading so we can query events in them.
			StudioSystem[Context]->flushCommands();
		}

		FMOD::Studio::ID Guid = FMODUtils::ConvertGuid(Event->AssetGuid);
		FMOD::Studio::EventDescription* EventDesc = nullptr;
		StudioSystem[Context]->getEventByID(&Guid, &EventDesc);
		return EventDesc;
	}
	return nullptr;
}

FMOD::Studio::EventInstance* FFMODStudioModule::CreateAuditioningInstance(const UFMODEvent* Event)
{
	StopAuditioningInstance();

	FMOD::Studio::EventDescription* EventDesc = GetEventDescription(Event, EFMODSystemContext::Auditioning);
	if (EventDesc)
	{
		FMOD_RESULT Result = EventDesc->createInstance(&AuditioningInstance);
		if (Result == FMOD_OK)
		{
			return AuditioningInstance;
		}
	}
	return nullptr;
}

void FFMODStudioModule::StopAuditioningInstance()
{
	if (AuditioningInstance)
	{
		// Don't bother checking for errors just in case auditioning is already shutting down
		AuditioningInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
		AuditioningInstance->release();
		AuditioningInstance = nullptr;
	}
}
