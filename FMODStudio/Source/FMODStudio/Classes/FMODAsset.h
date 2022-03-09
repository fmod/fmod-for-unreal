// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2021.

#pragma once

#include "Misc/Guid.h"
#include "CoreMinimal.h"
#include "FMODAsset.generated.h"

/**
 * FMOD Asset.
 */
UCLASS(BlueprintType)
class FMODSTUDIO_API UFMODAsset : public UObject
{
    GENERATED_UCLASS_BODY()

    /** The unique Guid, which matches the one exported from FMOD Studio */
    UPROPERTY()
    FGuid AssetGuid;

    /** Non default instances of UFMODAsset are assets */
    virtual bool IsAsset() const override;

    /** Get tags to show in content view */
    virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag> &OutTags) const override;
};
