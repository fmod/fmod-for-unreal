// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioOculusPrivatePCH.h"
#include "FMODOculusSettings.h"
#include "FMODUtils.h"

//////////////////////////////////////////////////////////////////////////
// UFMODOculusSettings

UFMODOculusSettings::UFMODOculusSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bOculusEnabled = false;
	bEarlyReflectionsEnabled = false;
	bLateReverberationEnabled = false;

}
