// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
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

	/** Whether to show in the content window */
	UPROPERTY()
	bool bShowAsAsset;

	/** Force this to be an asset */
	virtual bool IsAsset() const override { return bShowAsAsset; }

	/** Get tags to show in content view */
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

};


