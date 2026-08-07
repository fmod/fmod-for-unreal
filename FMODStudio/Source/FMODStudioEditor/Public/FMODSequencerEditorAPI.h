#pragma once

#include "CoreMinimal.h"

class ISequencer;
class UObject;

/** Public editor facade for creating FMOD Sequencer content without exposing FMOD track internals. */
class FMODSTUDIOEDITOR_API FFMODSequencerEditorAPI
{
public:
	static bool AddNewAmbientSound(ISequencer& Sequencer, UObject* EventAsset, FFrameNumber LocalFrame, FText& OutError);
	static bool AddKeysToExistingComponent(ISequencer& Sequencer, const FGuid& ComponentBindingGuid, UObject* EventAsset, FFrameNumber LocalFrame, FText& OutError);
	static bool AddStopKeyToExistingComponent(ISequencer& Sequencer, const FGuid& ComponentBindingGuid, FFrameNumber LocalFrame, FText& OutError);
};
