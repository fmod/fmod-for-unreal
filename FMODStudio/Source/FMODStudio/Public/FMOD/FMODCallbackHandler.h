// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2025.

#pragma once

#include "UObject/Interface.h"
#include "fmod_studio.hpp"
#include "FMODCallbackHandler.generated.h"

/**
 * This class is required for Unreal reflection, but does not need to be modified.
 */
UINTERFACE(MinimalAPI)
class UFMODCallbackHandler : public UInterface
{
	GENERATED_BODY()
};

class FMODSTUDIO_API IFMODCallbackHandler
{
    GENERATED_BODY()

public:
    /**
     * Method called just before the FMOD System initializes.
     */
    virtual void PreInitialize(FMOD::Studio::System* system) = 0;
};
