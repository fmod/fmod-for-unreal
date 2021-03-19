// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2021.

#pragma once

#include "UObject/Class.h"
#include "Engine/EngineTypes.h"
#include "GenericPlatform/GenericPlatform.h"
#include "FMODSettings.generated.h"

class Paths;

UENUM()
enum EFMODLogging
{
    LEVEL_NONE = 0,
    LEVEL_ERROR = 1,
    LEVEL_WARNING = 2,
    LEVEL_LOG = 4,
    TYPE_MEMORY = 100,
    TYPE_FILE = 200,
    TYPE_CODEC = 400,
    TYPE_TRACE = 800,
    DISPLAY_TIMESTAMPS = 10000,
    DISPLAY_LINENUMBERS = 20000,
    DISPLAY_THREAD = 40000
};

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

USTRUCT()
struct FCustomPoolSizes
{
    GENERATED_USTRUCT_BODY()

    /** Default = 0 (Disabled) units in bytes*/
    UPROPERTY(config, EditAnywhere, Category = InitSettings, meta = (ClampMin = "0"))
    int32 Desktop;
    /** Default = 0 (Disabled) units in bytes*/
    UPROPERTY(config, EditAnywhere, Category = InitSettings, meta = (ClampMin = "0"))
    int32 Mobile;
    /** Default = 0 (Disabled) units in bytes*/
    UPROPERTY(config, EditAnywhere, Category = InitSettings, meta = (ClampMin = "0"))
    int32 PS4;
    /** Default = 0 (Disabled) units in bytes*/
    UPROPERTY(config, EditAnywhere, Category = InitSettings, meta = (ClampMin = "0"))
    int32 Switch;
    /** Default = 0 (Disabled) units in bytes*/
    UPROPERTY(config, EditAnywhere, Category = InitSettings, meta = (ClampMin = "0"))
    int32 XboxOne;

    FCustomPoolSizes()
        : Desktop(0)
        , Mobile(0)
        , PS4(0)
        , Switch(0)
        , XboxOne(0)
    {
    }
};

USTRUCT()
struct FFMODProjectLocale
{
    GENERATED_USTRUCT_BODY()

    /**
    * Human readable locale name, displayed in Blueprints.
    */
    UPROPERTY(config, EditAnywhere, Category = Localization)
    FString LocaleName;

    /**
    * Locale code. Must correspond to project locale codes in FMOD Studio project.
    */
    UPROPERTY(config, EditAnywhere, Category = Localization)
    FString LocaleCode;

    /**
    * Default locale at startup. Only one locale should be marked as default.
    */
    UPROPERTY(config, EditAnywhere, Category = Localization)
    bool bDefault;
};

USTRUCT()
struct FFMODPerPlatformConfig
{
   GENERATED_USTRUCT_BODY()
   /**
    * Sample rate to use, or 0 to match system rate.
   */

   UPROPERTY(config, EditAnywhere, Category = InitSettings)
   int32 SampleRate = 48000;

   /** Project Output Format, should match the mode set up for the Studio project. */
   UPROPERTY(config, EditAnywhere, Category = Basic)
   TEnumAsByte<EFMODSpeakerMode::Type> OutputFormat = EFMODSpeakerMode::Surround_5_1;

   /**
   * Whether to match hardware sample rate where reasonable (44.1kHz to 48kHz).
   */
   UPROPERTY(config, EditAnywhere, Category = InitSettings)
   bool bMatchHardwareSampleRate = true;

   /**
   * DSP mixer buffer length (eg. 512, 1024) or 0 for system default.
   * When changing the Buffer Length, Buffer Count also needs to be set.
   */
   UPROPERTY(config, EditAnywhere, Category = InitSettings)
   int32 DSPBufferLength = 0;

