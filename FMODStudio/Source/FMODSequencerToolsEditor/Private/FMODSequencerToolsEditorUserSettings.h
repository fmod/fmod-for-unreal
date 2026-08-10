#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FMODSequencerToolsEditorUserSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class FMODSEQUENCERTOOLSEDITOR_API UFMODSequencerToolsEditorUserSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	FString LastFolderPath = TEXT("/Game/FMOD/Events/SFX/Environment/Cutscenes");
};
