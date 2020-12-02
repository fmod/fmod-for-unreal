// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2020.

#include "FMODAssetTable.h"
#include "FMODEvent.h"
#include "FMODSnapshot.h"
#include "FMODSnapshotReverb.h"
#include "FMODBank.h"
#include "FMODBus.h"
#include "FMODVCA.h"
#include "FMODUtils.h"
#include "FMODSettings.h"
#include "FMODFileCallbacks.h"
#include "FMODStudioPrivatePCH.h"
#include "fmod_studio.hpp"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "AssetRegistryModule.h"

#if WITH_EDITOR
#include "ObjectTools.h"
#endif

FFMODAssetTable::FFMODAssetTable()
    : StudioSystem(nullptr)
{
}

FFMODAssetTable::~FFMODAssetTable()
{
    Destroy();
}

void FFMODAssetTable::Create()
{
    Destroy();

    // Create a sandbox system purely for loading and considering banks
    verifyfmod(FMOD::Studio::System::create(&StudioSystem));
    FMOD::System *lowLevelSystem = nullptr;
    verifyfmod(StudioSystem->getCoreSystem(&lowLevelSystem));
    verifyfmod(lowLevelSystem->setOutput(FMOD_OUTPUTTYPE_NOSOUND));
    AttachFMODFileSystem(lowLevelSystem, 2048);
    verifyfmod(
        StudioSystem->initialize(1, FMOD_STUDIO_INIT_ALLOW_MISSING_PLUGINS | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE, FMOD_INIT_MIX_FROM_UPDATE, 0));
}

void FFMODAssetTable::Destroy()
{
    if (StudioSystem != nullptr)
    {
        verifyfmod(StudioSystem->release());
    }
    StudioSystem = nullptr;
}

UFMODAsset *FFMODAssetTable::FindByName(const FString &Name) const
{
    const TWeakObjectPtr<UFMODAsset> *FoundAsset = NameLookup.Find(Name);
    if (FoundAsset)
    {
        return FoundAsset->Get();
    }
    return nullptr;
}

void FFMODAssetTable::Refresh()
{
    if (StudioSystem == nullptr)
    {
        return;
    }

    BuildBankPathLookup();

    if (!MasterStringsBankPath.IsEmpty())
    {
        const UFMODSettings &Settings = *GetDefault<UFMODSettings>();
        FString StringPath = Settings.GetFullBankPath() / MasterStringsBankPath;

        UE_LOG(LogFMOD, Log, TEXT("Loading strings bank: %s"), *StringPath);

        FMOD::Studio::Bank *StudioStringBank;
        FMOD_RESULT StringResult = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*StringPath), FMOD_STUDIO_LOAD_BANK_NORMAL, &StudioStringBank);

        if (StringResult == FMOD_OK)
        {
            TArray<char> RawBuffer;
            RawBuffer.SetNum(256); // Initial capacity

            int Count = 0;
            verifyfmod(StudioStringBank->getStringCount(&Count));

            // Enumerate all of the names in the strings bank and gather the information required to create the UE4 assets for each object
            TArray<AssetCreateInfo> AssetCreateInfos;
            AssetCreateInfos.Reserve(Count);

            for (int StringIdx = 0; StringIdx < Count; ++StringIdx)
            {
                FMOD::Studio::ID Guid = { 0 };

                while (true)
                {
                    int ActualSize = 0;
                    FMOD_RESULT Result = StudioStringBank->getStringInfo(StringIdx, &Guid, RawBuffer.GetData(), RawBuffer.Num(), &ActualSize);

                    if (Result == FMOD_ERR_TRUNCATED)
                    {
                        RawBuffer.SetNum(ActualSize);
                    }
                    else
                    {
                        verifyfmod(Result);
                        break;
                    }
                }

                FString AssetName(UTF8_TO_TCHAR(RawBuffer.GetData()));
                FGuid AssetGuid = FMODUtils::ConvertGuid(Guid);

                if (!AssetName.IsEmpty())
                {
                    AssetCreateInfo CreateInfo = {};

                    if (MakeAssetCreateInfo(AssetGuid, AssetName, &CreateInfo))
                    {
                        AssetCreateInfos.Add(CreateInfo);
                    }
                }
            }

            verifyfmod(StudioStringBank->unload());
            verifyfmod(StudioSystem->update());

            // Create new asset map - move existing assets over if they still match
            TMap<FString, TWeakObjectPtr<UFMODAsset>> NewNameLookup;

            for (const AssetCreateInfo &CreateInfo : AssetCreateInfos)
            {
                TWeakObjectPtr<UFMODAsset> Asset;
                NameLookup.RemoveAndCopyValue(CreateInfo.StudioPath, Asset);

                if (Asset.IsValid() && Asset->GetClass() == CreateInfo.Class && Asset->AssetGuid == CreateInfo.Guid)
                {
                    NewNameLookup.Add(CreateInfo.StudioPath, Asset);
                }
                else
                {
                    // Clean up existing asset (if there was one)
                    if (Asset.IsValid())
                    {
                        DeleteAsset(Asset.Get());
                    }

                    UFMODAsset *NewAsset = CreateAsset(CreateInfo);
                    NewNameLookup.Add(CreateInfo.StudioPath, NewAsset);
                    FAssetRegistryModule::AssetCreated(NewAsset);
                }
            }

            // Everything left in the name lookup was removed
            for (auto& Entry : NameLookup)
            {
                DeleteAsset(Entry.Value.Get());
            }

            NameLookup = NewNameLookup;
        }
        else
        {
            UE_LOG(LogFMOD, Warning, TEXT("Failed to load strings bank: %s"), *StringPath);
        }
    }
}

