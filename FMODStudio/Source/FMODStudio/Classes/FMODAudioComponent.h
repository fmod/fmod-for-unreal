// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#pragma once

#include "Map.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Sound/SoundAttenuation.h"

// UE Includes
#include "Audio.h"
#include "Components/SceneComponent.h"

#include "FMODAudioComponent.generated.h"

// Event property
UENUM()
namespace EFMODEventProperty
{
	enum Type
	{
		/* Priority to set on low-level channels created by this event instance (-1 to 256). */
		ChannelPriority,
		/** Schedule delay to synchronized playback for multiple tracks in DSP clocks, or -1 for default. */
		ScheduleDelay,
		/** Schedule look-ahead on the timeline in DSP clocks, or -1 for default. */
		ScheduleLookahead,
		/** Override the event's 3D minimum distance, or -1 for default. */
		MinimumDistance,
		/** Override the event's 3D maximum distance, or -1 for default. */
		MaximumDistance,
		/** Number of options */
		Count
	};
}

class FFMODDynamicParameter : public FDynamicParameter
{
	public:
		FFMODDynamicParameter(float Initial=0.0f) : FDynamicParameter(Initial) { }
};

/** Used to store callback info from FMOD thread to our event */
struct FTimelineMarkerProperties
{
	FString Name;
	int32 Position;
};

/** Used to store callback info from FMOD thread to our event */
struct FTimelineBeatProperties
{
	int32 Bar;
	int32 Beat;
	int32 Position;
	float Tempo;
	int32 TimeSignatureUpper;
	int32 TimeSignatureLower;
};

/** called when an event stops, either because it played to completion or because a Stop() call turned it off early */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEventStopped);
/** called when we reach a named marker on the timeline */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimelineMarker, FString, Name, int32, Position);
/** called when we reach a beat on the timeline */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnTimelineBeat, int32, Bar, int32, Beat, int32, Position, float, Tempo, int32, TimeSignatureUpper, int32, TimeSignatureLower);

namespace FMOD
{
	class Sound;

	namespace Studio
	{
		class EventDescription;
		class EventInstance;
	}
}

struct FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES;
struct FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES;

/**
 * Plays FMOD Studio events.
 */
