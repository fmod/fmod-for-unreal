#include "FMODSequencerEditorAPI.h"

#include "FMODAmbientSound.h"
#include "FMODAudioComponent.h"
#include "FMODEvent.h"
#include "ISequencer.h"
#include "LevelEditorViewport.h"
#include "MovieScene.h"
#include "MovieSceneBindingReferences.h"
#include "Sections/MovieSceneObjectPropertySection.h"
#include "Tracks/MovieSceneObjectPropertyTrack.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSpawnRegister.h"
#include "ScopedTransaction.h"
#include "SequencerUtilities.h"
#include "Sequencer/FMODEventControlSection.h"
#include "Sequencer/FMODEventControlTrack.h"

#define LOCTEXT_NAMESPACE "FMODSequencerEditorAPI"

namespace
{
bool ResolveExactlyOneFMODAudioComponentImpl(ISequencer& Sequencer, const FGuid& BindingGuid, UFMODAudioComponent*& OutComponent, FText& OutError)
{
	OutComponent = nullptr;
	const TArrayView<TWeakObjectPtr<>> Objects = Sequencer.FindObjectsInCurrentSequence(BindingGuid);
	if (Objects.Num() == 0)
	{
		OutError = LOCTEXT("SelectedBindingNotSingleComponent", "The selected FMOD binding does not resolve to one live FMOD Audio Component.");
		return false;
	}
	if (Objects.Num() != 1)
	{
		OutError = LOCTEXT("SelectedBindingMultipleObjects", "The selected FMOD binding resolves to multiple objects.");
		return false;
	}
	OutComponent = Cast<UFMODAudioComponent>(Objects[0].Get());
	if (!IsValid(OutComponent))
	{
		OutComponent = nullptr;
		OutError = LOCTEXT("SelectedBindingNotSingleComponent", "The selected FMOD binding does not resolve to one live FMOD Audio Component.");
		return false;
	}
	return true;
}

void RemoveBindingLikeSequencerUI(ISequencer& Sequencer, UMovieSceneSequence& Sequence, UMovieScene& MovieScene, const FGuid& BindingGuid)
{
	if (!BindingGuid.IsValid() || !MovieScene.FindPossessable(BindingGuid))
	{
		return;
	}

	bool bDestroysSpawnedObject = false;
	if (const FMovieSceneBindingReferences* BindingReferences = Sequence.GetBindingReferences())
	{
		for (const FMovieSceneBindingReference& BindingReference : BindingReferences->GetReferences(BindingGuid))
		{
			if (BindingReference.CustomBinding && BindingReference.CustomBinding->WillSpawnObject(Sequencer.GetSharedPlaybackState()))
			{
				bDestroysSpawnedObject = true;
				break;
			}
		}
	}

	if (MovieScene.RemovePossessable(BindingGuid))
	{
		if (bDestroysSpawnedObject)
		{
			Sequencer.GetSpawnRegister().DestroySpawnedObject(BindingGuid, Sequencer.GetFocusedTemplateID(), Sequencer.GetSharedPlaybackState(), 0);
		}
		Sequence.UnbindPossessableObjects(BindingGuid);
	}
}
}

