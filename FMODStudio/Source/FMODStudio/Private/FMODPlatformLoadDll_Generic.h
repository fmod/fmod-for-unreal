// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.
#pragma once

void* FMODPlatformLoadDll(const TCHAR* LibToLoad)
{
	return FPlatformProcess::GetDllHandle(LibToLoad);
}

