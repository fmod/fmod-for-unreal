// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#include "FMODStudioPrivatePCH.h"
#include "FMODSnapshotReverb.h"

UFMODSnapshotReverb::UFMODSnapshotReverb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if ENGINE_MINOR_VERSION > 14
#if WITH_EDITORONLY_DATA
void UFMODSnapshotReverb::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{

}
#endif // EDITORONLY_DATA
#endif //ENGINE_MINOR_VERSION