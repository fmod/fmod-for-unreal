#include "FMODSequencerToolsEditorSubsystem.h"
#include "FMODSequencerToolsEditorUserSettings.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "FMODAmbientSound.h"
#include "FMODAudioComponent.h"
#include "FMODEvent.h"
#include "FMODSequencerEditorAPI.h"
#include "IContentBrowserSingleton.h"
#include "IContentBrowserDataModule.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "MovieSceneBinding.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "SequencerSettings.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "FMODSequencerToolsEditorSubsystem"

namespace FMODSequencerToolsEditorPrivate
{
const FString DefaultFMODFolderPath = TEXT("/Game/FMOD/Events/SFX/Environment/Cutscenes");

enum class EAttachZeroPickerResultImpl
{
	Confirmed,
	Cancelled,
	Error
};

bool ResolveExactlyOneLiveObjectImpl(ISequencer& Sequencer, const FGuid& BindingGuid, UObject*& OutObject, FText& OutError, const FText& NotSingleError, const FText& MultipleObjectsError)
{
	OutObject = nullptr;
	const TArrayView<TWeakObjectPtr<>> Objects = Sequencer.FindObjectsInCurrentSequence(BindingGuid);
	if (Objects.Num() == 0)
	{
		OutError = NotSingleError;
		return false;
	}
	if (Objects.Num() != 1)
	{
		OutError = MultipleObjectsError;
		return false;
	}
	OutObject = Objects[0].Get();
	if (!IsValid(OutObject))
	{
		OutObject = nullptr;
		OutError = NotSingleError;
		return false;
	}
	return true;
}

bool ResolveExactlyOneFMODAudioComponentImpl(ISequencer& Sequencer, const FGuid& BindingGuid, UFMODAudioComponent*& OutComponent, FText& OutError, const FText& NotComponentError, const FText& MultipleObjectsError)
{
	UObject* Object = nullptr;
	OutComponent = nullptr;
	if (!ResolveExactlyOneLiveObjectImpl(Sequencer, BindingGuid, Object, OutError, NotComponentError, MultipleObjectsError))
	{
		return false;
	}
	OutComponent = Cast<UFMODAudioComponent>(Object);
	if (OutComponent == nullptr)
	{
		OutError = NotComponentError;
		return false;
	}
	return true;
}

bool ResolveExactlyOneActorImpl(ISequencer& Sequencer, const FGuid& BindingGuid, AActor*& OutActor, FText& OutError, const FText& NotActorError, const FText& MultipleObjectsError)
{
	UObject* Object = nullptr;
	OutActor = nullptr;
	if (!ResolveExactlyOneLiveObjectImpl(Sequencer, BindingGuid, Object, OutError, NotActorError, MultipleObjectsError))
	{
		return false;
	}
	OutActor = Cast<AActor>(Object);
	if (OutActor == nullptr)
	{
		OutError = NotActorError;
		return false;
	}
	return true;
}

bool ResolveExactlyOneSceneComponentImpl(ISequencer& Sequencer, const FGuid& BindingGuid, USceneComponent*& OutComponent, FText& OutError, const FText& NotComponentError, const FText& MultipleObjectsError)
{
	UObject* Object = nullptr;
	OutComponent = nullptr;
	if (!ResolveExactlyOneLiveObjectImpl(Sequencer, BindingGuid, Object, OutError, NotComponentError, MultipleObjectsError))
	{
		return false;
	}
	OutComponent = Cast<USceneComponent>(Object);
	if (OutComponent == nullptr)
	{
		OutError = NotComponentError;
		return false;
	}
	return true;
}

struct FAttachZeroPickerItemImpl
{
	FName Value;
	FText Label;
};

class SAttachZeroNamePickerImpl : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAttachZeroNamePickerImpl)
		: _UseComponentButtonPresentation(false)
	{}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(TArray<FName>, ItemNames)
		SLATE_ARGUMENT(bool, UseComponentButtonPresentation)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Result = EAttachZeroPickerResultImpl::Cancelled;
		AllItems.Add(MakeShared<FAttachZeroPickerItemImpl>(FAttachZeroPickerItemImpl{ NAME_None, LOCTEXT("AttachZeroRootOption", "None (Root)") }));

		TArray<FName> SortedNames = InArgs._ItemNames;
		SortedNames.Sort([](const FName& Left, const FName& Right)
		{
			return Left.ToString() < Right.ToString();
		});
		for (const FName ItemName : SortedNames)
		{
			if (!ItemName.IsNone())
			{
				AllItems.Add(MakeShared<FAttachZeroPickerItemImpl>(FAttachZeroPickerItemImpl{ ItemName, FText::FromName(ItemName) }));
			}
		}

		FilteredItems = AllItems;
		ChildSlot
		[
			SNew(SBorder)
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(InArgs._Title)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SAssignNew(SearchBox, SSearchBox)
					.OnTextChanged(this, &SAttachZeroNamePickerImpl::HandleSearchTextChangedImpl)
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FAttachZeroPickerItemImpl>>)
					.ListItemsSource(&FilteredItems)
					.OnGenerateRow(this, &SAttachZeroNamePickerImpl::MakeRowImpl)
					.OnMouseButtonDoubleClick(this, &SAttachZeroNamePickerImpl::HandleDoubleClickImpl)
					.SelectionMode(ESelectionMode::Single)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(InArgs._UseComponentButtonPresentation ? LOCTEXT("AttachZeroPickerConfirm", "Select") : LOCTEXT("AttachZeroPickerCancel", "Cancel"))
						.ButtonColorAndOpacity(InArgs._UseComponentButtonPresentation ? FLinearColor(0.03f, 0.20f, 0.07f, 1.0f) : FLinearColor::White)
						.ForegroundColor(InArgs._UseComponentButtonPresentation ? FSlateColor(FLinearColor::White) : FSlateColor::UseStyle())
						.OnClicked(this, InArgs._UseComponentButtonPresentation ? &SAttachZeroNamePickerImpl::HandleConfirmImpl : &SAttachZeroNamePickerImpl::HandleCancelImpl)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(InArgs._UseComponentButtonPresentation ? LOCTEXT("AttachZeroPickerCancel", "Cancel") : LOCTEXT("AttachZeroPickerConfirm", "Select"))
						.ButtonColorAndOpacity(InArgs._UseComponentButtonPresentation ? FLinearColor(0.25f, 0.05f, 0.05f, 1.0f) : FLinearColor::White)
						.ForegroundColor(InArgs._UseComponentButtonPresentation ? FSlateColor(FLinearColor::White) : FSlateColor::UseStyle())
						.OnClicked(this, InArgs._UseComponentButtonPresentation ? &SAttachZeroNamePickerImpl::HandleCancelImpl : &SAttachZeroNamePickerImpl::HandleConfirmImpl)
					]
					]
				]
			]
		;

		if (!FilteredItems.IsEmpty())
		{
			ListView->SetSelection(FilteredItems[0]);
		}
	}

	EAttachZeroPickerResultImpl GetResultImpl() const
	{
		return Result;
	}

	FName GetSelectedNameImpl() const
	{
		return SelectedName;
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			return HandleConfirmImpl();
		}
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return HandleCancelImpl();
		}
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	TSharedRef<ITableRow> MakeRowImpl(TSharedPtr<FAttachZeroPickerItemImpl> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FAttachZeroPickerItemImpl>>, OwnerTable)
		[
			SNew(STextBlock).Text(Item->Label)
		];
	}

	void HandleSearchTextChangedImpl(const FText& InText)
	{
		TArray<FString> SearchTokens;
		InText.ToString().ParseIntoArrayWS(SearchTokens);
		FilteredItems.Reset();
		for (const TSharedPtr<FAttachZeroPickerItemImpl>& Item : AllItems)
		{
			const FString ItemText = Item->Label.ToString();
			if (SearchTokens.ContainsByPredicate([&ItemText](const FString& Token)
			{
				return !ItemText.Contains(Token, ESearchCase::IgnoreCase);
			}) == false)
			{
				FilteredItems.Add(Item);
			}
		}
		ListView->RequestListRefresh();
		if (!FilteredItems.IsEmpty())
		{
			ListView->SetSelection(FilteredItems[0]);
		}
	}

	void HandleDoubleClickImpl(TSharedPtr<FAttachZeroPickerItemImpl> Item)
	{
		if (Item.IsValid())
		{
			SelectedName = Item->Value;
			Result = EAttachZeroPickerResultImpl::Confirmed;
			CloseWindowImpl();
		}
	}

	FReply HandleConfirmImpl()
	{
		const TArray<TSharedPtr<FAttachZeroPickerItemImpl>> SelectedItems = ListView->GetSelectedItems();
		if (SelectedItems.Num() == 1 && SelectedItems[0].IsValid())
		{
			SelectedName = SelectedItems[0]->Value;
			Result = EAttachZeroPickerResultImpl::Confirmed;
			CloseWindowImpl();
		}
		return FReply::Handled();
	}

	FReply HandleCancelImpl()
	{
		Result = EAttachZeroPickerResultImpl::Cancelled;
		CloseWindowImpl();
		return FReply::Handled();
	}

	void CloseWindowImpl()
	{
		if (TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
		{
			Window->RequestDestroyWindow();
		}
	}

	EAttachZeroPickerResultImpl Result;
	FName SelectedName;
	TArray<TSharedPtr<FAttachZeroPickerItemImpl>> AllItems;
	TArray<TSharedPtr<FAttachZeroPickerItemImpl>> FilteredItems;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SListView<TSharedPtr<FAttachZeroPickerItemImpl>>> ListView;
};

