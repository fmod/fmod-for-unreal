#pragma once

#include "MVVM/Views/KeyDrawParams.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "Sequencer/FMODEventControlSection.h"
#include "ISequencerChannelInterface.h"

/** Key editor overrides */
bool CanCreateKeyEditor(const FFMODEventControlChannel* Channel);

TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FFMODEventControlChannel>& Channel, const UE::Sequencer::FCreateKeyEditorParams& Params);

/** Key drawing overrides */
void DrawKeys(FFMODEventControlChannel *Channel, TArrayView<const FKeyHandle> InKeyHandles, const UMovieSceneSection* InOwner, TArrayView<FKeyDrawParams> OutKeyDrawParams);
