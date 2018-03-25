// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "UnrealString.h"
#include "DateTime.h"
#include "Delegate.h"

class FFMODBankUpdateNotifier
{
public:
	FFMODBankUpdateNotifier();

	void SetFilePath(const FString& InPath);
	void Update();

	void EnableUpdate(bool bEnable);

	FSimpleMulticastDelegate BanksUpdatedEvent;

private:
	void Refresh();

	bool bUpdateEnabled;
	FString FilePath;
	FDateTime NextRefreshTime;
	FDateTime FileTime;
};
