// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#pragma once

#include "FMODOculusRoomParameters.h"
#include "FMODOculusSettings.generated.h"


UCLASS(config = Engine, defaultconfig)
class FMODSTUDIOOCULUS_API UFMODOculusSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* Enable Oculus plugin
	*/
	UPROPERTY(config, EditAnywhere, Category = Oculus)
	bool bOculusEnabled;

	/**
	* Enable Early Reflections
	*/
	UPROPERTY(config, EditAnywhere, Category = Oculus)
	bool bEarlyReflectionsEnabled;

	/**
	* Enable Late Reverb
	*/
	UPROPERTY(config, EditAnywhere, Category = Oculus)
	bool bLateReverberationEnabled;

	/**
	* Default room parameters to apply at startup
	*/
	UPROPERTY(config, EditAnywhere, Category = Oculus)
	FFMODOculusRoomParameters RoomParameters;
};
