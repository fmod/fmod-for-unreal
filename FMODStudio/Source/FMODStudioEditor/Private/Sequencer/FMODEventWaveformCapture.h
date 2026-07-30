// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#pragma once

#include "CoreMinimal.h"

class UFMODEvent;

struct FFMODEventWaveformData
{
    TArray<float> Peaks;
    int32 DurationMs = 0;
    int32 BucketDurationMs = 1;
};

class FFMODEventWaveformCapture
{
public:
    static void Request(const UFMODEvent* Event, int32 EventLengthMs, bool bIsOneShot);
    static const FFMODEventWaveformData* FindReady(const FGuid& EventGuid);
    static bool IsTerminal(const FGuid& EventGuid);
    static void Shutdown();
};
