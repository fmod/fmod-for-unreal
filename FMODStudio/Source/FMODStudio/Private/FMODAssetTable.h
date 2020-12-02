// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2020.

#pragma once

#include "FMODAsset.h"

namespace FMOD
{
namespace Studio
{
class System;
}
}

class UFMODBank;
struct FFMODBankDiskFileMap;

class FFMODAssetTable
{
public:
    FFMODAssetTable();
    ~FFMODAssetTable();

    void Create();
    void Destroy();

    void Refresh();

    UFMODAsset *FindByName(const FString &Name) const;
    FString GetBankPath(const UFMODBank &Bank) const;
    FString GetMasterBankPath() const;
    FString GetMasterStringsBankPath() const;
    FString GetMasterAssetsBankPath() const;
    void SetLocale(const FString &LocaleCode);
    void GetAllBankPaths(TArray<FString> &BankPaths, bool IncludeMasterBank) const;

private:
    struct AssetCreateInfo
    {
        UClass *Class;
        FGuid Guid;
        FString StudioPath;
        FString AssetName;
        FString PackagePath;
        FString Path;
    };

    struct BankLocalization
    {
        FString Locale;
        FString Path;
    };

    typedef TArray<BankLocalization> BankLocalizations;

    FString GetAssetClassName(UClass *AssetClass);
    bool MakeAssetCreateInfo(const FGuid &AssetGuid, const FString &StudioPath, AssetCreateInfo *CreateInfo);
    UFMODAsset *CreateAsset(const AssetCreateInfo& CreateInfo);
    void DeleteAsset(UObject *Asset);
    void GetAllBankPathsFromDisk(const FString &BankDir, TArray<FString> &Paths);
    void BuildBankPathLookup();
    FString GetBankPathByGuid(const FGuid& Guid) const;

    FMOD::Studio::System *StudioSystem;
    TMap<FString, TWeakObjectPtr<UFMODAsset>> NameLookup;
    FString MasterBankPath;
    FString MasterStringsBankPath;
    FString MasterAssetsBankPath;
    TMap<FGuid, BankLocalizations> BankPathLookup;
    FString ActiveLocale;
};
