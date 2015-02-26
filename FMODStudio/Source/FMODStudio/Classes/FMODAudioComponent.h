// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#pragma once

#include "Map.h"
#include "FMODAudioComponent.generated.h"

/** called when an event stops, either because it played to completion or because a Stop() call turned it off early */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEventStopped);

namespace FMOD
{
	namespace Studio
	{
		class EventDescription;
		class EventInstance;
	}
}

/* Purely for doxygen generation */
#ifdef GENERATE_DOX
	#define UCLASS(...)
	#define UPROPERTY(...) public:
#endif

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

	/** Auto destroy this component on completion */
	UPROPERTY()
	uint32 bAutoDestroy:1;

	/** Stop sound when owner is destroyed */
	UPROPERTY()
	uint32 bStopWhenOwnerDestroyed:1;

	/** called when an event stops, either because it played to completion or because a Stop() call turned it off early */
	UPROPERTY(BlueprintAssignable)
	FOnEventStopped OnEventStopped;

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

	/** Called when the event has finished stopping */
	void OnPlaybackCompleted();

public:

	/** Whether or not the audio component should display an icon in the editor */
	uint32 bVisualizeComponent:1;

	/** Actual Studio instance handle */
	FMOD::Studio::EventInstance* StudioInstance;

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
	virtual void OnUpdateTransform(bool bSkipPhysicsMove) override;
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
	UPROPERTY(transient)
    class UBillboardComponent* SpriteComponent;
	void UpdateSpriteTexture();
#endif
};


