// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#pragma once

#include "FMODEvent.h"
#include "FMODSnapshot.generated.h"

/* Purely for doxygen generation */
#ifdef GENERATE_DOX
	#define UCLASS(...)
	#define UPROPERTY(...) public:
#endif

/**
 * FMOD Snapshot Asset.
 */
UCLASS()
class FMODSTUDIO_API UFMODSnapshot : public UFMODEvent
{
	GENERATED_UCLASS_BODY()

	/** Descriptive name */
	virtual FString GetDesc() override;

};