EAttachZeroPickerResultImpl RunAttachZeroNamePickerImpl(const FText& Title, const TArray<FName>& ItemNames, FName& OutSelectedName, FText& OutError, bool bUseComponentButtonPresentation = false)
{
	OutSelectedName = NAME_None;
	if (!FSlateApplication::IsInitialized())
	{
		OutError = LOCTEXT("AttachZeroPickerSlateUnavailable", "Slate is unavailable for the Attach Zero picker.");
		return EAttachZeroPickerResultImpl::Error;
	}

	TSharedPtr<SAttachZeroNamePickerImpl> Picker;
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(360.0f, 420.0f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SAssignNew(Picker, SAttachZeroNamePickerImpl)
			.Title(Title)
			.ItemNames(ItemNames)
			.UseComponentButtonPresentation(bUseComponentButtonPresentation)
		];

	FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow(), false);
	OutSelectedName = Picker->GetSelectedNameImpl();
	return Picker->GetResultImpl();
}

void GetAttachZeroParentComponentNamesImpl(AActor* ParentActor, TArray<FName>& OutComponentNames)
{
	OutComponentNames.Reset();
	if (!IsValid(ParentActor))
	{
		return;
	}

	TSet<FName> UniqueNames;
	TInlineComponentArray<USceneComponent*> Components(ParentActor);
	for (USceneComponent* Component : Components)
	{
		if (IsValid(Component) && !Component->GetFName().IsNone())
		{
			UniqueNames.Add(Component->GetFName());
		}
	}
	for (const FName ComponentName : UniqueNames)
	{
		OutComponentNames.Add(ComponentName);
	}
}

USceneComponent* FindAttachZeroParentComponentImpl(AActor* ParentActor, FName ComponentName)
{
	if (!IsValid(ParentActor) || ComponentName.IsNone())
	{
		return nullptr;
	}

	TInlineComponentArray<USceneComponent*> Components(ParentActor);
	for (USceneComponent* Component : Components)
	{
		if (IsValid(Component) && Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}
	return nullptr;
}

EAttachZeroPickerResultImpl ChooseAttachZeroParentTargetImpl(AActor* ParentActor, FName& OutComponentName, FName& OutSocketName, FText& OutError)
{
	OutComponentName = NAME_None;
	OutSocketName = NAME_None;
	TArray<FName> ComponentNames;
	GetAttachZeroParentComponentNamesImpl(ParentActor, ComponentNames);

	const EAttachZeroPickerResultImpl ComponentResult = RunAttachZeroNamePickerImpl(
		LOCTEXT("AttachZeroChooseComponent", "Choose Component"), ComponentNames, OutComponentName, OutError, true);
	if (ComponentResult != EAttachZeroPickerResultImpl::Confirmed)
	{
		return ComponentResult;
	}
	if (OutComponentName.IsNone())
	{
		return EAttachZeroPickerResultImpl::Confirmed;
	}

	USceneComponent* SelectedComponent = FindAttachZeroParentComponentImpl(ParentActor, OutComponentName);
	if (SelectedComponent == nullptr || SelectedComponent->GetOwner() != ParentActor)
	{
		OutError = LOCTEXT("AttachZeroPickerComponentInvalid", "The selected component no longer belongs to the attachment parent.");
		return EAttachZeroPickerResultImpl::Error;
	}
	if (Cast<USkeletalMeshComponent>(SelectedComponent) == nullptr)
	{
		return EAttachZeroPickerResultImpl::Confirmed;
	}

	const TArray<FName> SocketNames = SelectedComponent->GetAllSocketNames();
	const EAttachZeroPickerResultImpl SocketResult = RunAttachZeroNamePickerImpl(
		LOCTEXT("AttachZeroChooseSocketBone", "Choose Socket / Bone"), SocketNames, OutSocketName, OutError);
	if (SocketResult != EAttachZeroPickerResultImpl::Confirmed)
	{
		return SocketResult;
	}
	if (!OutSocketName.IsNone() && !SocketNames.Contains(OutSocketName))
	{
		OutError = LOCTEXT("AttachZeroPickerSocketInvalid", "The selected socket or bone no longer exists on the skeletal mesh component.");
		return EAttachZeroPickerResultImpl::Error;
	}
	return EAttachZeroPickerResultImpl::Confirmed;
}

FString NormalizeFMODEventSearchText(const FString& SearchText)
{
	FString NormalizedText = SearchText;
	NormalizedText.TrimStartAndEndInline();
	for (int32 CharacterIndex = 0; CharacterIndex < NormalizedText.Len(); ++CharacterIndex)
	{
		TCHAR& Character = NormalizedText[CharacterIndex];
		if (Character == TEXT('_') || Character == TEXT('-') || Character == TEXT('.') || Character == TEXT('/'))
		{
			Character = TEXT(' ');
		}
	}
	return NormalizedText;
}

bool NormalizeContentBrowserFolderPath(const FString& InFolderPath, FString& OutPackagePath, FText& OutError)
{
	OutPackagePath.Reset();
	FString TrimmedPath = InFolderPath;
	TrimmedPath.TrimStartAndEndInline();
	while (TrimmedPath.Len() > 1 && TrimmedPath.EndsWith(TEXT("/")))
	{
		TrimmedPath.RemoveFromEnd(TEXT("/"));
	}

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	if (FPackageName::IsValidLongPackageName(TrimmedPath))
	{
		OutPackagePath = TrimmedPath;
		return true;
	}

	if (ContentBrowserData == nullptr ||
		ContentBrowserData->TryConvertVirtualPath(TrimmedPath, OutPackagePath) != EContentBrowserPathType::Internal ||
		!FPackageName::IsValidLongPackageName(OutPackagePath))
	{
		OutPackagePath.Reset();
		OutError = LOCTEXT("InvalidFMODFolderPath", "The FMOD folder path is invalid.");
		return false;
	}
	return true;
}

void SaveLastFolderPath(const FString& PackagePath)
{
	UFMODSequencerToolsEditorUserSettings* UserSettings = GetMutableDefault<UFMODSequencerToolsEditorUserSettings>();
	UserSettings->LastFolderPath = PackagePath;
	UserSettings->SaveConfig();
}

bool QueryFMODEventsInFolder(const FString& PackagePath, TArray<UFMODEvent*>& OutEvents)
{
	OutEvents.Reset();
	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*PackagePath), Assets, false, false);

	TSet<UFMODEvent*> UniqueEvents;
	const FTopLevelAssetPath EventClassPath = UFMODEvent::StaticClass()->GetClassPathName();
	for (const FAssetData& AssetData : Assets)
	{
		if (AssetData.AssetClassPath != EventClassPath)
		{
			continue;
		}
		if (UFMODEvent* Event = Cast<UFMODEvent>(AssetData.GetAsset()); IsValid(Event))
		{
			UniqueEvents.Add(Event);
		}
	}
	OutEvents.Reserve(UniqueEvents.Num());
	for (UFMODEvent* Event : UniqueEvents)
	{
		OutEvents.Add(Event);
	}
	OutEvents.Sort([](const UFMODEvent& Left, const UFMODEvent& Right)
	{
		return Left.GetName().Compare(Right.GetName(), ESearchCase::IgnoreCase) < 0;
	});
	return true;
}

bool GetSelectedEvent(UFMODEvent*& OutEvent, FText& OutError)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	if (SelectedAssets.IsEmpty())
	{
		OutError = LOCTEXT("NoSelectedFMODEvent", "No FMOD Event is selected in the browser.");
		return false;
	}
	if (SelectedAssets.Num() != 1)
	{
		OutError = LOCTEXT("SelectExactlyOneEvent", "Select exactly one FMOD Event in the Content Browser.");
		return false;
	}

	OutEvent = Cast<UFMODEvent>(SelectedAssets[0].GetAsset());
	if (!IsValid(OutEvent))
	{
		OutError = LOCTEXT("SelectedEventInvalid", "The selected FMOD Event is invalid.");
		return false;
	}
	return true;
}