bool FFMODSequencerEditorAPI::AddNewAmbientSound(ISequencer& Sequencer, UObject* EventAsset, FFrameNumber LocalFrame, FText& OutError)
{
	OutError = FText::GetEmpty();
	UFMODEvent* Event = Cast<UFMODEvent>(EventAsset);
	UMovieSceneSequence* FocusedSequence = Sequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;
	if (Event == nullptr || FocusedSequence == nullptr || MovieScene == nullptr)
	{
		OutError = LOCTEXT("FocusedSequenceUnavailable", "The focused sequence or FMOD Event is unavailable.");
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("AddNewAmbientSound", "Add FMOD Event To Sequencer"));
	FocusedSequence->Modify();
	MovieScene->Modify();

	FGuid SpawnableGuid;
	FGuid ComponentGuid;
	AFMODAmbientSound* SourceActor = nullptr;
	auto Rollback = [&]()
	{
		// This mirrors Sequencer's possessable deletion path for custom spawnable bindings.
		RemoveBindingLikeSequencerUI(Sequencer, *FocusedSequence, *MovieScene, ComponentGuid);
		RemoveBindingLikeSequencerUI(Sequencer, *FocusedSequence, *MovieScene, SpawnableGuid);
		if (SourceActor)
		{
			SourceActor->GetWorld()->EditorDestroyActor(SourceActor, true);
			SourceActor = nullptr;
		}
		Transaction.Cancel();
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	};

	UWorld* EditorWorld = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	SourceActor = EditorWorld ? EditorWorld->SpawnActor<AFMODAmbientSound>() : nullptr;
	if (SourceActor == nullptr || SourceActor->AudioComponent == nullptr)
	{
		OutError = LOCTEXT("CreateAmbientSourceFailed", "Could not create an FMOD Ambient Sound in the editor world.");
		Rollback();
		return false;
	}
	SourceActor->AudioComponent->Event = Event;

	const FString SpawnableName = FString::Printf(TEXT("FMOD_%s"), *Event->GetName());
	UE::Sequencer::FCreateBindingParams CreateBindingParams;
	CreateBindingParams.BindingNameOverride = SpawnableName;
	CreateBindingParams.bSpawnable = true;
	SpawnableGuid = FSequencerUtilities::CreateBinding(Sequencer.AsShared(), *SourceActor, CreateBindingParams);
	if (!SpawnableGuid.IsValid())
	{
		OutError = LOCTEXT("CreateSpawnableFailed", "Could not create the FMOD Ambient Sound spawnable.");
		Rollback();
		return false;
	}

	AFMODAmbientSound* SpawnedActor = Cast<AFMODAmbientSound>(Sequencer.FindSpawnedObjectOrTemplate(SpawnableGuid));
	if (SpawnedActor == nullptr || SpawnedActor->GetWorld() != EditorWorld || MovieScene->FindPossessable(SpawnableGuid) == nullptr)
	{
		OutError = LOCTEXT("SpawnedActorUnavailable", "The new FMOD Ambient Sound binding did not resolve in the focused Sequencer context.");
		Rollback();
		return false;
	}
	EditorWorld->EditorDestroyActor(SourceActor, true);
	SourceActor = nullptr;

	UFMODAudioComponent* AudioComponent = SpawnedActor->AudioComponent;
	ComponentGuid = AudioComponent ? Sequencer.GetHandleToObject(AudioComponent, true) : FGuid();
	FMovieScenePossessable* ComponentPossessable = ComponentGuid.IsValid() ? MovieScene->FindPossessable(ComponentGuid) : nullptr;
	bool bComponentResolved = false;
	for (const TWeakObjectPtr<>& BoundObject : Sequencer.FindObjectsInCurrentSequence(ComponentGuid))
	{
		bComponentResolved |= BoundObject.Get() == AudioComponent;
	}
	if (AudioComponent == nullptr || ComponentPossessable == nullptr || ComponentPossessable->GetParent() != SpawnableGuid || !bComponentResolved)
	{
		OutError = LOCTEXT("CreateComponentBindingFailed", "Could not create a resolved FMOD Audio Component binding for the new spawnable.");
		Rollback();
		return false;
	}

	const FName EventPropertyName = GET_MEMBER_NAME_CHECKED(UFMODAudioComponent, Event);
	const FString EventPropertyPath(TEXT("Event"));
	const FText SoundTrackDisplayName = LOCTEXT("SoundTrackName", "Sound Track");
	const FText PlaybackTrackDisplayName = LOCTEXT("PlaybackTrackName", "Playback Track");

	const FMovieSceneBinding* ComponentBinding = MovieScene->FindBinding(ComponentGuid);
	if (ComponentBinding == nullptr)
	{
		OutError = LOCTEXT("ComponentBindingUnavailable", "Could not find the FMOD Audio Component binding while creating tracks.");
		Rollback();
		return false;
	}

	UMovieSceneObjectPropertyTrack* SoundTrack = nullptr;
	UFMODEventControlTrack* PlaybackTrack = nullptr;
	int32 EventTrackCount = 0;
	int32 PlaybackTrackCount = 0;
	for (UMovieSceneTrack* Track : ComponentBinding->GetTracks())
	{
		if (UMovieSceneObjectPropertyTrack* PropertyTrack = Cast<UMovieSceneObjectPropertyTrack>(Track))
		{
			if (PropertyTrack->GetPropertyName() == EventPropertyName)
			{
				++EventTrackCount;
				if (PropertyTrack->GetPropertyPath().ToString() == EventPropertyPath)
				{
					SoundTrack = PropertyTrack;
				}
			}
		}
		else if (UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track))
		{
			++PlaybackTrackCount;
			PlaybackTrack = ControlTrack;
		}
	}

	if (EventTrackCount > 1 || PlaybackTrackCount > 1)
	{
		OutError = LOCTEXT("AmbiguousFMODTracks", "The FMOD Audio Component already has ambiguous Event or Playback tracks. Remove the duplicate tracks before using Add New.");
		Rollback();
		return false;
	}
	if (EventTrackCount == 1 && SoundTrack == nullptr)
	{
		OutError = LOCTEXT("InvalidEventTrack", "The FMOD Audio Component already has an Event track with an unexpected property path. Remove or repair it before using Add New.");
		Rollback();
		return false;
	}

	if (SoundTrack == nullptr)
	{
		SoundTrack = MovieScene->AddTrack<UMovieSceneObjectPropertyTrack>(ComponentGuid);
		if (SoundTrack == nullptr)
		{
			OutError = LOCTEXT("CreateSoundTrackFailed", "Could not create the FMOD Sound track.");
			Rollback();
			return false;
		}
		SoundTrack->Modify();
		SoundTrack->SetPropertyNameAndPath(EventPropertyName, EventPropertyPath);
		SoundTrack->PropertyClass = UFMODEvent::StaticClass();
	}
	SoundTrack->Modify();
	SoundTrack->PropertyClass = UFMODEvent::StaticClass();
	SoundTrack->SetDisplayName(SoundTrackDisplayName);

	bool bSoundSectionAdded = false;
	UMovieSceneObjectPropertySection* SoundSection = Cast<UMovieSceneObjectPropertySection>(SoundTrack->FindOrAddSection(LocalFrame, bSoundSectionAdded));
	if (SoundSection == nullptr)
	{
		OutError = LOCTEXT("CreateSoundSectionFailed", "Could not create a valid FMOD Sound track section.");
		Rollback();
		return false;
	}
	if (bSoundSectionAdded && Sequencer.GetInfiniteKeyAreas())
	{
		SoundSection->SetRange(TRange<FFrameNumber>::All());
	}
	SoundTrack->Modify();
	SoundSection->Modify();
	SoundSection->ObjectChannel.SetPropertyClass(UFMODEvent::StaticClass());
	if (SoundSection->ObjectChannel.GetPropertyClass() != UFMODEvent::StaticClass())
	{
		OutError = LOCTEXT("ConfigureSoundSectionFailed", "Could not configure the FMOD Sound track section for UFMODEvent assets.");
		Rollback();
		return false;
	}
	SoundSection->ObjectChannel.GetData().UpdateOrAddKey(LocalFrame, FMovieSceneObjectPathChannelKeyValue(Event));

	if (PlaybackTrack == nullptr)
	{
		PlaybackTrack = MovieScene->AddTrack<UFMODEventControlTrack>(ComponentGuid);
		if (PlaybackTrack == nullptr)
		{
			OutError = LOCTEXT("CreatePlaybackTrackFailed", "Could not create the FMOD Playback track.");
			Rollback();
			return false;
		}
	}
	PlaybackTrack->Modify();
	PlaybackTrack->SetDisplayName(PlaybackTrackDisplayName);
	PlaybackTrack->AddNewSection(LocalFrame);

	UFMODEventControlSection* ControlSection = nullptr;
	int32 PlaybackSectionCount = 0;
	for (UMovieSceneSection* Section : PlaybackTrack->GetAllSections())
	{
		if (Section != nullptr && Section->GetRange().Contains(LocalFrame))
		{
			ControlSection = Cast<UFMODEventControlSection>(Section);
			++PlaybackSectionCount;
		}
	}
	if (PlaybackSectionCount != 1 || ControlSection == nullptr)
	{
		OutError = LOCTEXT("CreatePlaybackSectionFailed", "Could not find one valid FMOD Playback track section at the current frame.");
		Rollback();
		return false;
	}
	PlaybackTrack->Modify();
	ControlSection->Modify();
	ControlSection->ControlKeys.GetData().UpdateOrAddKey(LocalFrame, static_cast<uint8>(EFMODEventControlKey::Play));

	bool bHasEventKey = false;
	const TMovieSceneChannelData<const FMovieSceneObjectPathChannelKeyValue> SoundChannelData = SoundSection->ObjectChannel.GetData();
	const TArrayView<const FFrameNumber> SoundKeyTimes = SoundChannelData.GetTimes();
	const TArrayView<const FMovieSceneObjectPathChannelKeyValue> SoundKeyValues = SoundChannelData.GetValues();
	for (int32 KeyIndex = 0; KeyIndex < SoundKeyTimes.Num(); ++KeyIndex)
	{
		bHasEventKey |= SoundKeyTimes[KeyIndex] == LocalFrame && SoundKeyValues[KeyIndex].Get() == Event;
	}

	bool bHasPlayKey = false;
	const TMovieSceneChannelData<const uint8> ControlChannelData = ControlSection->ControlKeys.GetData();
	const TArrayView<const FFrameNumber> ControlKeyTimes = ControlChannelData.GetTimes();
	const TArrayView<const uint8> ControlKeyValues = ControlChannelData.GetValues();
	for (int32 KeyIndex = 0; KeyIndex < ControlKeyTimes.Num(); ++KeyIndex)
	{
		bHasPlayKey |= ControlKeyTimes[KeyIndex] == LocalFrame && ControlKeyValues[KeyIndex] == static_cast<uint8>(EFMODEventControlKey::Play);
	}

	if (SoundTrack->GetDisplayName().ToString() != SoundTrackDisplayName.ToString() ||
		PlaybackTrack->GetDisplayName().ToString() != PlaybackTrackDisplayName.ToString() ||
		!SoundSection->GetRange().Contains(LocalFrame) || !bHasEventKey || !bHasPlayKey)
	{
		OutError = LOCTEXT("ValidateFMODTracksFailed", "FMOD tracks were created but failed validation.");
		Rollback();
		return false;
	}

	Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	return true;
}