UCLASS(ClassGroup = (Audio, Common), hidecategories = (Object, ActorComponent, Physics, Rendering, Mobility, LOD), ShowCategories = Trigger, meta = (BlueprintSpawnableComponent))
class FMODSTUDIO_API UFMODAudioComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** The event asset to use for this sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	TAssetPtr<class UFMODEvent> Event;

	/** Stored parameters to apply next time we create an instance */
	TMap<FName, float> StoredParameters;

	/** Stored properties to apply next time we create an instance */
	float StoredProperties[EFMODEventProperty::Count];

	/** Enable timeline callbacks for this sound, so that OnTimelineMarker and OnTimelineBeat can be used */
	UPROPERTY(EditAnywhere, Category=Callbacks)
	uint32 bEnableTimelineCallbacks:1;

	/** Auto destroy this component on completion */
	UPROPERTY()
	uint32 bAutoDestroy:1;

	/** Stop sound when owner is destroyed */
	UPROPERTY()
	uint32 bStopWhenOwnerDestroyed:1;

	/** Whether we apply gain and low-pass based on audio zones. */
	uint32 bApplyAmbientVolumes:1;

	/** Whether we apply gain and low-pass based on occlusion directly into the sound. */
	uint32 bApplyOcclusionDirect:1;

	/** Whether we apply gain and low-pass based on occlusion onto a parameter. */
	uint32 bApplyOcclusionParameter:1;

	/** Should the Attenuation Settings asset be used or should the properties set directly on the component be used for attenuation properties (FMOD only uses the Occlusion part)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation)
	uint32 bOverrideAttenuation:1;

	/** Whether we have applied the occlusion at least once. */
	uint32 bHasCheckedOcclusion:1;

	/** called when an event stops, either because it played to completion or because a Stop() call turned it off early */
	UPROPERTY(BlueprintAssignable)
	FOnEventStopped OnEventStopped;

	/** called when we reach a named marker (if bEnableTimelineCallbacks is true) */
	UPROPERTY(BlueprintAssignable)
	FOnTimelineMarker OnTimelineMarker;

	/** called when we reach a beat of a tempo (if bEnableTimelineCallbacks is true) */
	UPROPERTY(BlueprintAssignable)
	FOnTimelineBeat OnTimelineBeat;

	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetEvent(UFMODEvent* NewEvent);

	/** Start a sound playing on an audio component */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void Play();

	/** Stop an audio component playing its sound cue, issue any delegates if needed */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void Stop();

	/** Trigger a cue in an event */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void TriggerCue();

	/** @return true if this component is currently playing an event */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	bool IsPlaying();

	/** Set volume on an audio component */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetVolume(float volume);

	/** Set pitch on an audio component */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetPitch(float pitch);

	/** Pause/Unpause an audio component */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetPaused(bool paused);

	/** Set a parameter into the event */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetParameter(FName Name, float Value);

	/** Set a parameter into the event */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	float GetParameter(FName Name);

	/** Set a parameter into the event */
	UFUNCTION(BlueprintCallable, Category = "Audio|FMOD|Components")
	void SetProperty(EFMODEventProperty::Type Property, float Value);

	/** Get the event length in milliseconds */
	UFUNCTION(BlueprintCallable, Category = "Audio|FMOD|Components")
	int32 GetLength() const;

	/** Set the timeline position in milliseconds  */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetTimelinePosition(int32 Time);

	/** Get the timeline position in milliseconds */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	int32 GetTimelinePosition();

	/** Called when the event has finished stopping */
	void OnPlaybackCompleted();

	/** Update gain and low-pass based on interior volumes */
	void UpdateInteriorVolumes();


	/** Sound name used for programmer sound.  Will look up the name in any loaded audio table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	FString ProgrammerSoundName;

	/** Set the sound name to use for programmer sound.  Will look up the name in any loaded audio table. */
	UFUNCTION(BlueprintCallable, Category="Audio|FMOD|Components")
	void SetProgrammerSoundName(FString Value);

	/** Set a programmer sound to use for this audio component.  Lifetime of sound must exceed that of the audio component. */
	void SetProgrammerSound(FMOD::Sound* Sound);

	/** If bOverrideSettings is false, the asset to use to determine attenuation properties for sounds generated by this component (FMOD only uses the Occlusion part) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation, meta=(EditCondition="!bOverrideAttenuation"))
	class USoundAttenuation* AttenuationSettings;

	/** If bOverrideSettings is true, the attenuation properties to use for sounds generated by this component (FMOD only uses the Occlusion part) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation, meta=(EditCondition="bOverrideAttenuation"))
	struct FSoundAttenuationSettings AttenuationOverrides;

	/** Update attenuation if we have it set */
	void UpdateAttenuation();

	/** Apply Volume and LPF into event */
	void ApplyVolumeLPF();

public:

	/** Actual Studio instance handle */
	FMOD::Studio::EventInstance* StudioInstance;

	void EventCallbackAddMarker(struct FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES* props);
	void EventCallbackAddBeat(struct FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES* props);
	void EventCallbackCreateProgrammerSound(struct FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props);
	void EventCallbackDestroyProgrammerSound(struct FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props);

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual FString GetDetailedInfoInternal() const override;
	// End UObject interface.
	// Begin USceneComponent Interface
	virtual void Activate(bool bReset=false) override;
	virtual void Deactivate() override;
#if ENGINE_MINOR_VERSION >= 12
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
#elif ENGINE_MINOR_VERSION >= 9
	virtual void OnUpdateTransform(bool bSkipPhysicsMove, ETeleportType Teleport = ETeleportType::None) override;
#else
	virtual void OnUpdateTransform(bool bSkipPhysicsMove) override;
#endif
	// End USceneComponent Interface

private:

	// Begin ActorComponent interface.
#if WITH_EDITORONLY_DATA
	virtual void OnRegister() override;
#endif
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	// End ActorComponent interface.

#if WITH_EDITORONLY_DATA
	void UpdateSpriteTexture();
#endif
	
	// Settings for ambient volume effects
	double InteriorLastUpdateTime; 
	float SourceInteriorVolume;
	float SourceInteriorLPF;
	float CurrentInteriorVolume;
	float CurrentInteriorLPF;
	float AmbientVolumeMultiplier;
	float AmbientLPF;
	float LastLPF;
	float LastVolume;
	FFMODDynamicParameter CurrentOcclusionFilterFrequency;
	FFMODDynamicParameter CurrentOcclusionVolumeAttenuation;

	// Tempo and marker callbacks
	FCriticalSection CallbackLock;
	TArray<FTimelineMarkerProperties> CallbackMarkerQueue;
	TArray<FTimelineBeatProperties> CallbackBeatQueue;

	// Direct assignment of programmer sound from other C++ code
	FMOD::Sound* ProgrammerSound;
	
	int32 EventLength;
};


