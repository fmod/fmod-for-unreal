// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioPrivatePCH.h"
#include "FMODSettings.h"

//////////////////////////////////////////////////////////////////////////
// UPaperRuntimeSettings

UFMODSettings::UFMODSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MasterBankName = TEXT("Master Bank");
	BankOutputDirectory.Path = TEXT("FMOD");
	OutputFormat = EFMODSpeakerMode::Surround_5_1;
	ContentBrowserPrefix = TEXT("/Game/FMOD/");
	bLoadAllBanks = true;
	bLoadAllSampleData = false;
	bEnableLiveUpdate = true;
	bVol0Virtual = true;
	Vol0VirtualLevel = 0.0001f;
	RealChannelCount = 64;
	TotalChannelCount = 512;
	DSPBufferLength = 0;
	DSPBufferCount = 0;
	StudioUpdatePeriod = 0;
	LiveUpdatePort = 0;
	bMatchHardwareSampleRate = true;
}

FString UFMODSettings::GetFullBankPath() const
{
	FString FullPath = BankOutputDirectory.Path;
	if (FPaths::IsRelative(FullPath))
	{
		FullPath = FPaths::GameContentDir() / FullPath;
	}

	if (ForcePlatformName == TEXT("."))
	{
		// Leave path without subdirectory
	}
	else if (!ForcePlatformName.IsEmpty())
	{
		FullPath = FullPath / ForcePlatformName;
	}
	else
	{
#if PLATFORM_IOS || PLATFORM_ANDROID
		FString PlatformName = "Mobile";
#elif PLATFORM_PS4
		FString PlatformName = "PS4";
#elif PLATFORM_XBOXONE
		FString PlatformName = "XboxOne";
#else
		FString PlatformName = "Desktop";
#endif
		FullPath = FullPath / PlatformName;
	}
	return FullPath;
}

FString UFMODSettings::GetMasterBankPath() const
{
	return GetFullBankPath() / (MasterBankName + TEXT(".bank"));
}

FString UFMODSettings::GetMasterStringsBankPath() const
{
	return GetFullBankPath() / (MasterBankName + TEXT(".strings.bank"));
}

void UFMODSettings::GetAllBankPaths(TArray<FString>& Paths, bool IncludeMasterBank) const
{
	FString BankDir = GetFullBankPath();
	FString SearchDir = BankDir / FString(TEXT("*"));

	TArray<FString> AllFiles;
	IFileManager::Get().FindFiles(AllFiles, *SearchDir, true, false);
	for ( FString& CurFile : AllFiles )
	{
		if (CurFile.EndsWith(".bank"))
		{
			bool IsMaster = (CurFile == MasterBankName + TEXT(".bank") ||
							 CurFile == MasterBankName + TEXT(".strings.bank"));
			if (IncludeMasterBank || !IsMaster)
			{
				Paths.Push(BankDir / CurFile);
			}
		}
	}
}

