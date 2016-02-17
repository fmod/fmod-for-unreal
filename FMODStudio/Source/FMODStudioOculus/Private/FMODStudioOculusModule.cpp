// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioOculusPrivatePCH.h"

#include "FMODStudioOculusModule.h"
#include "FMODStudioModule.h"
#include "FMODOculusSettings.h"
#include "FMODOculusRoomParameters.h"
#include "FMODOculusBlueprintStatics.h"
#include "FMODUtils.h"

#include "fmod.hpp"
#include "fmod_studio.hpp"

#if FMOD_OSP_SUPPORTED
	#include "OculusFMODSpatializerSettings.h"
#endif

#if WITH_EDITOR
	#include "Internationalization.h"
	#include "ISettingsModule.h"
	#include "ISettingsSection.h"
#endif

class FFMODStudioOculusModule : public IFMODStudioOculusModule
{
public:
	/** IModuleInterface implementation */
	FFMODStudioOculusModule()
	{
		bRunning = false;
	}

	virtual void StartupModule() override;
	virtual void PostLoadCallback() override;
	virtual void ShutdownModule() override;
	virtual bool IsRunning() override;

	bool HandleSettingsSaved();
	virtual void OnInitialize() override;

	bool bRunning;

};

IMPLEMENT_MODULE( FFMODStudioOculusModule, FMODStudioOculus )

#define LOCTEXT_NAMESPACE "FMODStudioOculus"

DEFINE_LOG_CATEGORY(LogFMODOculus);

void FFMODStudioOculusModule::StartupModule()
{
	UE_LOG(LogFMODOculus, Log, TEXT("StartupModule"));

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "FMODStudioOculus",
			LOCTEXT("FMODStudioOculusSettingsName", "FMOD Studio Oculus"),
			LOCTEXT("FMODStudioOculusDescription", "Configure the FMOD Studio Oculus plugin"),
			GetMutableDefault<UFMODOculusSettings>()
		);

		if (SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FFMODStudioOculusModule::HandleSettingsSaved);
		}
	}
#endif
}

void FFMODStudioOculusModule::PostLoadCallback()
{
	UE_LOG(LogFMODOculus, Log, TEXT("PostLoadCallback"));
}

void FFMODStudioOculusModule::ShutdownModule()
{
	UE_LOG(LogFMODOculus, Log, TEXT("ShutdownModule"));
}

void FFMODStudioOculusModule::OnInitialize()
{
	UE_LOG(LogFMODOculus, Verbose, TEXT("OnInitialize"));

#if FMOD_OSP_SUPPORTED
	const UFMODOculusSettings& Settings = *GetDefault<UFMODOculusSettings>();
	if (!Settings.bOculusEnabled)
	{
		UE_LOG(LogFMODOculus, Verbose, TEXT("bOculusEnabled is false - skipping init"));
		return;
	}

	FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (!StudioSystem)
	{
		UE_LOG(LogFMODOculus, Verbose, TEXT("StudioSystem is null - skipping init"));
		return;
	}

	if (!IFMODStudioModule::Get().LoadPlugin(TEXT("ovrfmod")))
	{
		UE_LOG(LogFMODOculus, Verbose, TEXT("ovrfmod failed to load - skipping init"));

		return;
	}

	UE_LOG(LogFMODOculus, Verbose, TEXT("Initialising OSP"));

	FMOD::System* LowLevelSystem = nullptr;
	verifyfmod(StudioSystem->getLowLevelSystem(&LowLevelSystem));
	if (LowLevelSystem)
	{
		int SampleRate = 0;
		unsigned int BufferSize = 0;
		verifyfmod(LowLevelSystem->getSoftwareFormat(&SampleRate, nullptr, nullptr));
		verifyfmod(LowLevelSystem->getDSPBufferSize(&BufferSize, nullptr));
		if (OSP_FMOD_Initialize(SampleRate, BufferSize))
		{
			bRunning = true;
			// Unreal units are per cm, but our plugin converts everything to metres when calling into Studio
			OSP_FMOD_SetGlobalScale(1.0f);
			UFMODOculusBlueprintStatics::SetEarlyReflectionsEnabled(Settings.bEarlyReflectionsEnabled);
			UFMODOculusBlueprintStatics::SetLateReverberationEnabled(Settings.bLateReverberationEnabled);
			UFMODOculusBlueprintStatics::SetRoomParameters(Settings.RoomParameters);
			UE_LOG(LogFMODOculus, Verbose, TEXT("OSP_FMOD_Initialize returned true - running with Oculus plugin enabled"));
		}
		else
		{
			UE_LOG(LogFMODOculus, Error, TEXT("OSP_FMOD_Initialize returned false"));
		}
	}
#else
	UE_LOG(LogFMODOculus, Log, TEXT("FMOD_OSP_SUPPORTED is not enabled"));
#endif
}

bool FFMODStudioOculusModule::IsRunning()
{
	return bRunning;
}

bool FFMODStudioOculusModule::HandleSettingsSaved()
{
	UE_LOG(LogFMODOculus, Verbose, TEXT("HandleSettingsSaved"));
	return true;
}
