#include "FMODAssetBuilder.h"

#include "AssetRegistryModule.h"
#include "FMODAssetLookup.h"
#include "FMODAssetTable.h"
#include "FMODBank.h"
#include "FMODBankLookup.h"
#include "FMODBus.h"
#include "FMODEvent.h"
#include "FMODSettings.h"
#include "FMODSnapshot.h"
#include "FMODSnapshotReverb.h"
#include "FMODUtils.h"
#include "FMODVCA.h"
#include "HAL/FileManager.h"
#include "ObjectTools.h"

#include "fmod_studio.hpp"

FFMODAssetBuilder::~FFMODAssetBuilder()
{
    if (StudioSystem)
    {
        StudioSystem->release();
    }
}

void FFMODAssetBuilder::Create()
{
    verifyfmod(FMOD::Studio::System::create(&StudioSystem));
    FMOD::System *lowLevelSystem = nullptr;
    verifyfmod(StudioSystem->getCoreSystem(&lowLevelSystem));
    verifyfmod(lowLevelSystem->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT));
    verifyfmod(StudioSystem->initialize(1, FMOD_STUDIO_INIT_ALLOW_MISSING_PLUGINS | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE, FMOD_INIT_MIX_FROM_UPDATE, nullptr));
}

void FFMODAssetBuilder::ProcessBanks()
{
    const UFMODSettings& Settings = *GetDefault<UFMODSettings>();
    FString PackagePath = Settings.ContentBrowserPrefix + FFMODAssetTable::PrivateDataPath();
    BuildBankLookup(FFMODAssetTable::BankLookupName(), PackagePath, Settings);
    BuildAssets(Settings);
    BuildAssetLookup(FFMODAssetTable::AssetLookupName(), PackagePath);
}

FString FFMODAssetBuilder::GetMasterStringsBankPath()
{
    return BankLookup ? BankLookup->MasterStringsBankPath : FString();
}