bool GetFocusedSequencer(TSharedPtr<ISequencer>& OutSequencer, FText& OutError)
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSubsystem == nullptr)
	{
		OutError = LOCTEXT("AssetEditorSubsystemUnavailable", "The Asset Editor subsystem is unavailable.");
		return false;
	}

	TArray<ILevelSequenceEditorToolkit*> LevelSequenceEditors;
	for (UObject* EditedAsset : AssetEditorSubsystem->GetAllEditedAssets())
	{
		if (!EditedAsset->IsA<ULevelSequence>())
		{
			continue;
		}
		for (IAssetEditorInstance* EditorInstance : AssetEditorSubsystem->FindEditorsForAsset(EditedAsset))
		{
			if (EditorInstance != nullptr)
			{
				LevelSequenceEditors.Add(static_cast<ILevelSequenceEditorToolkit*>(EditorInstance));
			}
		}
	}
	if (LevelSequenceEditors.Num() != 1)
	{
		OutError = LOCTEXT("OpenExactlyOneLevelSequence", "Open exactly one Level Sequence Editor before adding an FMOD Event.");
		return false;
	}

	OutSequencer = LevelSequenceEditors[0]->GetSequencer();
	if (!OutSequencer.IsValid())
	{
		OutError = LOCTEXT("SequencerUnavailable", "The open Level Sequence Editor has no active Sequencer.");
		return false;
	}
	return true;
}

bool GetDisplayedLocalFrame(ISequencer& Sequencer, bool bUseCustomTime, double CustomDisplayedTime, FFrameNumber& OutLocalFrame, FText& OutError)
{
	OutLocalFrame = Sequencer.GetLocalTime().Time.FloorToFrame();
	if (!bUseCustomTime)
	{
		return true;
	}
	if (!FMath::IsFinite(CustomDisplayedTime))
	{
		OutError = LOCTEXT("CustomTimeMustBeFinite", "The custom displayed time must be a finite number.");
		return false;
	}
	const FFrameRate DisplayRate = Sequencer.GetFocusedDisplayRate();
	const FFrameRate TickResolution = Sequencer.GetFocusedTickResolution();
	const USequencerSettings* SequencerSettings = Sequencer.GetSequencerSettings();
	if (!DisplayRate.IsValid() || !TickResolution.IsValid())
	{
		OutError = LOCTEXT("InvalidFocusedRates", "The focused Sequencer has an invalid display rate or tick resolution.");
		return false;
	}
	if (SequencerSettings == nullptr)
	{
		OutError = LOCTEXT("SequencerSettingsUnavailable", "The focused Sequencer settings are unavailable.");
		return false;
	}
	switch (SequencerSettings->GetTimeDisplayFormat())
	{
	case EFrameNumberDisplayFormats::Frames:
	{
		const double RoundedDisplayFrame = FMath::RoundToDouble(CustomDisplayedTime);
		if (RoundedDisplayFrame < static_cast<double>(TNumericLimits<int32>::Min()) || RoundedDisplayFrame > static_cast<double>(TNumericLimits<int32>::Max()))
		{
			OutError = LOCTEXT("CustomFrameOutOfRange", "The custom display frame is outside the supported frame range.");
			return false;
		}
		OutLocalFrame = FFrameRate::TransformTime(FFrameTime(FFrameNumber(static_cast<int32>(RoundedDisplayFrame))), DisplayRate, TickResolution).RoundToFrame();
		break;
	}
	case EFrameNumberDisplayFormats::Seconds:
		OutLocalFrame = TickResolution.AsFrameTime(CustomDisplayedTime).RoundToFrame();
		break;
	default:
		OutError = LOCTEXT("UnsupportedTimeDisplayFormat", "Custom displayed time supports only Frames or Seconds. Change the focused Sequencer display format and try again.");
		return false;
	}
	Sequencer.SetLocalTime(OutLocalFrame, ESnapTimeMode::STM_None, true);
	return true;
}

void AddBindingForTrack(const UMovieScene* MovieScene, const UMovieSceneTrack* Track, TSet<FGuid>& OutBindings)
{
	if (MovieScene == nullptr || Track == nullptr)
	{
		return;
	}
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		if (Binding.GetTracks().Contains(Track))
		{
			OutBindings.Add(Binding.GetObjectGuid());
		}
	}
}

void AddBindingForSection(const UMovieScene* MovieScene, const UMovieSceneSection* Section, TSet<FGuid>& OutBindings)
{
	if (MovieScene == nullptr || Section == nullptr)
	{
		return;
	}
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (Track && Track->GetAllSections().Contains(Section))
			{
				OutBindings.Add(Binding.GetObjectGuid());
			}
		}
	}
}

bool AddBindingFromViewModel(UE::Sequencer::FViewModelPtr Model, TSet<FGuid>& OutBindings, FText& OutError)
{
	constexpr int32 MaxParentDepth = 256;
	TSet<const UE::Sequencer::FViewModel*> VisitedModels;
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] ViewModel resolver enter"));

	for (int32 Depth = 0; Model; ++Depth)
	{
		const UE::Sequencer::FViewModel* Current = Model.Get();
		if (Current == nullptr || Depth >= MaxParentDepth || VisitedModels.Contains(Current))
		{
			UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Invalid parent hierarchy at depth %d (current=%p parent=%p)"), Depth, Current, static_cast<const void*>(nullptr));
			OutError = LOCTEXT("InvalidSelectionHierarchy", "Sequencer selection hierarchy is invalid or cyclic.");
			return false;
		}
		VisitedModels.Add(Current);

		if (UE::Sequencer::IObjectBindingExtension* Binding = Model->CastThis<UE::Sequencer::IObjectBindingExtension>())
		{
			OutBindings.Add(Binding->GetObjectGuid());
			UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] ViewModel resolver exit: binding found at depth %d"), Depth);
			return true;
		}

		UE::Sequencer::FViewModelPtr Parent = Model->GetParent();
		if (Parent && (Parent.Get() == Current || VisitedModels.Contains(Parent.Get())))
		{
			UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Invalid parent hierarchy at depth %d (current=%p parent=%p)"), Depth, Current, Parent.Get());
			OutError = LOCTEXT("InvalidSelectionHierarchy", "Sequencer selection hierarchy is invalid or cyclic.");
			return false;
		}
		Model = MoveTemp(Parent);
	}

	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] ViewModel resolver exit: no binding"));
	return true;
}

