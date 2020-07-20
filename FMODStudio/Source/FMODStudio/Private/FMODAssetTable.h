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
    void AddAsset(const FGuid &AssetGuid, const FString &AssetFullName);
    void GetAllBankPathsFromDisk(const FString &BankDir, TArray<FString> &Paths);
    void BuildBankPathLookup();
    FString GetBankPathByGuid(const FGuid& Guid) const;

private:
    FMOD::Studio::System *StudioSystem;
    TMap<FGuid, TWeakObjectPtr<UFMODAsset>> GuidMap;
    TMap<FName, TWeakObjectPtr<UFMODAsset>> NameMap;
    TMap<FString, TWeakObjectPtr<UFMODAsset>> FullNameLookup;
    FString MasterBankPath;
    FString MasterStringsBankPath;
    FString MasterAssetsBankPath;

    struct BankLocalization
    {
        FString Locale;
        FString Path;
    };

    typedef TArray<BankLocalization> BankLocalizations;

    TMap<FGuid, BankLocalizations> BankPathLookup;
    FString ActiveLocale;
};
