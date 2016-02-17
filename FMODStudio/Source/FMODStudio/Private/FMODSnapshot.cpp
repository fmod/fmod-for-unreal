// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioPrivatePCH.h"
#include "FMODSnapshot.h"
#include "FMODStudioModule.h"

UFMODSnapshot::UFMODSnapshot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UFMODSnapshot::GetDesc()
{
	return FString::Printf( TEXT( "Snapshot %s" ), *AssetGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces) );
}