bool ResolveSelectedComponentBinding(ISequencer& Sequencer, FGuid& OutComponentGuid, FText& OutError)
{
	UMovieSceneSequence* FocusedSequence = Sequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;
	if (MovieScene == nullptr)
	{
		OutError = LOCTEXT("FocusedSequenceUnavailable", "The focused sequence or its Movie Scene is unavailable.");
		return false;
	}

	TSet<FGuid> CandidateBindings;
	TArray<FGuid> SelectedObjects;
	Sequencer.GetSelectedObjects(SelectedObjects);
	CandidateBindings.Append(SelectedObjects);
	TArray<UMovieSceneTrack*> SelectedTracks;
	Sequencer.GetSelectedTracks(SelectedTracks);
	for (UMovieSceneTrack* Track : SelectedTracks) AddBindingForTrack(MovieScene, Track, CandidateBindings);
	TArray<UMovieSceneSection*> SelectedSections;
	Sequencer.GetSelectedSections(SelectedSections);
	for (UMovieSceneSection* Section : SelectedSections) AddBindingForSection(MovieScene, Section, CandidateBindings);
	int32 SelectedKeyCount = 0;
	int32 SelectedOutlinerItemCount = 0;
	bool bLoggedSelectionSizes = false;

	if (TSharedPtr<UE::Sequencer::FSequencerEditorViewModel> ViewModel = Sequencer.GetViewModel())
	{
		if (TSharedPtr<UE::Sequencer::FSequencerSelection> Selection = ViewModel->GetSelection())
		{
			SelectedKeyCount = Selection->KeySelection.Num();
			SelectedOutlinerItemCount = Selection->Outliner.GetSelected().Num();
			UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Selection sizes: objects=%d tracks=%d sections=%d keys=%d outliner=%d"), SelectedObjects.Num(), SelectedTracks.Num(), SelectedSections.Num(), SelectedKeyCount, SelectedOutlinerItemCount);
			bLoggedSelectionSizes = true;
			for (const FKeyHandle Key : Selection->KeySelection)
			{
				if (UE::Sequencer::TViewModelPtr<UE::Sequencer::FChannelModel> Channel = Selection->KeySelection.GetModelForKey(Key))
				{
					AddBindingForSection(MovieScene, Channel->GetSection(), CandidateBindings);
				}
			}
			for (const UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::IOutlinerExtension>& WeakItem : Selection->Outliner.GetSelected())
			{
				if (!AddBindingFromViewModel(WeakItem.Pin(), CandidateBindings, OutError))
				{
					return false;
				}
			}
		}
	}
	if (!bLoggedSelectionSizes)
	{
		UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Selection sizes: objects=%d tracks=%d sections=%d keys=%d outliner=%d"), SelectedObjects.Num(), SelectedTracks.Num(), SelectedSections.Num(), SelectedKeyCount, SelectedOutlinerItemCount);
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Candidate bindings=%d"), CandidateBindings.Num());

	TSet<FGuid> ComponentBindings;
	const UMovieScene* ConstMovieScene = MovieScene;
	for (const FGuid& Candidate : CandidateBindings)
	{
		UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] FindObjects candidate begin"));
		const TArrayView<TWeakObjectPtr<>> Objects = Sequencer.FindObjectsInCurrentSequence(Candidate);
		UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] FindObjects candidate end: objects=%d"), Objects.Num());
		bool bContainsFMODComponent = false;
		bool bContainsAmbientSound = false;
		for (const TWeakObjectPtr<>& Object : Objects)
		{
			bContainsFMODComponent |= Object.IsValid() && Object->IsA<UFMODAudioComponent>();
			bContainsAmbientSound |= Object.IsValid() && Object->IsA<AFMODAmbientSound>();
		}
		if (bContainsFMODComponent)
		{
			UFMODAudioComponent* Component = nullptr;
			if (!ResolveExactlyOneFMODAudioComponentImpl(Sequencer, Candidate, Component, OutError,
				LOCTEXT("SelectedFMODBindingNotSingleComponent", "The selected FMOD binding does not resolve to one live FMOD Audio Component."),
				LOCTEXT("SelectedFMODBindingMultipleObjects", "The selected FMOD binding resolves to multiple objects.")))
			{
				return false;
			}
			ComponentBindings.Add(Candidate);
		}
		else if (bContainsAmbientSound)
		{
			UObject* Object = nullptr;
			if (!ResolveExactlyOneLiveObjectImpl(Sequencer, Candidate, Object, OutError,
				LOCTEXT("SelectedFMODBindingNotSingleAmbientSound", "The selected FMOD binding does not resolve to one live FMOD Ambient Sound."),
				LOCTEXT("SelectedFMODBindingMultipleObjects", "The selected FMOD binding resolves to multiple objects.")))
			{
				return false;
			}
			AFMODAmbientSound* AmbientSound = Cast<AFMODAmbientSound>(Object);
			if (AmbientSound == nullptr)
			{
				OutError = LOCTEXT("SelectedFMODBindingNotSingleAmbientSound", "The selected FMOD binding does not resolve to one live FMOD Ambient Sound.");
				return false;
			}
			for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
			{
				const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
				if (Possessable && Possessable->GetParent() == Candidate)
				{
					UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] FindObjects child begin"));
					const TArrayView<TWeakObjectPtr<>> ChildObjects = Sequencer.FindObjectsInCurrentSequence(Binding.GetObjectGuid());
					UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] FindObjects child end: objects=%d"), ChildObjects.Num());
					bool bContainsAmbientComponent = false;
					for (const TWeakObjectPtr<>& ChildObject : ChildObjects)
					{
						bContainsAmbientComponent |= ChildObject.Get() == AmbientSound->AudioComponent && ChildObject->IsA<UFMODAudioComponent>();
					}
					if (bContainsAmbientComponent)
					{
						UFMODAudioComponent* ChildComponent = nullptr;
						if (!ResolveExactlyOneFMODAudioComponentImpl(Sequencer, Binding.GetObjectGuid(), ChildComponent, OutError,
							LOCTEXT("SelectedFMODChildBindingNotSingleComponent", "The selected FMOD binding does not resolve to one live FMOD Audio Component."),
							LOCTEXT("SelectedFMODBindingMultipleObjects", "The selected FMOD binding resolves to multiple objects.")))
						{
							return false;
						}
						if (ChildComponent == AmbientSound->AudioComponent)
						{
							ComponentBindings.Add(Binding.GetObjectGuid());
						}
					}
				}
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Component bindings=%d"), ComponentBindings.Num());
	if (ComponentBindings.Num() == 0)
	{
		OutError = LOCTEXT("NoSelectedFMODComponent", "Select an FMOD Ambient Sound, FMOD Audio Component, Sound Track, Playback Track, section, key, or outliner row in the focused Sequencer.");
		return false;
	}
	if (ComponentBindings.Num() != 1)
	{
		OutError = LOCTEXT("AmbiguousSelectedFMODComponents", "The selection resolves to multiple FMOD Audio Components. Select exactly one FMOD hierarchy.");
		return false;
	}
	OutComponentGuid = *ComponentBindings.CreateConstIterator();
	return true;
}

bool AddNewToFocusedSequencerWithExplicitEventImpl(UFMODEvent* Event, bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	TSharedPtr<ISequencer> Sequencer;
	if (!GetFocusedSequencer(Sequencer, OutError)) return false;
	UMovieSceneSequence* FocusedSequence = Sequencer->GetFocusedMovieSceneSequence();
	if (FocusedSequence == nullptr || FocusedSequence->GetMovieScene() == nullptr)
	{
		OutError = LOCTEXT("FocusedSequenceUnavailable", "The focused sequence or its Movie Scene is unavailable.");
		return false;
	}
	FFrameNumber LocalFrame;
	if (!GetDisplayedLocalFrame(*Sequencer, bUseCustomTime, CustomDisplayedTime, LocalFrame, OutError)) return false;
	return FFMODSequencerEditorAPI::AddNewAmbientSound(*Sequencer, Event, LocalFrame, OutError);
}

bool AddToSelectedWithExplicitEventImpl(UFMODEvent* Event, bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	TSharedPtr<ISequencer> Sequencer;
	if (!GetFocusedSequencer(Sequencer, OutError))
	{
		UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Exit error: %s"), *OutError.ToString());
		return false;
	}
	FFrameNumber LocalFrame;
	if (!GetDisplayedLocalFrame(*Sequencer, bUseCustomTime, CustomDisplayedTime, LocalFrame, OutError))
	{
		UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Exit error: %s"), *OutError.ToString());
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Focused Sequencer/time resolved: frame=%d"), LocalFrame.Value);
	FGuid ComponentBindingGuid;
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] ResolveSelectedComponentBinding begin"));
	if (!ResolveSelectedComponentBinding(*Sequencer, ComponentBindingGuid, OutError))
	{
		UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] ResolveSelectedComponentBinding end: error=%s"), *OutError.ToString());
		UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Exit error: %s"), *OutError.ToString());
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] ResolveSelectedComponentBinding end: success"));
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade begin"));
	const bool bSucceeded = FFMODSequencerEditorAPI::AddKeysToExistingComponent(*Sequencer, ComponentBindingGuid, Event, LocalFrame, OutError);
	if (bSucceeded)
	{
		UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Facade end: success"));
		UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Exit success"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Facade end: %s"), *OutError.ToString());
		UE_LOG(LogTemp, Error, TEXT("[FMOD AddSelected] Exit error"));
	}
	return bSucceeded;
}

bool AddStopKeyToSelectedAtDisplayedTimeImpl(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	OutError = FText::GetEmpty();
	TSharedPtr<ISequencer> Sequencer;
	if (!GetFocusedSequencer(Sequencer, OutError)) return false;

	FFrameNumber LocalFrame;
	if (!GetDisplayedLocalFrame(*Sequencer, bUseCustomTime, CustomDisplayedTime, LocalFrame, OutError)) return false;

	FGuid ComponentBindingGuid;
	if (!ResolveSelectedComponentBinding(*Sequencer, ComponentBindingGuid, OutError)) return false;

	return FFMODSequencerEditorAPI::AddStopKeyToExistingComponent(*Sequencer, ComponentBindingGuid, LocalFrame, OutError);
}

struct FAttachZeroSource
{
	FGuid ActorBinding;
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<USceneComponent> RootComponent;
};

FString GetAttachZeroSourceDisplayNameImpl(UMovieScene& MovieScene, const FAttachZeroSource& Source)
{
	if (AActor* SourceActor = Source.Actor.Get(); IsValid(SourceActor))
	{
		const FString& ActorLabel = SourceActor->GetActorLabel(false);
		return ActorLabel.IsEmpty() ? SourceActor->GetName() : ActorLabel;
	}
	if (const FMovieScenePossessable* Possessable = MovieScene.FindPossessable(Source.ActorBinding))
	{
		if (!Possessable->GetName().IsEmpty()) return Possessable->GetName();
	}
	return Source.ActorBinding.ToString();
}

struct FAttachZeroSelection
{
	FGuid ParentActorBinding;
	FName ParentComponentName;
	FName ParentSocketName;
	TWeakObjectPtr<AActor> ParentActor;
	TWeakObjectPtr<USceneComponent> ParentComponent;
	TArray<FAttachZeroSource> Sources;
};

void AddAttachZeroBindingForTrackImpl(const UMovieScene* MovieScene, const UMovieSceneTrack* Track, TSet<FGuid>& OutBindings)
{
	if (MovieScene == nullptr || Track == nullptr)
	{
		return;
	}
	const UMovieScene* ConstMovieScene = MovieScene;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		if (Binding.GetTracks().Contains(Track))
		{
			OutBindings.Add(Binding.GetObjectGuid());
		}
	}
}

void AddAttachZeroBindingForSectionImpl(const UMovieScene* MovieScene, const UMovieSceneSection* Section, TSet<FGuid>& OutBindings)
{
	if (MovieScene == nullptr || Section == nullptr)
	{
		return;
	}
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (Track != nullptr && Track->GetAllSections().Contains(Section))
			{
				OutBindings.Add(Binding.GetObjectGuid());
			}
		}
	}
}

