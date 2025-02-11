// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#pragma once

#include "FMODAsset.h"
#include "FMODVCA.generated.h"

/**
 * FMOD VCA Asset.
 */
UCLASS()
class FMODSTUDIO_API UFMODVCA : public UFMODAsset
{
    GENERATED_UCLASS_BODY()

private:
    /** Get tags to show in content view */
    virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

    /** Descriptive name */
    virtual FString GetDesc() override;
};
