// Copyright (c), Firelight Technologies Pty, Ltd.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FMODBankLookup.generated.h"

USTRUCT()
struct FMODSTUDIO_API FFMODLocalizedBankRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Path;
};

USTRUCT()
struct FMODSTUDIO_API FFMODLocalizedBankTable : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UDataTable *Banks;
};

UCLASS()
class FMODSTUDIO_API UFMODBankLookup : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UDataTable *DataTable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString MasterBankPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString MasterAssetsBankPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString MasterStringsBankPath;
};