bool AddAttachZeroBindingFromViewModelImpl(UE::Sequencer::FViewModelPtr Model, TSet<FGuid>& OutBindings, FText& OutError)
{
	constexpr int32 MaxParentDepth = 256;
	TSet<const UE::Sequencer::FViewModel*> VisitedModels;
	for (int32 Depth = 0; Model; ++Depth)
	{
		const UE::Sequencer::FViewModel* Current = Model.Get();
		if (Current == nullptr || Depth >= MaxParentDepth || VisitedModels.Contains(Current))
		{
			OutError = LOCTEXT("AttachZeroInvalidSelectionHierarchy", "Sequencer selection hierarchy is invalid or cyclic.");
			return false;
		}
		VisitedModels.Add(Current);
		if (UE::Sequencer::IObjectBindingExtension* Binding = Model->CastThis<UE::Sequencer::IObjectBindingExtension>())
		{
			OutBindings.Add(Binding->GetObjectGuid());
			return true;
		}
		UE::Sequencer::FViewModelPtr Parent = Model->GetParent();
		if (Parent && (Parent.Get() == Current || VisitedModels.Contains(Parent.Get())))
		{
			OutError = LOCTEXT("AttachZeroInvalidSelectionHierarchy", "Sequencer selection hierarchy is invalid or cyclic.");
			return false;
		}
		Model = MoveTemp(Parent);
	}
	return true;
}

bool ResolveActorBindingForActorImpl(ISequencer& Sequencer, UMovieScene* MovieScene, AActor* Actor, FGuid& OutActorBinding, FText& OutError, const FText& MissingBindingError, const FText& AmbiguousBindingError)
{
	OutActorBinding.Invalidate();
	if (!IsValid(Actor) || MovieScene == nullptr)
	{
		OutError = MissingBindingError;
		return false;
	}
	TSet<FGuid> MatchingBindings;
	const UMovieScene* ConstMovieScene = MovieScene;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		const TArrayView<TWeakObjectPtr<>> Objects = Sequencer.FindObjectsInCurrentSequence(Binding.GetObjectGuid());
		bool bContainsActor = false;
		for (const TWeakObjectPtr<>& Object : Objects)
		{
			if (Object.Get() == Actor)
			{
				bContainsActor = true;
			}
		}
		if (!bContainsActor)
		{
			continue;
		}
		AActor* ResolvedActor = nullptr;
		FText IgnoredError;
		if (!ResolveExactlyOneActorImpl(Sequencer, Binding.GetObjectGuid(), ResolvedActor, IgnoredError, MissingBindingError, AmbiguousBindingError) || ResolvedActor != Actor)
		{
			OutError = AmbiguousBindingError;
			return false;
		}
		MatchingBindings.Add(Binding.GetObjectGuid());
	}
	if (MatchingBindings.Num() == 0)
	{
		OutError = MissingBindingError;
		return false;
	}
	if (MatchingBindings.Num() != 1)
	{
		OutError = AmbiguousBindingError;
		return false;
	}
	OutActorBinding = *MatchingBindings.CreateConstIterator();
	return true;
}

