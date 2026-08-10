#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "FMODSequencerToolsEditorSubsystem.generated.h"

class UFMODEvent;

/** Editor Utility Widget entry point for adding a selected FMOD event to Sequencer. */
UCLASS()
class FMODSEQUENCERTOOLSEDITOR_API UFMODSequencerToolsEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FMOD|Sequencer|Browser")
	FString GetLastFMODFolderPath() const;

	UFUNCTION(BlueprintPure, Category = "FMOD|Sequencer|Browser", meta = (ClampMin = "1", UIMin = "1"))
	FString GetCompactFMODFolderDisplayPath(const FString& FullFolderPath, int32 MaxSegments = 4) const;

	UFUNCTION(BlueprintPure, Category = "FMOD|Sequencer|Browser")
	void FilterFMODEvents(const TArray<UFMODEvent*>& SourceEvents, const FString& SearchText, TArray<UFMODEvent*>& OutFilteredEvents) const;

	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer|Browser", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool LoadFMODEventsFromFolder(const FString& FolderPath, TArray<UFMODEvent*>& OutEvents, FString& OutNormalizedFolderPath, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer|Browser", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool LoadFMODEventsFromSelectedContentBrowserFolder(TArray<UFMODEvent*>& OutEvents, FString& OutFolderPath, FText& OutError);

	/** Adds the single Content Browser FMOD Event to the single open Level Sequence at the local Sequencer cursor. */
	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer", meta = (ExpandBoolAsExecs = "ReturnValue", ToolTip = "Adds the selected FMOD Event as a spawnable ambient sound at the focused Sequencer cursor."))
	bool AddNewToFocusedSequencer(FText& OutError);

	UFUNCTION(
		BlueprintCallable,
		Category = "FMOD|Sequencer",
		meta = (
			ExpandBoolAsExecs = "ReturnValue",
			ToolTip = "Adds the selected FMOD Event at the current cursor or at a custom value interpreted using the focused Sequencer display format."
		)
	)
	bool AddNewToFocusedSequencerAtDisplayedTime(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool AddNewToFocusedSequencerAtDisplayedTimeWithEvent(UFMODEvent* Event, bool bUseCustomTime, double CustomDisplayedTime, FText& OutError);

	/** Adds the selected FMOD Event and a Play key to the selected existing FMOD component. */
	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer", meta = (ExpandBoolAsExecs = "ReturnValue", ToolTip = "Adds the selected FMOD Event and Play key to the single FMOD component selected in the focused Sequencer."))
	bool AddToSelectedAtDisplayedTime(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool AddToSelectedAtDisplayedTimeWithEvent(UFMODEvent* Event, bool bUseCustomTime, double CustomDisplayedTime, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer|Keys", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool AddStopKeyToSelectedAtDisplayedTime(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "FMOD|Sequencer|Attach", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool AttachZeroSelectedFMODToSelectedBinding(FText& OutError);
};
