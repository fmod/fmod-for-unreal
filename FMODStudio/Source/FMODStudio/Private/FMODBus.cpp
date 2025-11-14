// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#include "FMODBus.h"
#include "FMODStudioModule.h"

UFMODBus::UFMODBus(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
}

/** Get tags to show in content view */
void UFMODBus::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
    Super::GetAssetRegistryTags(Context);
}

FString UFMODBus::GetDesc()
{
    return FString::Printf(TEXT("Bus %s"), *AssetGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
}