bool ResolveFMODSourceActorBindingImpl(ISequencer& Sequencer, UMovieScene* MovieScene, const FGuid& CandidateBinding, FGuid& OutActorBinding, AActor*& OutActor, FText& OutError)
{
	OutActorBinding.Invalidate();
	OutActor = nullptr;
	const TArrayView<TWeakObjectPtr<>> Objects = Sequencer.FindObjectsInCurrentSequence(CandidateBinding);
	bool bContainsFMODComponent = false;
	bool bContainsFMODActor = false;
	for (const TWeakObjectPtr<>& Object : Objects)
	{
		bContainsFMODComponent |= Object.IsValid() && Object->IsA<UFMODAudioComponent>();
		if (AActor* Actor = Cast<AActor>(Object.Get()))
		{
			bContainsFMODActor |= IsValid(Actor) && Actor->FindComponentByClass<UFMODAudioComponent>() != nullptr;
		}
	}
	if (bContainsFMODComponent)
	{
		UFMODAudioComponent* Component = nullptr;
		if (!ResolveExactlyOneFMODAudioComponentImpl(Sequencer, CandidateBinding, Component, OutError,
			LOCTEXT("AttachZeroFMODBindingNotSingleComponent", "The selected FMOD binding does not resolve to one live FMOD Audio Component."),
			LOCTEXT("AttachZeroFMODBindingMultipleObjects", "The selected FMOD binding resolves to multiple objects.")))
		{
			return false;
		}
		OutActor = Component->GetOwner();
	}
	else if (bContainsFMODActor)
	{
		if (!ResolveExactlyOneActorImpl(Sequencer, CandidateBinding, OutActor, OutError,
			LOCTEXT("AttachZeroFMODActorBindingNotSingle", "The FMOD actor binding does not resolve to one live Actor."),
			LOCTEXT("AttachZeroFMODActorBindingMultipleObjects", "The FMOD actor binding resolves to multiple objects.")))
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	return ResolveActorBindingForActorImpl(Sequencer, MovieScene, OutActor, OutActorBinding, OutError,
		LOCTEXT("AttachZeroFMODOwnerActorBindingMissing", "The FMOD source owner actor does not have an existing binding in the focused Movie Scene."),
		LOCTEXT("AttachZeroFMODActorBindingAmbiguous", "The FMOD actor binding is ambiguous."));
}

bool ResolveAttachZeroSelectionImpl(ISequencer& Sequencer, FAttachZeroSelection& OutSelection, FText& OutError)
{
	UMovieSceneSequence* FocusedSequence = Sequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;
	if (MovieScene == nullptr)
	{
		OutError = LOCTEXT("AttachZeroFocusedSequenceUnavailable", "The focused sequence or its Movie Scene is unavailable.");
		return false;
	}

	TSet<FGuid> Candidates;
	TArray<FGuid> SelectedObjects;
	Sequencer.GetSelectedObjects(SelectedObjects);
	for (const FGuid& Binding : SelectedObjects) Candidates.Add(Binding);
	TArray<UMovieSceneTrack*> SelectedTracks;
	Sequencer.GetSelectedTracks(SelectedTracks);
	for (const UMovieSceneTrack* Track : SelectedTracks) AddAttachZeroBindingForTrackImpl(MovieScene, Track, Candidates);
	TArray<UMovieSceneSection*> SelectedSections;
	Sequencer.GetSelectedSections(SelectedSections);
	for (const UMovieSceneSection* Section : SelectedSections) AddAttachZeroBindingForSectionImpl(MovieScene, Section, Candidates);
	if (TSharedPtr<UE::Sequencer::FSequencerEditorViewModel> ViewModel = Sequencer.GetViewModel())
	{
		if (TSharedPtr<UE::Sequencer::FSequencerSelection> Selection = ViewModel->GetSelection())
		{
			for (const FKeyHandle Key : Selection->KeySelection)
			{
				if (UE::Sequencer::TViewModelPtr<UE::Sequencer::FChannelModel> Channel = Selection->KeySelection.GetModelForKey(Key))
				{
					AddAttachZeroBindingForSectionImpl(MovieScene, Channel->GetSection(), Candidates);
				}
			}
			for (const UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::IOutlinerExtension>& WeakItem : Selection->Outliner.GetSelected())
			{
				if (!AddAttachZeroBindingFromViewModelImpl(WeakItem.Pin(), Candidates, OutError)) return false;
			}
		}
	}

	TMap<FGuid, AActor*> FMODSourceActors;
	TSet<FGuid> FMODCandidates;
	for (const FGuid& Candidate : Candidates)
	{
		FGuid SourceActorBinding;
		AActor* SourceActor = nullptr;
		FText SourceResolutionError;
		const bool bResolvedFMODActorBinding = ResolveFMODSourceActorBindingImpl(Sequencer, MovieScene, Candidate, SourceActorBinding, SourceActor, SourceResolutionError);
		if (!bResolvedFMODActorBinding && !SourceResolutionError.IsEmpty())
		{
			OutError = LOCTEXT("AttachZeroInvalidSource", "One or more selected FMOD sources no longer resolve to a valid actor binding.");
			return false;
		}
		if (bResolvedFMODActorBinding)
		{
			FMODCandidates.Add(Candidate);
			FMODSourceActors.FindOrAdd(SourceActorBinding) = SourceActor;
		}
	}
	if (FMODSourceActors.Num() == 0)
	{
		OutError = LOCTEXT("AttachZeroNoFMODSource", "Select one or more FMOD sources and exactly one non-FMOD attachment target.");
		return false;
	}

	TSet<FGuid> ParentCandidates;
	for (const FGuid& Candidate : Candidates)
	{
		if (!FMODCandidates.Contains(Candidate)) ParentCandidates.Add(Candidate);
	}
	if (ParentCandidates.Num() == 0)
	{
		OutError = LOCTEXT("AttachZeroNoParent", "Select exactly one non-FMOD attachment target in addition to the FMOD sources.");
		return false;
	}
	if (ParentCandidates.Num() != 1)
	{
		OutError = LOCTEXT("AttachZeroMultipleParents", "Multiple attachment targets are selected. Select exactly one non-FMOD target.");
		return false;
	}

	const FGuid ParentCandidate = *ParentCandidates.CreateConstIterator();
	AActor* ParentActor = nullptr;
	USceneComponent* ParentComponent = nullptr;
	const TArrayView<TWeakObjectPtr<>> ParentObjects = Sequencer.FindObjectsInCurrentSequence(ParentCandidate);
	bool bContainsParentSceneComponent = false;
	bool bContainsParentActor = false;
	for (const TWeakObjectPtr<>& Object : ParentObjects)
	{
		bContainsParentSceneComponent |= Object.IsValid() && Object->IsA<USceneComponent>();
		bContainsParentActor |= Object.IsValid() && Object->IsA<AActor>();
	}
	if (bContainsParentSceneComponent)
	{
		if (!ResolveExactlyOneSceneComponentImpl(Sequencer, ParentCandidate, ParentComponent, OutError,
			LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component."),
			LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component.")))
		{
			return false;
		}
		ParentActor = ParentComponent->GetOwner();
	}
	else if (bContainsParentActor)
	{
		if (!ResolveExactlyOneActorImpl(Sequencer, ParentCandidate, ParentActor, OutError,
			LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component."),
			LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component.")))
		{
			return false;
		}
	}
	else
	{
		OutError = LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component.");
		return false;
	}
	if (!ResolveActorBindingForActorImpl(Sequencer, MovieScene, ParentActor, OutSelection.ParentActorBinding, OutError,
		LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component."),
		LOCTEXT("AttachZeroInvalidParent", "The selected attachment target does not resolve to one valid Actor or Scene Component.")))
	{
		return false;
	}
	OutSelection.ParentActor = ParentActor;
	OutSelection.ParentComponent = ParentComponent;
	if (ParentComponent != nullptr)
	{
		if (ParentComponent->GetOwner() != ParentActor)
		{
			OutError = LOCTEXT("AttachZeroInvalidParentComponent", "The selected component does not belong to the resolved parent actor.");
			return false;
		}
		OutSelection.ParentComponentName = ParentComponent->GetFName();
	}

	OutSelection.Sources.Reserve(FMODSourceActors.Num());
	for (const TPair<FGuid, AActor*>& SourcePair : FMODSourceActors)
	{
		AActor* SourceActor = SourcePair.Value;
		USceneComponent* SourceRoot = IsValid(SourceActor) ? SourceActor->GetRootComponent() : nullptr;
		if (!IsValid(SourceActor) || !IsValid(SourceRoot))
		{
			OutError = LOCTEXT("AttachZeroInvalidSource", "One or more selected FMOD sources no longer resolve to a valid actor binding.");
			return false;
		}
		FAttachZeroSource& Source = OutSelection.Sources.AddDefaulted_GetRef();
		Source.ActorBinding = SourcePair.Key;
		Source.Actor = SourceActor;
		Source.RootComponent = SourceRoot;
	}
	return true;
}

bool ValidateAttachCycleImpl(UMovieScene* MovieScene, const FGuid& SourceActorBinding, const FGuid& ParentActorBinding, FText& OutError)
{
	if (SourceActorBinding == ParentActorBinding)
	{
		OutError = LOCTEXT("AttachZeroSameBinding", "The FMOD source and attachment parent must be different actor bindings.");
		return false;
	}
	TSet<FGuid> Visited;
	TArray<FGuid> Pending;
	Pending.Add(ParentActorBinding);
	while (!Pending.IsEmpty())
	{
		const FGuid Current = Pending.Pop(EAllowShrinking::No);
		if (Visited.Contains(Current)) continue;
		Visited.Add(Current);
		if (Current == SourceActorBinding)
		{
			OutError = LOCTEXT("AttachZeroCycle", "The requested attachment would create a cycle.");
			return false;
		}
		const FMovieSceneBinding* Binding = MovieScene ? MovieScene->FindBinding(Current) : nullptr;
		if (Binding == nullptr) continue;
		for (UMovieSceneTrack* Track : Binding->GetTracks())
		{
			if (UMovieScene3DAttachTrack* AttachTrack = Cast<UMovieScene3DAttachTrack>(Track))
			{
				for (UMovieSceneSection* Section : AttachTrack->GetAllSections())
				{
					if (UMovieScene3DAttachSection* AttachSection = Cast<UMovieScene3DAttachSection>(Section))
					{
						const FGuid Target = AttachSection->GetConstraintBindingID().GetGuid();
						if (Target.IsValid()) Pending.Add(Target);
					}
				}
			}
		}
	}
	return true;
}

struct FAttachZeroSourcePlan
{
	FAttachZeroSource Source;
	TArray<UMovieSceneTrack*> TransformTracksToRemove;
	UMovieScene3DAttachTrack* ExistingAttachTrack = nullptr;
	UMovieScene3DTransformTrack* CreatedTransformTrack = nullptr;
	UMovieScene3DAttachTrack* CreatedAttachTrack = nullptr;
	UMovieScene3DAttachSection* CreatedAttachSection = nullptr;
	FVector InitialRelativeLocation = FVector::ZeroVector;
	FRotator InitialRelativeRotation = FRotator::ZeroRotator;
	FVector InitialRelativeScale = FVector::OneVector;
};

struct FAttachZeroBatchPlan
{
	FAttachZeroSelection Selection;
	TArray<FAttachZeroSourcePlan> Sources;
	bool bErrorAlreadyStatesNoChangesMade = false;
};

void AppendAttachZeroNoChangesMadeImpl(FText& InOutError)
{
	const FText NoChanges = LOCTEXT("AttachZeroNoChangesMade", "No changes were made.");
	InOutError = InOutError.IsEmpty() ? NoChanges : FText::Format(LOCTEXT("AttachZeroNoChangesMadeFormat", "{0} {1}"), InOutError, NoChanges);
}

bool ValidateAttachZeroTrackClassImpl(UClass* TrackClass, UClass* SectionClass, const FText& Error, FText& OutError)
{
	if (TrackClass == nullptr || TrackClass->HasAnyClassFlags(CLASS_Abstract) || !UMovieScene::IsTrackClassAllowed(TrackClass))
	{
		OutError = Error;
		return false;
	}
	const UMovieSceneTrack* TrackCDO = Cast<UMovieSceneTrack>(TrackClass->GetDefaultObject());
	if (TrackCDO == nullptr || !TrackCDO->SupportsType(SectionClass))
	{
		OutError = Error;
		return false;
	}
	return true;
}

bool BuildAttachZeroBatchPlanImpl(ISequencer& Sequencer, FAttachZeroBatchPlan& OutPlan, FText& OutError)
{
	UMovieSceneSequence* FocusedSequence = Sequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;
	if (MovieScene == nullptr || !ResolveAttachZeroSelectionImpl(Sequencer, OutPlan.Selection, OutError))
	{
		if (OutError.IsEmpty()) OutError = LOCTEXT("AttachZeroFocusedSequenceUnavailable", "The focused sequence or its Movie Scene is unavailable.");
		return false;
	}

	AActor* ParentActor = OutPlan.Selection.ParentActor.Get();
	if (!IsValid(ParentActor))
	{
		OutError = LOCTEXT("AttachZeroInvalidParentAfterResolve", "The selected attachment target does not resolve to one valid Actor or Scene Component.");
		return false;
	}

	TSet<FString> ExistingAttachSourceNameSet;
	TArray<FString> ExistingAttachSourceNames;
	for (const FAttachZeroSource& Source : OutPlan.Selection.Sources)
	{
		UMovieScene3DAttachTrack* ExistingAttachTrack = MovieScene->FindTrack<UMovieScene3DAttachTrack>(Source.ActorBinding);
		if (ExistingAttachTrack == nullptr)
		{
			continue;
		}
		for (UMovieSceneSection* ExistingAttachSection : ExistingAttachTrack->GetAllSections())
		{
			if (IsValid(ExistingAttachSection))
			{
				const FString SourceName = GetAttachZeroSourceDisplayNameImpl(*MovieScene, Source);
				if (!ExistingAttachSourceNameSet.Contains(SourceName))
				{
					ExistingAttachSourceNameSet.Add(SourceName);
					ExistingAttachSourceNames.Add(SourceName);
				}
				break;
			}
		}
	}
	if (!ExistingAttachSourceNames.IsEmpty())
	{
		ExistingAttachSourceNames.StableSort();
		FString ExistingAttachSourceList;
		for (const FString& SourceName : ExistingAttachSourceNames)
		{
			if (!ExistingAttachSourceList.IsEmpty()) ExistingAttachSourceList += TEXT("\n");
			ExistingAttachSourceList += TEXT("- ");
			ExistingAttachSourceList += SourceName;
		}
		OutPlan.bErrorAlreadyStatesNoChangesMade = true;
		OutError = OutPlan.Selection.Sources.Num() == 1
			? LOCTEXT("AttachZeroExistingAttachment", "The selected FMOD source already has an attachment. Remove the existing Attach section before creating a new Attach Zero.")
			: LOCTEXT("AttachZeroExistingAttachments", "One or more selected FMOD sources already have an attachment. Remove the existing Attach sections before creating a new Attach Zero. No changes were made.");
		FMessageDialog::Open(
			EAppMsgCategory::Info,
			EAppMsgType::Ok,
			FText::Format(LOCTEXT("AttachZeroExistingAttachmentsDialogMessage", "The following FMOD objects already have an Attach section:\n\n{0}\n\nDelete the existing Attach section or Attach track from these objects, then run Attach Zero again.\n\nNo changes were made."), FText::FromString(ExistingAttachSourceList)),
			LOCTEXT("AttachZeroExistingAttachmentsDialogTitle", "Attach Zero Cannot Continue"));
		return false;
	}

	FName SelectedParentComponentName;
	FName SelectedParentSocketName;
	const EAttachZeroPickerResultImpl PickerResult = ChooseAttachZeroParentTargetImpl(ParentActor, SelectedParentComponentName, SelectedParentSocketName, OutError);
	if (PickerResult != EAttachZeroPickerResultImpl::Confirmed)
	{
		return false;
	}
	OutPlan.Selection.ParentComponentName = SelectedParentComponentName;
	OutPlan.Selection.ParentSocketName = SelectedParentSocketName;
	USceneComponent* SelectedParentComponent = FindAttachZeroParentComponentImpl(ParentActor, SelectedParentComponentName);
	if (!SelectedParentComponentName.IsNone() && (SelectedParentComponent == nullptr || SelectedParentComponent->GetOwner() != ParentActor))
	{
		OutError = LOCTEXT("AttachZeroPickerComponentInvalid", "The selected component no longer belongs to the attachment parent.");
		return false;
	}
	if (!SelectedParentSocketName.IsNone() && (Cast<USkeletalMeshComponent>(SelectedParentComponent) == nullptr || !SelectedParentComponent->GetAllSocketNames().Contains(SelectedParentSocketName)))
	{
		OutError = LOCTEXT("AttachZeroPickerSocketInvalid", "The selected socket or bone no longer exists on the skeletal mesh component.");
		return false;
	}

	if (!ValidateAttachZeroTrackClassImpl(UMovieScene3DTransformTrack::StaticClass(), UMovieScene3DTransformSection::StaticClass(), LOCTEXT("AttachZeroTransformTrackUnavailable", "The FMOD source transform track class is unavailable or disallowed by this Movie Scene."), OutError)
		|| !ValidateAttachZeroTrackClassImpl(UMovieScene3DAttachTrack::StaticClass(), UMovieScene3DAttachSection::StaticClass(), LOCTEXT("AttachZeroAttachTrackUnavailable", "The FMOD source attachment track class is unavailable or disallowed by this Movie Scene."), OutError))
	{
		return false;
	}

	OutPlan.Sources.Reserve(OutPlan.Selection.Sources.Num());
	for (const FAttachZeroSource& Source : OutPlan.Selection.Sources)
	{
		AActor* SourceActor = Source.Actor.Get();
		USceneComponent* SourceRoot = Source.RootComponent.Get();
		FMovieSceneBinding* SourceBinding = MovieScene->FindBinding(Source.ActorBinding);
		if (!IsValid(SourceActor) || !IsValid(SourceRoot) || SourceActor->GetRootComponent() != SourceRoot || Source.ActorBinding == OutPlan.Selection.ParentActorBinding || SourceActor == ParentActor || ParentActor->IsAttachedTo(SourceActor) || (SelectedParentComponent != nullptr && SelectedParentComponent->IsAttachedTo(SourceRoot)) || SourceBinding == nullptr)
		{
			OutError = LOCTEXT("AttachZeroActorHierarchyCycle", "Cannot attach all selected FMOD sources because the operation would create an attachment cycle.");
			return false;
		}
		if (!ValidateAttachCycleImpl(MovieScene, Source.ActorBinding, OutPlan.Selection.ParentActorBinding, OutError))
		{
			OutError = LOCTEXT("AttachZeroCycle", "Cannot attach all selected FMOD sources because the operation would create an attachment cycle.");
			return false;
		}

		FAttachZeroSourcePlan& SourcePlan = OutPlan.Sources.AddDefaulted_GetRef();
		SourcePlan.Source = Source;
		SourcePlan.InitialRelativeLocation = SourceRoot->GetRelativeLocation();
		SourcePlan.InitialRelativeRotation = SourceRoot->GetRelativeRotation();
		SourcePlan.InitialRelativeScale = SourceRoot->GetRelativeScale3D();
		for (UMovieSceneTrack* Track : SourceBinding->GetTracks())
		{
			if (Cast<UMovieScene3DTransformTrack>(Track) != nullptr) SourcePlan.TransformTracksToRemove.Add(Track);
		}
		SourcePlan.ExistingAttachTrack = MovieScene->FindTrack<UMovieScene3DAttachTrack>(Source.ActorBinding);
	}
	return true;
}

void RollbackAttachZeroBatchPlanImpl(UMovieScene& MovieScene, FAttachZeroBatchPlan& Plan)
{
	for (FAttachZeroSourcePlan& SourcePlan : Plan.Sources)
	{
		if (SourcePlan.CreatedAttachTrack != nullptr) MovieScene.RemoveTrack(*SourcePlan.CreatedAttachTrack);
		else if (SourcePlan.ExistingAttachTrack != nullptr)
		{
			if (SourcePlan.CreatedAttachSection != nullptr) SourcePlan.ExistingAttachTrack->RemoveSection(*SourcePlan.CreatedAttachSection);
		}
		if (SourcePlan.CreatedTransformTrack != nullptr) MovieScene.RemoveTrack(*SourcePlan.CreatedTransformTrack);
		for (UMovieSceneTrack* TransformTrack : SourcePlan.TransformTracksToRemove)
		{
			MovieScene.AddGivenTrack(TransformTrack, SourcePlan.Source.ActorBinding);
		}
		if (USceneComponent* SourceRoot = SourcePlan.Source.RootComponent.Get())
		{
			SourceRoot->SetRelativeLocation(SourcePlan.InitialRelativeLocation);
			SourceRoot->SetRelativeRotation(SourcePlan.InitialRelativeRotation);
			SourceRoot->SetRelativeScale3D(SourcePlan.InitialRelativeScale);
		}
	}
}

bool ApplyAttachZeroBatchPlanImpl(UMovieScene& MovieScene, FAttachZeroBatchPlan& Plan)
{
	for (FAttachZeroSourcePlan& SourcePlan : Plan.Sources)
	{
		for (UMovieSceneTrack* TransformTrack : SourcePlan.TransformTracksToRemove)
		{
			TransformTrack->Modify();
			MovieScene.RemoveTrack(*TransformTrack);
		}
		UMovieScene3DTransformTrack* TransformTrack = MovieScene.AddTrack<UMovieScene3DTransformTrack>(SourcePlan.Source.ActorBinding);
		if (TransformTrack == nullptr) return false;
		SourcePlan.CreatedTransformTrack = TransformTrack;
		TransformTrack->Modify();
		UMovieSceneSection* TransformSection = TransformTrack->CreateNewSection();
		if (TransformSection == nullptr) return false;
		TransformSection->Modify();
		TransformSection->SetRange(TRange<FFrameNumber>::All());
		TransformTrack->AddSection(*TransformSection);

		UMovieScene3DAttachTrack* AttachTrack = SourcePlan.ExistingAttachTrack;
		if (AttachTrack == nullptr)
		{
			AttachTrack = MovieScene.AddTrack<UMovieScene3DAttachTrack>(SourcePlan.Source.ActorBinding);
			if (AttachTrack == nullptr) return false;
			SourcePlan.CreatedAttachTrack = AttachTrack;
		}
		AttachTrack->Modify();
		UMovieScene3DAttachSection* AttachSection = Cast<UMovieScene3DAttachSection>(AttachTrack->CreateNewSection());
		if (AttachSection == nullptr) return false;
		SourcePlan.CreatedAttachSection = AttachSection;
		AttachSection->Modify();
		AttachSection->SetRange(TRange<FFrameNumber>::All());
		AttachSection->SetConstraintBindingID(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(Plan.Selection.ParentActorBinding)));
		AttachSection->AttachComponentName = Plan.Selection.ParentComponentName;
		AttachSection->AttachSocketName = Plan.Selection.ParentSocketName;
		AttachTrack->AddSection(*AttachSection);
	}
	for (FAttachZeroSourcePlan& SourcePlan : Plan.Sources)
	{
		AActor* SourceActor = SourcePlan.Source.Actor.Get();
		USceneComponent* SourceRoot = SourcePlan.Source.RootComponent.Get();
		SourceActor->Modify();
		SourceRoot->Modify();
		SourceRoot->SetRelativeLocation(FVector::ZeroVector);
		SourceRoot->SetRelativeRotation(FRotator::ZeroRotator);
		SourceRoot->SetRelativeScale3D(FVector::OneVector);
	}
	return true;
}

bool AttachZeroSelectedFMODToSelectedBindingImpl(FText& OutError)
{
	TSharedPtr<ISequencer> Sequencer;
	if (!GetFocusedSequencer(Sequencer, OutError)) return false;
	FAttachZeroBatchPlan Plan;
	if (!BuildAttachZeroBatchPlanImpl(*Sequencer, Plan, OutError))
	{
		if (!OutError.IsEmpty() && !Plan.bErrorAlreadyStatesNoChangesMade) AppendAttachZeroNoChangesMadeImpl(OutError);
		return false;
	}
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence() ? Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
	if (MovieScene == nullptr)
	{
		OutError = LOCTEXT("AttachZeroFocusedSequenceUnavailable", "The focused sequence or its Movie Scene is unavailable.");
		AppendAttachZeroNoChangesMadeImpl(OutError);
		return false;
	}
	FScopedTransaction Transaction(LOCTEXT("AttachZeroTransaction", "Attach Selected FMOD Sources at Zero"));
	MovieScene->Modify();
	if (!ApplyAttachZeroBatchPlanImpl(*MovieScene, Plan))
	{
		RollbackAttachZeroBatchPlanImpl(*MovieScene, Plan);
		Transaction.Cancel();
		OutError = LOCTEXT("AttachZeroCommitFailed", "Could not apply the Attach Zero batch. All changes were rolled back.");
		return false;
	}
	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	return true;
}
}

FString UFMODSequencerToolsEditorSubsystem::GetLastFMODFolderPath() const
{
	FString NormalizedPath;
	FText IgnoredError;
	const UFMODSequencerToolsEditorUserSettings* UserSettings = GetDefault<UFMODSequencerToolsEditorUserSettings>();
	if (UserSettings != nullptr && FMODSequencerToolsEditorPrivate::NormalizeContentBrowserFolderPath(UserSettings->LastFolderPath, NormalizedPath, IgnoredError))
	{
		return NormalizedPath;
	}
	return FMODSequencerToolsEditorPrivate::DefaultFMODFolderPath;
}

FString UFMODSequencerToolsEditorSubsystem::GetCompactFMODFolderDisplayPath(const FString& FullFolderPath, int32 MaxSegments) const
{
	FString NormalizedPath = FullFolderPath;
	NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	TArray<FString> Segments;
	NormalizedPath.ParseIntoArray(Segments, TEXT("/"), true);
	if (Segments.IsEmpty())
	{
		return FString();
	}

	const int32 SegmentLimit = FMath::Max(1, MaxSegments);
	const int32 FirstSegmentIndex = FMath::Max(0, Segments.Num() - SegmentLimit);
	FString CompactPath = TEXT("/");
	for (int32 SegmentIndex = FirstSegmentIndex; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		if (SegmentIndex > FirstSegmentIndex)
		{
			CompactPath += TEXT("/");
		}
		CompactPath += Segments[SegmentIndex];
	}
	return CompactPath;
}

void UFMODSequencerToolsEditorSubsystem::FilterFMODEvents(const TArray<UFMODEvent*>& SourceEvents, const FString& SearchText, TArray<UFMODEvent*>& OutFilteredEvents) const
{
	OutFilteredEvents.Reset();

	TArray<FString> SearchTokens;
	FMODSequencerToolsEditorPrivate::NormalizeFMODEventSearchText(SearchText).ParseIntoArrayWS(SearchTokens);

	TSet<UFMODEvent*> SeenEvents;
	for (UFMODEvent* Event : SourceEvents)
	{
		if (!IsValid(Event) || SeenEvents.Contains(Event))
		{
			continue;
		}
		SeenEvents.Add(Event);

		const FString EventName = Event->GetName();
		const bool bMatchesAllTokens = SearchTokens.ContainsByPredicate([&EventName](const FString& Token)
		{
			return !EventName.Contains(Token, ESearchCase::IgnoreCase);
		}) == false;
		if (bMatchesAllTokens)
		{
			OutFilteredEvents.Add(Event);
		}
	}
}

bool UFMODSequencerToolsEditorSubsystem::LoadFMODEventsFromFolder(const FString& FolderPath, TArray<UFMODEvent*>& OutEvents, FString& OutNormalizedFolderPath, FText& OutError)
{
	OutEvents.Reset();
	OutNormalizedFolderPath.Reset();
	OutError = FText::GetEmpty();
	if (!FMODSequencerToolsEditorPrivate::NormalizeContentBrowserFolderPath(FolderPath, OutNormalizedFolderPath, OutError)) return false;
	FMODSequencerToolsEditorPrivate::QueryFMODEventsInFolder(OutNormalizedFolderPath, OutEvents);
	FMODSequencerToolsEditorPrivate::SaveLastFolderPath(OutNormalizedFolderPath);
	return true;
}

bool UFMODSequencerToolsEditorSubsystem::LoadFMODEventsFromSelectedContentBrowserFolder(TArray<UFMODEvent*>& OutEvents, FString& OutFolderPath, FText& OutError)
{
	OutEvents.Reset();
	OutFolderPath.Reset();
	OutError = FText::GetEmpty();
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FString> SelectedPaths;
	ContentBrowserModule.Get().GetSelectedFolders(SelectedPaths);
	TArray<FString> SelectedPathViewFolders;
	ContentBrowserModule.Get().GetSelectedPathViewFolders(SelectedPathViewFolders);
	SelectedPaths.Append(SelectedPathViewFolders);

	TSet<FString> UniquePackagePaths;
	for (const FString& SelectedPath : SelectedPaths)
	{
		FString PackagePath;
		FText NormalizeError;
		if (!FMODSequencerToolsEditorPrivate::NormalizeContentBrowserFolderPath(SelectedPath, PackagePath, NormalizeError))
		{
			OutError = LOCTEXT("SelectedPathConversionFailed", "The selected Content Browser path could not be converted to an internal package path.");
			return false;
		}
		UniquePackagePaths.Add(MoveTemp(PackagePath));
	}
	if (UniquePackagePaths.Num() != 1)
	{
		OutError = LOCTEXT("SelectExactlyOneFolder", "Select exactly one Content Browser folder.");
		return false;
	}
	return LoadFMODEventsFromFolder(*UniquePackagePaths.CreateConstIterator(), OutEvents, OutFolderPath, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AddNewToFocusedSequencer(FText& OutError)
{
	return AddNewToFocusedSequencerAtDisplayedTime(false, 0.0, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AddNewToFocusedSequencerAtDisplayedTime(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	OutError = FText::GetEmpty();

	UFMODEvent* SelectedEvent = nullptr;
	if (!FMODSequencerToolsEditorPrivate::GetSelectedEvent(SelectedEvent, OutError)) return false;
	return FMODSequencerToolsEditorPrivate::AddNewToFocusedSequencerWithExplicitEventImpl(SelectedEvent, bUseCustomTime, CustomDisplayedTime, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AddNewToFocusedSequencerAtDisplayedTimeWithEvent(UFMODEvent* Event, bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!IsValid(Event))
	{
		OutError = LOCTEXT("ExplicitEventInvalid", "The selected FMOD Event is invalid.");
		return false;
	}
	return FMODSequencerToolsEditorPrivate::AddNewToFocusedSequencerWithExplicitEventImpl(Event, bUseCustomTime, CustomDisplayedTime, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AddToSelectedAtDisplayedTime(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	OutError = FText::GetEmpty();
	UFMODEvent* SelectedEvent = nullptr;
	if (!FMODSequencerToolsEditorPrivate::GetSelectedEvent(SelectedEvent, OutError)) return false;
	return FMODSequencerToolsEditorPrivate::AddToSelectedWithExplicitEventImpl(SelectedEvent, bUseCustomTime, CustomDisplayedTime, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AddToSelectedAtDisplayedTimeWithEvent(UFMODEvent* Event, bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	UE_LOG(LogTemp, Display, TEXT("[FMOD AddSelected] Entry"));
	OutError = FText::GetEmpty();
	if (!IsValid(Event))
	{
		OutError = LOCTEXT("ExplicitEventInvalid", "The selected FMOD Event is invalid.");
		return false;
	}
	return FMODSequencerToolsEditorPrivate::AddToSelectedWithExplicitEventImpl(Event, bUseCustomTime, CustomDisplayedTime, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AddStopKeyToSelectedAtDisplayedTime(bool bUseCustomTime, double CustomDisplayedTime, FText& OutError)
{
	return FMODSequencerToolsEditorPrivate::AddStopKeyToSelectedAtDisplayedTimeImpl(bUseCustomTime, CustomDisplayedTime, OutError);
}

bool UFMODSequencerToolsEditorSubsystem::AttachZeroSelectedFMODToSelectedBinding(FText& OutError)
{
	OutError = FText::GetEmpty();
	return FMODSequencerToolsEditorPrivate::AttachZeroSelectedFMODToSelectedBindingImpl(OutError);
}

#undef LOCTEXT_NAMESPACE
