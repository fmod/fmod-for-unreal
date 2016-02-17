// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioOculusPrivatePCH.h"
#include "FMODStudioOculusModule.h"
#include "FMODOculusBlueprintStatics.h"
#include "FMODUtils.h"

#if FMOD_OSP_SUPPORTED
	#include "OculusFMODSpatializerSettings.h"
#endif

/////////////////////////////////////////////////////
// UFMODOculusBlueprintStatics

UFMODOculusBlueprintStatics::UFMODOculusBlueprintStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFMODOculusBlueprintStatics::SetEarlyReflectionsEnabled(bool bEnable)
{
#if FMOD_OSP_SUPPORTED
	if (IFMODStudioOculusModule::Get().IsRunning())
	{
		OSP_FMOD_SetEarlyReflectionsEnabled(bEnable);
	}
#endif
}

void UFMODOculusBlueprintStatics::SetLateReverberationEnabled(bool bEnable)
{
#if FMOD_OSP_SUPPORTED
	if (IFMODStudioOculusModule::Get().IsRunning())
	{
		OSP_FMOD_SetLateReverberationEnabled(bEnable);
	}
#endif
}

void UFMODOculusBlueprintStatics::SetRoomParameters(const FFMODOculusRoomParameters& Params)
{
#if FMOD_OSP_SUPPORTED
	if (IFMODStudioOculusModule::Get().IsRunning())
	{
		// If we have a set of empty parameters, don't set it into oculus
		if (Params.RoomWidth != 0.0f && Params.RoomHeight != 0.0f && Params.RoomDepth != 0.0f)
		{
			// Convert from unreal units (1cm) to metres
			OSP_FMOD_SetSimpleBoxRoomParameters(
				Params.RoomWidth * FMOD_VECTOR_SCALE_DEFAULT,
				Params.RoomHeight * FMOD_VECTOR_SCALE_DEFAULT,
				Params.RoomDepth * FMOD_VECTOR_SCALE_DEFAULT,
				Params.RefLeft, Params.RefRight,
				Params.RefUp, Params.RefDown,
				Params.RefNear, Params.RefFar);
		}
	}
#endif
}
