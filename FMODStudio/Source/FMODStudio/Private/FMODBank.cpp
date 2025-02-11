// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#include "FMODBank.h"
#include "FMODStudioModule.h"

UFMODBank::UFMODBank(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
}

/** Get tags to show in content view */
void UFMODBank::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
    Super::GetAssetRegistryTags(Context);
}

FString UFMODBank::GetDesc()
{
    return FString::Printf(TEXT("Bank %s"), *AssetGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
}
