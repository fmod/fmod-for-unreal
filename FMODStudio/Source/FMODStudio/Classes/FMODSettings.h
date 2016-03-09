// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

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
	 * Whether to enable vol0virtual, which means voices with low volume will automatically go virtual to save CPU.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	bool bVol0Virtual;

	/**
	 * If vol0virtual is enabled, the signal level at which to make channels virtual.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	float Vol0VirtualLevel;

	/**
	 * Sample rate to use, or 0 to match system rate.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	int32 SampleRate;


	/**
	* Whether to match hardware sample rate where reasonable (44.1kHz to 48kHz).
	*/
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	bool bMatchHardwareSampleRate;

	/**
	 * Number of actual software voices that can be used at once.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	int32 RealChannelCount;

	/**
	 * Total number of voices available that can be either real or virtual.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	int32 TotalChannelCount;

	/**
	 * DSP mixer buffer length, or 0 for system default.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	int32 DSPBufferLength;

	/**
	 * DSP mixer buffer count, or 0 for system default.
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	int32 DSPBufferCount;

	/**
	 * Studio update period in milliseconds, or 0 for default (which means 20ms).
	 */
	UPROPERTY(config, EditAnywhere, Category = InitSettings)
	int32 StudioUpdatePeriod;

	/**
	 * Live update port to use, or 0 for default.
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	int32 LiveUpdatePort;

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
	 * Force platform directory name, or leave empty for automatic (Desktop/Mobile/PS4/XBoxOne)
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	FString ForcePlatformName;

	/**
	 * Name of master bank.  The default in Studio is "Master Bank".
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	FString MasterBankName;

	/**
	 * Skip bank files of the given name.
	 * Can be used to load all banks except for a certain set, such as localization banks.
	 */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	FString SkipLoadBankName;

	/** Is the bank path set up . */
	bool IsBankPathSet() const { return !BankOutputDirectory.Path.IsEmpty(); }

	/** Get the full bank path.  Uses the game's content directory as a base. */
	FString GetFullBankPath() const;

	/** Get the master bank path. */
	FString GetMasterBankPath() const;

	/** Get the master strings bank path. */
	FString GetMasterStringsBankPath() const;

	/** Get all banks in our bank directory excluding the master and strings bank. */
	void GetAllBankPaths(TArray<FString>& Paths, bool IncludeMasterBank=false) const;
};
