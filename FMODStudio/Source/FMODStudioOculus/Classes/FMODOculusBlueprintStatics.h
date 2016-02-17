// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#pragma once

#include "FMODOculusRoomParameters.h"
#include "FMODOculusBlueprintStatics.generated.h"

UCLASS()
class FMODSTUDIOOCULUS_API UFMODOculusBlueprintStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Enables or disables early reflections
	 */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Oculus")
	static void SetEarlyReflectionsEnabled(bool bEnable);

	/** Enables or disables late reverberation
	 */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Oculus")
	static void SetLateReverberationEnabled(bool bEnable);

	/** Default room parameters
	 */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Oculus")
	static void SetRoomParameters(const FFMODOculusRoomParameters& Params);
};
