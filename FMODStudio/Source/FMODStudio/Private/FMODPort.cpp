// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#include "FMODPort.h"
#include "FMODStudioModule.h"

UFMODPort::UFMODPort(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
}

/** Get tags to show in content view */
void UFMODPort::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
    Super::GetAssetRegistryTags(Context);
}

FString UFMODPort::GetDesc()
{
    return FString::Printf(TEXT("Port %s"), *AssetGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
}
