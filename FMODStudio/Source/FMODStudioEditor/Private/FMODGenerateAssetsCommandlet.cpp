// Copyright (c), Firelight Technologies Pty, Ltd.

#include "FMODGenerateAssetsCommandlet.h"

#include "FMODSettings.h"
#include "AssetRegistryModule.h"
#include "Editor.h"
#include "Editor/UnrealEd/Public/FileHelpers.h"
#include "HAL/PlatformFilemanager.h"
#include "../Classes/FMODAssetBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogFMODCommandlet, Log, All);

static constexpr auto RebuildSwitch = TEXT("rebuild");

UFMODGenerateAssetsCommandlet::UFMODGenerateAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

int32 UFMODGenerateAssetsCommandlet::Main(const FString& CommandLine)
{
    int32 returnCode = 0;

#if WITH_EDITOR

    FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    IAssetRegistry& AssetRegistry = assetRegistryModule.Get();

    const UFMODSettings& Settings = *GetDefault<UFMODSettings>();

    TArray<FString> Tokens, Switches;
    TMap<FString, FString> Params;
    ParseCommandLine(*CommandLine, Tokens, Switches, Params);

    // Rebuild switch
    if (Switches.Contains(RebuildSwitch))
    {
        FString FolderToDelete;
        IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
        for (FString folder : Settings.GeneratedFolders)
        {
            FolderToDelete = FPaths::ProjectContentDir() + Settings.ContentBrowserPrefix + folder;
            bool removed = FileManager.DeleteDirectoryRecursively(*FolderToDelete);
            if (!removed)
            {
                UE_LOG(LogFMODCommandlet, Warning, TEXT("Unable to delete '%s'."), *FolderToDelete);
            }
        }
    }

    // Ensure AssetRegistry is up to date
    TArray<FString> InPaths;
    InPaths.Add(Settings.GetFullContentPath());
    AssetRegistry.ScanPathsSynchronous(InPaths);
    while (AssetRegistry.IsLoadingAssets())
    {
        AssetRegistry.Tick(1.0f);
    }

    FFMODAssetBuilder assetBuilder;
    if (!IsEngineExitRequested())
    {
        assetBuilder.Create();
        assetBuilder.ProcessBanks();
    }
#endif

	return returnCode;
}