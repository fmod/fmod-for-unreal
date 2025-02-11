// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#include "FMODAsset.h"
#include "FMODStudioModule.h"

static const TCHAR *FMODAssetTypeStrings[] = { TEXT("Bank"), TEXT("Event"), TEXT("VCA"), TEXT("Bus"), TEXT("Invalid") };

UFMODAsset::UFMODAsset(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UFMODAsset::IsAsset() const
{
    return this != GetClass()->GetDefaultObject();
}

/** Get tags to show in content view */
void UFMODAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
    Super::GetAssetRegistryTags(Context);
    Context.AddTag(
        UObject::FAssetRegistryTag("Guid", AssetGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces), UObject::FAssetRegistryTag::TT_Alphabetical));
}
