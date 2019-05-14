// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2019.

#include "FMODSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

//////////////////////////////////////////////////////////////////////////
// UPaperRuntimeSettings

UFMODSettings::UFMODSettings(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
    MasterBankName = TEXT("Master");
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
    FileBufferSize = 2048;
    StudioUpdatePeriod = 0;
    LiveUpdatePort = 9264;
    EditorLiveUpdatePort = 9265;
    bMatchHardwareSampleRate = true;
    bLockAllBuses = false;
}

FString UFMODSettings::GetFullBankPath() const
{
    FString FullPath = BankOutputDirectory.Path;
    if (FPaths::IsRelative(FullPath))
    {
        FullPath = FPaths::ProjectContentDir() / FullPath;
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
#elif PLATFORM_SWITCH
        FString PlatformName = "Switch";
#else
        FString PlatformName = "Desktop";
#endif
        FullPath = FullPath / PlatformName;
    }
    return FullPath;
}

FString UFMODSettings::GetMasterBankFilename() const
{
    return MasterBankName + TEXT(".bank");
}

FString UFMODSettings::GetMasterAssetsBankFilename() const
{
    return MasterBankName + TEXT(".assets.bank");
}

FString UFMODSettings::GetMasterStringsBankFilename() const
{
    return MasterBankName + TEXT(".strings.bank");
}

void UFMODSettings::GetAllBankPaths(TArray<FString> &Paths, bool IncludeMasterBank) const
{
    FString BankDir = GetFullBankPath();
    FString SearchDir = BankDir;

    TArray<FString> AllFiles;
    IFileManager::Get().FindFilesRecursive(AllFiles, *SearchDir, TEXT("*.bank"), true, false, false);

    for (FString &CurFile : AllFiles)
    {
        bool Skip = false;

        if (!IncludeMasterBank)
        {
            FString CurFilename = FPaths::GetCleanFilename(CurFile);
            Skip = (CurFilename == GetMasterBankFilename() || CurFilename == GetMasterAssetsBankFilename() || CurFilename == GetMasterStringsBankFilename());
        }

        if (!Skip)
        {
            Paths.Push(CurFile);
        }
    }
}