   /**
    * DSP mixer buffer count (eg. 2, 4) or 0 for system default.
    * When changing the Buffer Count, Buffer Length also needs to be set.
    */
   UPROPERTY(config, EditAnywhere, Category = InitSettings)
   int32 DSPBufferCount = 0;
};

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
    * Enable live update in Editor for Auditioning. *Requires Restart*
    */
    UPROPERTY(Config, EditAnywhere, Category = Basic)
    bool bEnableEditorLiveUpdate;

    /**
	 * Path to find your studio bank output directory, relative to Content directory.
	 */
    UPROPERTY(config, EditAnywhere, Category = Basic, meta = (RelativeToGameContentDir))
    FDirectoryPath BankOutputDirectory;

    /**
    * Locales for localized banks. These should match the project locales configured in the FMOD Studio project.
    */
    UPROPERTY(config, EditAnywhere, Category = Localization)
    TArray<FFMODProjectLocale> Locales;

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
	 * Number of actual software voices that can be used at once.
	 */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    int32 RealChannelCount;

    /**
	 * Total number of voices available that can be either real or virtual.
	 */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    int32 TotalChannelCount;

    UPROPERTY(config, EditAnywhere, Category = PerPlatformSettings)
    FFMODPerPlatformConfig DefaultPlatformSettings;
    
    UPROPERTY(config, EditAnywhere, Category = PerPlatformSettings)
    FFMODPerPlatformConfig XB1PlatformSettings;
    
    UPROPERTY(config, EditAnywhere, Category = PerPlatformSettings)
    FFMODPerPlatformConfig PS4PlatformSettings;
    
    UPROPERTY(config, EditAnywhere, Category = PerPlatformSettings)
    FFMODPerPlatformConfig SwitchPlatformSettings;

    /**
	 * File buffer size in bytes (2048 by default).
	 */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    int32 FileBufferSize;

    /**
	 * Studio update period in milliseconds, or 0 for default (which means 20ms).
	 */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    int32 StudioUpdatePeriod;

    /**
	 * Output device to choose at system start up, or empty for default.
	 */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    FString InitialOutputDriverName;

    /**
	 * Lock all mixer buses at startup, making sure they are created up front.
	 */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    bool bLockAllBuses;

    /** 
     * Use specified memory pool size for platform, units in bytes. Disabled by default.
     * FMOD may become unstable if the limit is exceeded!
     */
    UPROPERTY(config, EditAnywhere, Category = InitSettings)
    FCustomPoolSizes MemoryPoolSizes;

    /**
	 * Live update port to use, or 0 for default.
	 */
    UPROPERTY(config, EditAnywhere, Category = Advanced, meta = (EditCondition = "bEnableLiveUpdate"))
    int32 LiveUpdatePort;

    /**
    * Live update port to use while in editor for auditioning. *Requires Restart*
    */
    UPROPERTY(config, EditAnywhere, Category = Advanced, meta = (EditCondition = "bEnableEditorLiveUpdate"))
    int32 EditorLiveUpdatePort;

    /**
    * Delay in seconds before automatically reloading modified banks from disk. This can be extended if building
    * banks takes a long time and UE4 tries to reload banks before building is completed.
    * Set to 0 to disable automatic bank reloading.
    */
    UPROPERTY(config, EditAnywhere, Category = Advanced)
    int32 ReloadBanksDelay;

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

	/*
    * Specify the key for loading sounds from encrypted banks.
	*/
	UPROPERTY(config, EditAnywhere, Category = Advanced, meta = (DisplayName = "Encryption Key"))
	FString StudioBankKey;

    /**
	* Force wav writer output, for debugging only.  Setting this will prevent normal sound output!
	*/
    UPROPERTY(config, EditAnywhere, Category = Advanced)
    FString WavWriterPath;

    UPROPERTY(config, EditAnywhere, Category = Advanced)
    TEnumAsByte<EFMODLogging> LoggingLevel;

    /**
    * Name of the parameter used in Studio to control Occlusion effects.
    */
    UPROPERTY(config, EditAnywhere, Category = Advanced)
    FString OcclusionParameter;

    /**
    * Name of the parameter used in Studio to control Ambient volume.
    */
    UPROPERTY(config, EditAnywhere, Category = Advanced)
    FString AmbientVolumeParameter;

    /**
    * Name of the parameter used in Studio to control Ambient LPF effects.
    */
    UPROPERTY(config, EditAnywhere, Category = Advanced)
    FString AmbientLPFParameter;

    /** Is the bank path set up . */
    bool IsBankPathSet() const { return !BankOutputDirectory.Path.IsEmpty(); }

    /** Get the full bank path.  Uses the game's content directory as a base. */
    FString GetFullBankPath() const;

    /** Get the master bank filename. */
    FString GetMasterBankFilename() const;

    /** Get the master assets bank filename. */
    FString GetMasterAssetsBankFilename() const;

    /** Get the master strings bank filename. */
    FString GetMasterStringsBankFilename() const;

#if WITH_EDITOR
    /** Check the settings for any configuration issues. */
    enum EProblem
    {
        Okay,
        BankPathNotSet,
        AddedToUFS,
        NotPackaged,
        AddedToBoth
    };

    EProblem Check() const;
#endif // WITH_EDITOR
};
