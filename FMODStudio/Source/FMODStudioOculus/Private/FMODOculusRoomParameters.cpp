// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioOculusPrivatePCH.h"
#include "FMODOculusRoomParameters.h"
#include "FMODUtils.h"

FFMODOculusRoomParameters::FFMODOculusRoomParameters()
{
	// Defaults as specified in OSP documentation
	RoomWidth = 10.0f / FMOD_VECTOR_SCALE_DEFAULT;
	RoomHeight = 11.0f / FMOD_VECTOR_SCALE_DEFAULT;
	RoomDepth = 12.0f / FMOD_VECTOR_SCALE_DEFAULT;
	RefLeft = 0.25f;
	RefRight = 0.25f;
	RefUp = 0.25f;
	RefDown = 0.25f;
	RefNear = 0.25f;
	RefFar = 0.25f;
}