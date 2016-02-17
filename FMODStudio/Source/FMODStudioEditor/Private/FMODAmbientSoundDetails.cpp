// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioEditorPrivatePCH.h"
#include "FMODAmbientSoundDetails.h"
#include "Toolkits/AssetEditorManager.h"
#include "FMODAmbientSound.h"
#include "FMODStudioModule.h"
#include "FMODEvent.h"
#include "fmod_studio.hpp"

#define LOCTEXT_NAMESPACE "FMODStudio"

TSharedRef<IDetailCustomization> FFMODAmbientSoundDetails::MakeInstance()
{
	return MakeShareable( new FFMODAmbientSoundDetails );
}

void FFMODAmbientSoundDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();

	for( int32 ObjectIndex = 0; !AmbientSound.IsValid() && ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			AmbientSound = Cast<AFMODAmbientSound>(CurrentObject.Get());
		}
	}

	DetailBuilder.EditCategory(TEXT("Sound"))
		.AddCustomRow(FText::GetEmpty())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding( 0, 2.0f, 0, 0 )
			.FillHeight(1.0f)
			.VAlign( VAlign_Center )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 2.0f, 0.0f )
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked( this, &FFMODAmbientSoundDetails::OnEditSoundClicked )
						.Text( LOCTEXT("EditAsset", "Edit") )
						.ToolTipText( LOCTEXT("EditAssetToolTip", "Edit this sound cue") )
					]
				+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 2.0f, 0.0f )
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked( this, &FFMODAmbientSoundDetails::OnPlaySoundClicked )
						.Text( LOCTEXT("PlaySoundCue", "Play") )
					]
				+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 2.0f, 0.0f )
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked( this, &FFMODAmbientSoundDetails::OnStopSoundClicked )
						.Text( LOCTEXT("StopSoundCue", "Stop") )
					]
			]
		];
}


FReply FFMODAmbientSoundDetails::OnEditSoundClicked()
{
	if( AmbientSound.IsValid() )
	{
		UFMODEvent* Event = AmbientSound.Get()->AudioComponent->Event.Get();
		if (Event)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Event);
		}
	}

	return FReply::Handled();
}

FReply FFMODAmbientSoundDetails::OnPlaySoundClicked()
{
	if( AmbientSound.IsValid() )
	{
		UFMODEvent* Event = AmbientSound.Get()->AudioComponent->Event.Get();
		if (Event)
		{
			FMOD::Studio::EventInstance* Instance = IFMODStudioModule::Get().CreateAuditioningInstance(Event);
			if (Instance)
			{
				Instance->start();
			}
		}
	}

	return FReply::Handled();
}

FReply FFMODAmbientSoundDetails::OnStopSoundClicked()
{
	IFMODStudioModule::Get().StopAuditioningInstance();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