void FFMODAssetBuilder::BuildAssets(const UFMODSettings& InSettings)
{
    if (!BankLookup->MasterStringsBankPath.IsEmpty())
    {
        FString StringPath = InSettings.GetFullBankPath() / BankLookup->MasterStringsBankPath;

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

void FFMODAssetBuilder::BuildBankLookup(const FString &AssetName, const FString &PackagePath, const UFMODSettings &InSettings)
{
    FString PackageName = PackagePath + AssetName;
    UPackage *Package = CreatePackage(*PackageName);
    Package->FullyLoad();

    bool bCreated = false;
    BankLookup = FindObject<UFMODBankLookup>(Package, *AssetName, true);

    if (BankLookup)
    {
        BankLookup->MasterBankPath.Empty();
        BankLookup->MasterStringsBankPath.Empty();
        BankLookup->MasterAssetsBankPath.Empty();
        BankLookup->DataTable->EmptyTable();
    }
    else
    {
        BankLookup = NewObject<UFMODBankLookup>(Package, *AssetName, RF_Public | RF_Standalone | RF_MarkAsRootSet);
        BankLookup->DataTable = NewObject<UDataTable>(BankLookup, "DataTable", RF_NoFlags);
        BankLookup->DataTable->RowStruct = FFMODLocalizedBankTable::StaticStruct();
        bCreated = true;
    }

    TArray<FString> BankPaths;
    FString SearchDir = InSettings.GetFullBankPath();
    IFileManager::Get().FindFilesRecursive(BankPaths, *SearchDir, TEXT("*.bank"), true, false, false);

    for (FString BankPath : BankPaths)
    {
        FMOD::Studio::Bank *Bank;
        FMOD_RESULT result = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*BankPath), FMOD_STUDIO_LOAD_BANK_NORMAL, &Bank);
        FMOD_GUID BankID;

        if (result == FMOD_OK)
        {
            result = Bank->getID(&BankID);
            Bank->unload();
        }

        if (result == FMOD_OK)
        {
            FString GUID = FMODUtils::ConvertGuid(BankID).ToString(EGuidFormats::DigitsWithHyphensInBraces);
            FName OuterRowName(*GUID);
            FFMODLocalizedBankTable *Row = BankLookup->DataTable->FindRow<FFMODLocalizedBankTable>(OuterRowName, nullptr, false);

            if (!Row)
            {
                FFMODLocalizedBankTable NewRow{};
                NewRow.Banks = NewObject<UDataTable>(BankLookup->DataTable, *GUID, RF_NoFlags);
                NewRow.Banks->RowStruct = FFMODLocalizedBankRow::StaticStruct();
                BankLookup->DataTable->AddRow(OuterRowName, NewRow);
                Row = BankLookup->DataTable->FindRow<FFMODLocalizedBankTable>(OuterRowName, nullptr, false);
            }

            FString CurFilename = FPaths::GetCleanFilename(BankPath);
            FString PathPart;
            FString FilenamePart;
            FString ExtensionPart;
            FPaths::Split(BankPath, PathPart, FilenamePart, ExtensionPart);
            FString RelativeBankPath = BankPath.RightChop(InSettings.GetFullBankPath().Len() + 1);

            FFMODLocalizedBankRow InnerRow{};
            InnerRow.Path = RelativeBankPath;
            FString InnerRowName("<NON-LOCALIZED>");

            for (const FFMODProjectLocale &Locale : InSettings.Locales)
            {
                if (FilenamePart.EndsWith(FString("_") + Locale.LocaleCode))
                {
                    InnerRowName = Locale.LocaleCode;
                    break;
                }
            }

            Row->Banks->AddRow(FName(*InnerRowName), InnerRow);

            if (BankLookup->MasterBankPath.IsEmpty() && CurFilename == InSettings.GetMasterBankFilename())
            {
                BankLookup->MasterBankPath = RelativeBankPath;
            }
            else if (BankLookup->MasterStringsBankPath.IsEmpty() && CurFilename == InSettings.GetMasterStringsBankFilename())
            {
                BankLookup->MasterStringsBankPath = RelativeBankPath;
            }
            else if (BankLookup->MasterAssetsBankPath.IsEmpty() && CurFilename == InSettings.GetMasterAssetsBankFilename())
            {
                BankLookup->MasterAssetsBankPath = RelativeBankPath;
            }
        }
        else
        {
            UE_LOG(LogFMOD, Error, TEXT("Failed to add bank %s to lookup."), *BankPath);
        }
    }

    StudioSystem->flushCommands();

    Package->MarkPackageDirty();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

    if (!UPackage::SavePackage(Package, BankLookup, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr,
        true, true, SAVE_None))
    {
        UE_LOG(LogFMOD, Error, TEXT("Failed to save package %s."), *PackageFileName);
    }

    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(BankLookup);
    }
}

void FFMODAssetBuilder::BuildAssetLookup(const FString &AssetName, const FString &PackagePath)
{
    FString PackageName = PackagePath + AssetName;
    UPackage *Package = CreatePackage(*PackageName);
    Package->FullyLoad();

    UDataTable *AssetLookup = NewObject<UDataTable>(Package, *AssetName, RF_Public | RF_Standalone | RF_MarkAsRootSet);
    AssetLookup->RowStruct = FFMODAssetLookupRow::StaticStruct();

    for (auto& Entry : NameLookup)
    {
        UFMODAsset *Asset = Entry.Value.Get();
        UPackage *AssetPackage = Asset->GetPackage();
        FFMODAssetLookupRow Row{};
        Row.PackageName = AssetPackage->GetPathName();
        Row.AssetName = Asset->GetPathName(AssetPackage);
        AssetLookup->AddRow(FName(*Entry.Key), Row);
    }

    Package->MarkPackageDirty();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

    if (!UPackage::SavePackage(Package, AssetLookup, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr,
        true, true, SAVE_None))
    {
        UE_LOG(LogFMOD, Error, TEXT("Failed to save package %s."), *PackageFileName);
    }

    FAssetRegistryModule::AssetCreated(AssetLookup);
}

FString FFMODAssetBuilder::GetAssetClassName(UClass* AssetClass)
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

bool FFMODAssetBuilder::MakeAssetCreateInfo(const FGuid &AssetGuid, const FString &StudioPath, AssetCreateInfo *CreateInfo)
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

