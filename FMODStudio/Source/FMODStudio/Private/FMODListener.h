// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2023.

#pragma once

#include "GenericPlatform/GenericPlatform.h"
#include "UObject/Object.h"
#include "Math/Vector.h"

struct FInteriorSettings;

/** Struct encapsulating settings for interior areas. */
struct FFMODInteriorSettings
{
    uint32 bIsWorldSettings : 1;
    float ExteriorVolume = 0;
    float ExteriorTime = 0;
    float ExteriorLPF = 0;
    float ExteriorLPFTime = 0;
    float InteriorVolume = 0;
    float InteriorTime = 0;
    float InteriorLPF = 0;
    float InteriorLPFTime = 0;

    FFMODInteriorSettings();
    bool operator==(const FInteriorSettings &Other) const;
    bool operator!=(const FInteriorSettings &Other) const;
    FFMODInteriorSettings &operator=(FInteriorSettings Other);
};

/** A direct copy of FListener (which doesn't have external linkage, unfortunately) **/
struct FFMODListener
{
    FTransform Transform = FTransform::Identity;
    FVector Velocity = FVector().ZeroVector;

    struct FFMODInteriorSettings InteriorSettings = FFMODInteriorSettings();
    /** The volume the listener resides in */
    class AAudioVolume *Volume = nullptr;

    /** The times of interior volumes fading in and out */
    double InteriorStartTime = 0.0f;
    double InteriorEndTime = 0.0f;
    double ExteriorEndTime = 0.0f;
    double InteriorLPFEndTime = 0.0f;
    double ExteriorLPFEndTime = 0.0f;
    float InteriorVolumeInterp = 0.0f;
    float InteriorLPFInterp = 0.0f;
    float ExteriorVolumeInterp = 0.0f;
    float ExteriorLPFInterp = 0.0f;

    FVector GetUp() const { return Transform.GetUnitAxis(EAxis::Z); }
    FVector GetFront() const { return Transform.GetUnitAxis(EAxis::Y); }
    FVector GetRight() const { return Transform.GetUnitAxis(EAxis::X); }

    /**
	 * Works out the interp value between source and end
	 */
    float Interpolate(const double EndTime);

    /**
	 * Gets the current state of the interior settings for the listener
	 */
    void UpdateCurrentInteriorSettings();

    /** 
	 * Apply the interior settings to ambient sounds
	 */
    void ApplyInteriorSettings(class AAudioVolume *Volume, const FInteriorSettings &Settings);
};