FString FFMODAssetTable::GetAssetClassName(UClass* AssetClass)
{
    FString ClassName("");

    if (AssetClass == UFMODEvent::StaticClass())
    {
        ClassName = TEXT("Events");
    }
    else if (AssetClass == UFMODSnapshot::StaticClass())
    {
        ClassName = TEXT("Snapshots");
    }
    else if (AssetClass == UFMODBank::StaticClass())
    {
        ClassName = TEXT("Banks");
    }
    else if (AssetClass == UFMODBus::StaticClass())
    {
        ClassName = TEXT("Buses");
    }
    else if (AssetClass == UFMODVCA::StaticClass())
    {
        ClassName = TEXT("VCAs");
    }
    else if (AssetClass == UFMODSnapshotReverb::StaticClass())
    {
        ClassName = TEXT("Reverbs");
    }
    return ClassName;
}

bool FFMODAssetTable::MakeAssetCreateInfo(const FGuid &AssetGuid, const FString &StudioPath, AssetCreateInfo *CreateInfo)
{
    CreateInfo->StudioPath = StudioPath;
    CreateInfo->Guid = AssetGuid;

    FString AssetType;
    FString AssetPath;
    StudioPath.Split(TEXT(":"), &AssetType, &AssetPath, ESearchCase::CaseSensitive, ESearchDir::FromStart);

    if (AssetType.Equals(TEXT("event")))
    {
        CreateInfo->Class = UFMODEvent::StaticClass();
    }
    else if (AssetType.Equals(TEXT("snapshot")))
    {
        CreateInfo->Class = UFMODSnapshot::StaticClass();
    }
    else if (AssetType.Equals(TEXT("bank")))
    {
        CreateInfo->Class = UFMODBank::StaticClass();
    }
    else if (AssetType.Equals(TEXT("bus")))
    {
        CreateInfo->Class = UFMODBus::StaticClass();
    }
    else if (AssetType.Equals(TEXT("vca")))
    {
        CreateInfo->Class = UFMODVCA::StaticClass();
    }
    else if (AssetType.Equals(TEXT("parameter")))
    {
        return false;
    }
    else
    {
        UE_LOG(LogFMOD, Warning, TEXT("Unknown asset type: %s"), *AssetType);
        CreateInfo->Class = UFMODAsset::StaticClass();
    }

    AssetPath.Split(TEXT("/"), &(CreateInfo->Path), &(CreateInfo->AssetName), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

    if (CreateInfo->AssetName.IsEmpty() || CreateInfo->AssetName.Contains(TEXT(".strings")))
    {
        return false;
    }
    return true;
}


// These SanitizeXXX functions are copied from ObjectTools - we currently use this at runtime and ObjectTools is not available. If we start
// serializing our generated assets when cooking then we can revert to using the versions in ObjectTools and remove these local copies.
static FString SanitizeInvalidChars(const FString &InObjectName, const FString &InvalidChars)
{
    FString SanitizedName;

    // See if the name contains invalid characters.
    FString Char;
    for (int32 CharIdx = 0; CharIdx < InObjectName.Len(); ++CharIdx)
    {
        Char = InObjectName.Mid(CharIdx, 1);

        if (InvalidChars.Contains(*Char))
        {
            SanitizedName += TEXT("_");
        }
        else
        {
            SanitizedName += Char;
        }
    }

    return SanitizedName;
}

static FString SanitizeObjectName(const FString &InObjectName)
{
    return SanitizeInvalidChars(InObjectName, INVALID_OBJECTNAME_CHARACTERS);
}

UFMODAsset *FFMODAssetTable::CreateAsset(const AssetCreateInfo& CreateInfo)
{
    FString SanitizedAssetName;
    FText OutReason;

    if (FName::IsValidXName(CreateInfo.AssetName, INVALID_OBJECTNAME_CHARACTERS, &OutReason))
    {
        SanitizedAssetName = CreateInfo.AssetName;
    }
    else
    {
        SanitizedAssetName = SanitizeObjectName(CreateInfo.AssetName);
        UE_LOG(LogFMOD, Warning, TEXT("'%s' cannot be used as a UE4 asset name. %s. Using '%s' instead."), *CreateInfo.AssetName,
            *OutReason.ToString(), *SanitizedAssetName);
    }

    const UFMODSettings &Settings = *GetDefault<UFMODSettings>();
    FString Folder = Settings.ContentBrowserPrefix + GetAssetClassName(CreateInfo.Class) + CreateInfo.Path;
    FString PackagePath = FString::Printf(TEXT("%s/%s"), *Folder, *SanitizedAssetName);
    FString SanitizedPackagePath;

    if (FName::IsValidXName(PackagePath, INVALID_LONGPACKAGE_CHARACTERS, &OutReason))
    {
        SanitizedPackagePath = PackagePath;
    }
    else
    {
        SanitizedPackagePath = SanitizeInvalidChars(PackagePath, INVALID_OBJECTPATH_CHARACTERS);
        UE_LOG(LogFMOD, Warning, TEXT("'%s' cannot be used as a UE4 asset path. %s. Using '%s' instead."), *PackagePath, *OutReason.ToString(),
            *SanitizedPackagePath);
    }

    UE_LOG(LogFMOD, Log, TEXT("Constructing asset: %s"), *SanitizedPackagePath);

    UFMODAsset *Asset = nullptr;
    EObjectFlags NewObjectFlags = RF_Standalone | RF_Public;

    if (IsRunningDedicatedServer())
    {
        NewObjectFlags |= RF_MarkAsRootSet;
    }

    UPackage *NewPackage = CreatePackage(nullptr, *SanitizedPackagePath);
    EPackageFlags NewPackageFlags = GEventDrivenLoaderEnabled ? PKG_None : PKG_CompiledIn;

    if (IsValid(NewPackage))
    {
        NewPackage->SetPackageFlags(NewPackageFlags);
        Asset = NewObject<UFMODAsset>(NewPackage, CreateInfo.Class, FName(*SanitizedAssetName), NewObjectFlags);

        if (IsValid(Asset))
        {
            FAssetRegistryModule::AssetCreated(Asset);
            Asset->AssetGuid = CreateInfo.Guid;
        }
    }
    
    if (!IsValid(Asset))
    {
        UE_LOG(LogFMOD, Warning, TEXT("Failed to construct asset: %s"), *SanitizedPackagePath);
    }

    if (CreateInfo.Class == UFMODSnapshot::StaticClass())
    {
        FString OldPrefix = Settings.ContentBrowserPrefix + GetAssetClassName(Asset->GetClass());
        FString NewPrefix = Settings.ContentBrowserPrefix + GetAssetClassName(UFMODSnapshotReverb::StaticClass());
        UObject *Outer = Asset->GetOuter() ? Asset->GetOuter() : Asset;
        FString ReverbPackagePath = Outer->GetPathName().Replace(*OldPrefix, *NewPrefix);

        UE_LOG(LogFMOD, Log, TEXT("Constructing snapshot reverb asset: %s"), *ReverbPackagePath);

        UPackage *ReverbPackage = CreatePackage(nullptr, *ReverbPackagePath);
        UFMODSnapshotReverb *AssetReverb = nullptr;

        if (IsValid(ReverbPackage))
        {
            ReverbPackage->SetPackageFlags(NewPackageFlags);
            AssetReverb = NewObject<UFMODSnapshotReverb>(ReverbPackage, UFMODSnapshotReverb::StaticClass(), FName(*SanitizedAssetName), NewObjectFlags);

            if (IsValid(AssetReverb))
            {
                FAssetRegistryModule::AssetCreated(AssetReverb);
                AssetReverb->AssetGuid = CreateInfo.Guid;
            }
        }

        if (!IsValid(AssetReverb))
        {
            UE_LOG(LogFMOD, Warning, TEXT("Failed to construct snapshot reverb asset: %s"), *ReverbPackagePath);
        }
    }

    return Asset;
}

void FFMODAssetTable::DeleteAsset(UObject *Asset)
{
#if WITH_EDITOR
    if (Asset)
    {
        TArray<UObject *> ObjectsToDelete;
        ObjectsToDelete.Add(Asset);

        if (Asset->GetClass() == UFMODSnapshot::StaticClass())
        {
            // Also delete the reverb asset
            const UFMODSettings &Settings = *GetDefault<UFMODSettings>();
            FString OldPrefix = Settings.ContentBrowserPrefix + GetAssetClassName(Asset->GetClass());
            FString NewPrefix = Settings.ContentBrowserPrefix + GetAssetClassName(UFMODSnapshotReverb::StaticClass());
            FString ReverbName = Asset->GetPathName().Replace(*OldPrefix, *NewPrefix);
            UObject *Reverb = StaticFindObject(UFMODSnapshotReverb::StaticClass(), nullptr, *ReverbName);

            if (Reverb)
            {
                ObjectsToDelete.Add(Reverb);
            }
        }

        ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
    }
#endif
}

FString FFMODAssetTable::GetBankPathByGuid(const FGuid& Guid) const
{
    FString BankPath = "";
    const FString* File = nullptr;
    const BankLocalizations* localizations = BankPathLookup.Find(Guid);

    if (localizations)
    {
        const FString* DefaultFile = nullptr;

        for (int i = 0; i < localizations->Num(); ++i)
        {
            if ((*localizations)[i].Locale.IsEmpty())
            {
                DefaultFile = &(*localizations)[i].Path;
            }
            else if ((*localizations)[i].Locale == ActiveLocale)
            {
                File = &(*localizations)[i].Path;
                break;
            }
        }

        if (!File)
        {
            File = DefaultFile;
        }
    }

    if (File)
    {
        BankPath = *File;
    }

    return BankPath;
}

FString FFMODAssetTable::GetBankPath(const UFMODBank &Bank) const
{
    FString BankPath = GetBankPathByGuid(Bank.AssetGuid);

    if (BankPath.IsEmpty())
    {
        UE_LOG(LogFMOD, Warning, TEXT("Could not find disk file for bank %s"), *Bank.GetName());
    }

    return BankPath;
}

FString FFMODAssetTable::GetMasterBankPath() const
{
    return MasterBankPath;
}

FString FFMODAssetTable::GetMasterStringsBankPath() const
{
    return MasterStringsBankPath;
}

FString FFMODAssetTable::GetMasterAssetsBankPath() const
{
    return MasterAssetsBankPath;
}

void FFMODAssetTable::SetLocale(const FString &LocaleCode)
{
    ActiveLocale = LocaleCode;
}

void FFMODAssetTable::GetAllBankPaths(TArray<FString> &Paths, bool IncludeMasterBank) const
{
    const UFMODSettings &Settings = *GetDefault<UFMODSettings>();

    for (const TMap<FGuid, BankLocalizations>::ElementType& Localizations : BankPathLookup)
    {
        FString BankPath = GetBankPathByGuid(Localizations.Key);
        bool Skip = false;

        if (BankPath.IsEmpty())
        {
            // Never expect to be in here, but should skip empty paths
            continue;
        }

        if (!IncludeMasterBank)
        {
            Skip = (BankPath == Settings.GetMasterBankFilename() || BankPath == Settings.GetMasterAssetsBankFilename() || BankPath == Settings.GetMasterStringsBankFilename());
        }

        if (!Skip)
        {
            Paths.Push(Settings.GetFullBankPath() / BankPath);
        }
    }
}


void FFMODAssetTable::GetAllBankPathsFromDisk(const FString &BankDir, TArray<FString> &Paths)
{
    FString SearchDir = BankDir;

    TArray<FString> AllFiles;
    IFileManager::Get().FindFilesRecursive(AllFiles, *SearchDir, TEXT("*.bank"), true, false, false);

    for (FString &CurFile : AllFiles)
    {
        Paths.Push(CurFile);
    }
}

void FFMODAssetTable::BuildBankPathLookup()
{
    const UFMODSettings &Settings = *GetDefault<UFMODSettings>();

    TArray<FString> BankPaths;
    GetAllBankPathsFromDisk(Settings.GetFullBankPath(), BankPaths);

    BankPathLookup.Empty(BankPaths.Num());
    MasterBankPath.Empty();
    MasterStringsBankPath.Empty();
    MasterAssetsBankPath.Empty();

    if (BankPaths.Num() == 0)
    {
        return;
    }

    for (FString BankPath : BankPaths)
    {
        FMOD::Studio::Bank *Bank;
        FMOD_RESULT result = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*BankPath), FMOD_STUDIO_LOAD_BANK_NORMAL, &Bank);
        FMOD_GUID GUID;

        if (result == FMOD_OK)
        {
            result = Bank->getID(&GUID);
            Bank->unload();
        }

        if (result == FMOD_OK)
        {
            FString CurFilename = FPaths::GetCleanFilename(BankPath);
            FString PathPart;
            FString FilenamePart;
            FString ExtensionPart;
            FPaths::Split(BankPath, PathPart, FilenamePart, ExtensionPart);
            BankPath = BankPath.RightChop(Settings.GetFullBankPath().Len() + 1);

            BankLocalization localization;
            localization.Path = BankPath;
            localization.Locale = "";

            for (const FFMODProjectLocale& Locale : Settings.Locales)
            {
                if (FilenamePart.EndsWith(FString("_") + Locale.LocaleCode))
                {
                    localization.Locale = Locale.LocaleCode;
                    break;
                }
            }

            BankLocalizations& localizations = BankPathLookup.FindOrAdd(FMODUtils::ConvertGuid(GUID));
            localizations.Add(localization);

            if (MasterBankPath.IsEmpty() && CurFilename == Settings.GetMasterBankFilename())
            {
                MasterBankPath = BankPath;
            }
            else if (MasterStringsBankPath.IsEmpty() && CurFilename == Settings.GetMasterStringsBankFilename())
            {
                MasterStringsBankPath = BankPath;
            }
            else if (MasterAssetsBankPath.IsEmpty() && CurFilename == Settings.GetMasterAssetsBankFilename())
            {
                MasterAssetsBankPath = BankPath;
            }
        }

        if (result != FMOD_OK)
        {
            UE_LOG(LogFMOD, Error, TEXT("Failed to register disk file for bank: %s"), *BankPath);
        }
    }

    StudioSystem->flushCommands();
}