UFMODAsset *FFMODAssetBuilder::CreateAsset(const AssetCreateInfo& CreateInfo)
{
    FString SanitizedAssetName;
    FText OutReason;

    if (FName::IsValidXName(CreateInfo.AssetName, INVALID_OBJECTNAME_CHARACTERS, &OutReason))
    {
        SanitizedAssetName = CreateInfo.AssetName;
    }
    else
    {
        SanitizedAssetName = ObjectTools::SanitizeObjectName(CreateInfo.AssetName);
        UE_LOG(LogFMOD, Log, TEXT("'%s' cannot be used as a UE4 asset name. %s. Using '%s' instead."), *CreateInfo.AssetName,
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
        SanitizedPackagePath = ObjectTools::SanitizeInvalidChars(PackagePath, INVALID_OBJECTPATH_CHARACTERS);
        UE_LOG(LogFMOD, Log, TEXT("'%s' cannot be used as a UE4 asset path. %s. Using '%s' instead."), *PackagePath, *OutReason.ToString(),
            *SanitizedPackagePath);
    }

    UE_LOG(LogFMOD, Log, TEXT("Constructing asset: %s"), *SanitizedPackagePath);

    UFMODAsset *Asset = nullptr;
    EObjectFlags NewObjectFlags = RF_Standalone | RF_Public | RF_MarkAsRootSet;
    UPackage *NewPackage = CreatePackage(*SanitizedPackagePath);

    if (IsValid(NewPackage))
    {
        NewPackage->FullyLoad();
        Asset = NewObject<UFMODAsset>(NewPackage, CreateInfo.Class, FName(*SanitizedAssetName), NewObjectFlags);

        if (IsValid(Asset))
        {
            Asset->AssetGuid = CreateInfo.Guid;
            NewPackage->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(Asset);
            FString PackageFileName = FPackageName::LongPackageNameToFilename(SanitizedPackagePath, FPackageName::GetAssetPackageExtension());

            if (!UPackage::SavePackage(NewPackage, Asset, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr,
                true, true, SAVE_None))
            {
                UE_LOG(LogFMOD, Error, TEXT("Failed to save package %s."), *PackageFileName);
            }
        }
    }

    if (!IsValid(Asset))
    {
        UE_LOG(LogFMOD, Error, TEXT("Failed to construct asset: %s"), *SanitizedPackagePath);
    }

    if (CreateInfo.Class == UFMODSnapshot::StaticClass())
    {
        FString OldPrefix = Settings.ContentBrowserPrefix + GetAssetClassName(Asset->GetClass());
        FString NewPrefix = Settings.ContentBrowserPrefix + GetAssetClassName(UFMODSnapshotReverb::StaticClass());
        UObject *Outer = Asset->GetOuter() ? Asset->GetOuter() : Asset;
        FString ReverbPackagePath = Outer->GetPathName().Replace(*OldPrefix, *NewPrefix);

        UE_LOG(LogFMOD, Log, TEXT("Constructing snapshot reverb asset: %s"), *ReverbPackagePath);

        UPackage *ReverbPackage = CreatePackage(*ReverbPackagePath);
        UFMODSnapshotReverb *AssetReverb = nullptr;

        if (IsValid(ReverbPackage))
        {
            ReverbPackage->FullyLoad();
            AssetReverb = NewObject<UFMODSnapshotReverb>(ReverbPackage, UFMODSnapshotReverb::StaticClass(), FName(*SanitizedAssetName), NewObjectFlags);

            if (IsValid(AssetReverb))
            {
                AssetReverb->AssetGuid = CreateInfo.Guid;
                ReverbPackage->MarkPackageDirty();
                FAssetRegistryModule::AssetCreated(AssetReverb);
                FString PackageFileName = FPackageName::LongPackageNameToFilename(ReverbPackagePath, FPackageName::GetAssetPackageExtension());

                if (!UPackage::SavePackage(ReverbPackage, AssetReverb, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr,
                    true, true, SAVE_None))
                {
                    UE_LOG(LogFMOD, Error, TEXT("Failed to save package %s."), *PackageFileName);
                }
            }
        }

        if (!IsValid(AssetReverb))
        {
            UE_LOG(LogFMOD, Error, TEXT("Failed to construct snapshot reverb asset: %s"), *ReverbPackagePath);
        }
    }

    return Asset;
}

void FFMODAssetBuilder::DeleteAsset(UObject *Asset)
{
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

        ObjectTools::ForceDeleteObjects(ObjectsToDelete, true);
    }
}