bool FFMODSequencerEditorAPI::AddKeysToExistingComponent(ISequencer& Sequencer, const FGuid& ComponentBindingGuid, UObject* EventAsset, FFrameNumber LocalFrame, FText& OutError)
{
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade entry: binding=%s frame=%d"), *ComponentBindingGuid.ToString(), LocalFrame.Value);
	OutError = FText::GetEmpty();
	UFMODEvent* Event = Cast<UFMODEvent>(EventAsset);
	UMovieSceneSequence* FocusedSequence = Sequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;
	const FMovieSceneBinding* ComponentBinding = MovieScene ? MovieScene->FindBinding(ComponentBindingGuid) : nullptr;
	if (Event == nullptr || MovieScene == nullptr || ComponentBinding == nullptr)
	{
		OutError = LOCTEXT("ExistingComponentUnavailable", "The selected FMOD component or focused sequence is unavailable.");
		return false;
	}

	UFMODAudioComponent* ResolvedComponent = nullptr;
	if (!ResolveExactlyOneFMODAudioComponentImpl(Sequencer, ComponentBindingGuid, ResolvedComponent, OutError))
	{
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade component resolution: resolved=%d"), ResolvedComponent != nullptr);

	const FName EventPropertyName = GET_MEMBER_NAME_CHECKED(UFMODAudioComponent, Event);
	UMovieSceneObjectPropertyTrack* SoundTrack = nullptr;
	UFMODEventControlTrack* PlaybackTrack = nullptr;
	int32 SoundTrackCount = 0;
	int32 PlaybackTrackCount = 0;
	for (UMovieSceneTrack* Track : ComponentBinding->GetTracks())
	{
		if (UMovieSceneObjectPropertyTrack* PropertyTrack = Cast<UMovieSceneObjectPropertyTrack>(Track))
		{
			if (PropertyTrack->GetPropertyName() == EventPropertyName &&
				PropertyTrack->GetPropertyPath().ToString() == TEXT("Event") &&
				PropertyTrack->PropertyClass == UFMODEvent::StaticClass())
			{
				SoundTrack = PropertyTrack;
				++SoundTrackCount;
			}
		}
		else if (UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track))
		{
			PlaybackTrack = ControlTrack;
			++PlaybackTrackCount;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade tracks: sound=%d playback=%d"), SoundTrackCount, PlaybackTrackCount);

	UMovieSceneObjectPropertySection* SoundSection = nullptr;
	int32 SoundSectionCount = 0;
	if (SoundTrackCount == 1)
	{
		for (UMovieSceneSection* Section : SoundTrack->GetAllSections())
		{
			if (Section && Section->GetRange().Contains(LocalFrame))
			{
				if (UMovieSceneObjectPropertySection* PropertySection = Cast<UMovieSceneObjectPropertySection>(Section))
				{
					SoundSection = PropertySection;
					++SoundSectionCount;
				}
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade sound sections=%d"), SoundSectionCount);
	if (SoundTrackCount != 1 || SoundSectionCount != 1)
	{
		OutError = LOCTEXT("MissingSoundTrack", "Selected FMOD component has no valid Sound Track. Create it with Add New first.");
		return false;
	}

	UFMODEventControlSection* PlaybackSection = nullptr;
	int32 PlaybackSectionCount = 0;
	if (PlaybackTrackCount == 1)
	{
		for (UMovieSceneSection* Section : PlaybackTrack->GetAllSections())
		{
			if (Section && Section->GetRange().Contains(LocalFrame))
			{
				if (UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(Section))
				{
					PlaybackSection = ControlSection;
					++PlaybackSectionCount;
				}
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade playback sections=%d"), PlaybackSectionCount);
	if (PlaybackTrackCount != 1 || PlaybackSectionCount != 1)
	{
		OutError = LOCTEXT("MissingPlaybackTrack", "Selected FMOD component has no valid Playback Track at the target frame. Create it with Add New first.");
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade transaction begin"));
	FScopedTransaction Transaction(LOCTEXT("AddKeysToSelected", "Add FMOD Keys To Selected"));
	FocusedSequence->Modify();
	MovieScene->Modify();
	SoundTrack->Modify();
	SoundSection->Modify();
	PlaybackTrack->Modify();
	PlaybackSection->Modify();
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade Event key begin"));
	SoundSection->ObjectChannel.GetData().UpdateOrAddKey(LocalFrame, FMovieSceneObjectPathChannelKeyValue(Event));
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade Event key end"));
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade Play key begin"));
	PlaybackSection->ControlKeys.GetData().UpdateOrAddKey(LocalFrame, static_cast<uint8>(EFMODEventControlKey::Play));
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade Play key end"));
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade Notify begin"));
	Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade Notify end"));
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade success exit"));
	return true;
}

bool FFMODSequencerEditorAPI::AddStopKeyToExistingComponent(ISequencer& Sequencer, const FGuid& ComponentBindingGuid, FFrameNumber LocalFrame, FText& OutError)
{
	OutError = FText::GetEmpty();
	UMovieSceneSequence* FocusedSequence = Sequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;
	if (FocusedSequence == nullptr || MovieScene == nullptr)
	{
		OutError = LOCTEXT("StopKeyFocusedSequenceUnavailable", "There is no focused Level Sequence or its Movie Scene is unavailable.");
		return false;
	}

	const FMovieSceneBinding* ComponentBinding = MovieScene->FindBinding(ComponentBindingGuid);
	if (ComponentBinding == nullptr)
	{
		OutError = LOCTEXT("StopKeyComponentBindingUnavailable", "The selected FMOD component binding is unavailable in the focused Level Sequence.");
		return false;
	}

	UFMODAudioComponent* ResolvedComponent = nullptr;
	if (!ResolveExactlyOneFMODAudioComponentImpl(Sequencer, ComponentBindingGuid, ResolvedComponent, OutError))
	{
		return false;
	}

	UFMODEventControlTrack* PlaybackTrack = nullptr;
	int32 PlaybackTrackCount = 0;
	for (UMovieSceneTrack* Track : ComponentBinding->GetTracks())
	{
		if (UFMODEventControlTrack* ControlTrack = Cast<UFMODEventControlTrack>(Track))
		{
			PlaybackTrack = ControlTrack;
			++PlaybackTrackCount;
		}
	}
	if (PlaybackTrackCount == 0)
	{
		OutError = LOCTEXT("StopKeyPlaybackTrackMissing", "The selected FMOD component has no Playback Track.");
		return false;
	}
	if (PlaybackTrackCount != 1)
	{
		OutError = LOCTEXT("StopKeyPlaybackTrackMultiple", "The selected FMOD component has multiple Playback Tracks.");
		return false;
	}

	UFMODEventControlSection* PlaybackSection = nullptr;
	int32 PlaybackSectionCount = 0;
	for (UMovieSceneSection* Section : PlaybackTrack->GetAllSections())
	{
		if (Section != nullptr && Section->GetRange().Contains(LocalFrame))
		{
			if (UFMODEventControlSection* ControlSection = Cast<UFMODEventControlSection>(Section))
			{
				PlaybackSection = ControlSection;
				++PlaybackSectionCount;
			}
		}
	}
	if (PlaybackSectionCount == 0)
	{
		OutError = LOCTEXT("StopKeyPlaybackSectionMissing", "The Playback Track has no section at the target frame.");
		return false;
	}
	if (PlaybackSectionCount != 1)
	{
		OutError = LOCTEXT("StopKeyPlaybackSectionMultiple", "The Playback Track has multiple sections at the target frame.");
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("AddStopKeyToSelected", "Add FMOD Stop Key To Selected"));
	FocusedSequence->Modify();
	MovieScene->Modify();
	PlaybackTrack->Modify();
	PlaybackSection->Modify();
	PlaybackSection->ControlKeys.GetData().UpdateOrAddKey(LocalFrame, static_cast<uint8>(EFMODEventControlKey::Stop));
	Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
	return true;
}

#undef LOCTEXT_NAMESPACE
