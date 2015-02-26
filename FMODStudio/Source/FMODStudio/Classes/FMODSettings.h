// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#pragma once

#include "FMODSettings.generated.h"

UENUM()
namespace EFMODSpeakerMode
{
	enum Type
	{
		// The speakers are stereo
		Stereo,
		// 5.1 speaker setup
		Surround_5_1,
		// 7.1 speaker setup
		Surround_7_1
	};
}


UCLASS(config = Engine, defaultconfig)
class FMODSTUDIO_API UFMODSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Whether to load all banks at startup.
	 */
	UPROPERTY(config, EditAnywhere, Category = Basic)
	bool bLoadAllBanks;

	/**
	 * Whether to load all bank sample data into memory at startup.
	 */
	UPROPERTY(config, EditAnywhere, Category = Basic)
	bool bLoadAllSampleData;

	/**
	 * Enable live update in non-final builds.
	 */
	UPROPERTY(config, EditAnywhere, Category = Basic)
	bool bEnableLiveUpdate;

	/**
	 * Path to find your studio bank output directory, relative to Content directory.
	 */
    UPROPERTY(config, EditAnywhere, Category = Basic, meta=(RelativeToGameContentDir))
	FDirectoryPath BankOutputDirectory;

	/** Project Output Format, should match the mode set up for the Studio project. */
	UPROPERTY(config, EditAnywhere, Category = Basic)
	TEnumAsByte<EFMODSpeakerMode::Type> OutputFormat;

	/**
	 * Extra plugin files to load.  
	 * The plugin files should sit alongside the FMOD dynamic libraries in the ThirdParty directory.
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	TArray<FString> PluginFiles;

	/**
	 * Directory for content to appear in content window. Be careful changing this!
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	FString ContentBrowserPrefix;

	/**
	 * Name of master bank.  The default in Studio is "Master Bank".
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	FString MasterBankName;

	/** Is the bank path set up . */
	bool IsBankPathSet() const { return !BankOutputDirectory.Path.IsEmpty(); }

	/** Get the full bank path.  Uses the game's content directory as a base. */
	FString GetFullBankPath() const;

	/** Get the master bank path. */
	FString GetMasterBankPath() const;

	/** Get the master strings bank path. */
	FString GetMasterStringsBankPath() const;

	/** Get all banks in our bank directory excluding the master and strings bank. */
	void GetAllBankPaths(TArray<FString>& Paths) const;
};
