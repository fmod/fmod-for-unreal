// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2022.
#pragma once

#include "Containers/UnrealString.h"
#include "fmod_common.h"
#include "FMODSettings.h"

FString FMODPlatform_GetDllPath(const TCHAR *ShortName, bool bExplicitPath, bool bUseLibPrefix);

FString FMODPlatform_PlatformName();

void FMODPlatform_SetRealChannelCount(FMOD_ADVANCEDSETTINGS* advSettings);

int FMODPlatform_MemoryPoolSize();

#ifdef FMOD_PLATFORM_LOAD_DLL
void* FMODPlatformLoadDll(const TCHAR* LibToLoad);
#endif

FMOD_RESULT FMODPlatformSystemSetup();