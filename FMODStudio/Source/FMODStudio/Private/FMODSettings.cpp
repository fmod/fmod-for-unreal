// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2021.

#include "FMODSettings.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Settings/ProjectPackagingSettings.h"
#endif

#ifdef FMOD_PLATFORM_HEADER
#include "FMODPlatform.h"
#endif

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
    ReloadBanksDelay = 5;
    bMatchHardwareSampleRate = true;
    bLockAllBuses = false;
    bEnableMemoryTracking = false;
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
#ifdef FMOD_PLATFORM_HEADER
        FString PlatformName = FMODPlatform_PlatformName();
#elif PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_ANDROID
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

#if WITH_EDITOR
FString UFMODSettings::GetDesktopBankPath() const
{
    FString Path = BankOutputDirectory.Path;

    if (ForcePlatformName.IsEmpty())
    {
        Path = Path / "Desktop";
    }
    else if (ForcePlatformName != TEXT("."))
    {
        Path = Path / ForcePlatformName;
    }

    return Path;
}

UFMODSettings::EProblem UFMODSettings::Check() const
{
    if (!IsBankPathSet())
    {
        return BankPathNotSet;
    }

    // Check packaging settings to ensure that only the correct bank output directory for desktop (or forced platform) banks is set-up for staging
    FString DesktopBankPath = GetDesktopBankPath();
    UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
    bool bCorrectPathAdded = false;
    bool bOtherPathsAdded = false;

    for (int i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsNonUFS.Num(); ++i)
    {
        if (PackagingSettings->DirectoriesToAlwaysStageAsNonUFS[i].Path.StartsWith(BankOutputDirectory.Path))
        {
            if (PackagingSettings->DirectoriesToAlwaysStageAsNonUFS[i].Path == DesktopBankPath)
            {
                bCorrectPathAdded = true;
            }
            else
            {
                bOtherPathsAdded = true;
            }
        }
    }

    for (int i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); ++i)
    {
        if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path.StartsWith(BankOutputDirectory.Path))
        {
            bOtherPathsAdded = true;
            break;
        }
    }

    if (!bCorrectPathAdded || bOtherPathsAdded)
    {
        return PackagingSettingsBad;
    }

    return Okay;
}
#endif // WITH_EDITOR
