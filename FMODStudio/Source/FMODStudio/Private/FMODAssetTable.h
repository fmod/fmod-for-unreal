// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2019.

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

private:
    void AddAsset(const FGuid &AssetGuid, const FString &AssetFullName);
    void BuildBankPathLookup();

private:
    FMOD::Studio::System *StudioSystem;
    TMap<FGuid, TWeakObjectPtr<UFMODAsset>> GuidMap;
    TMap<FName, TWeakObjectPtr<UFMODAsset>> NameMap;
    TMap<FString, TWeakObjectPtr<UFMODAsset>> FullNameLookup;
    FString MasterBankPath;
    FString MasterStringsBankPath;
    FString MasterAssetsBankPath;
    TMap<FGuid, FString> BankPathLookup;
};
