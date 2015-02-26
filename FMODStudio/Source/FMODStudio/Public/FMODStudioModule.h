// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#pragma once

#include "ModuleManager.h"

namespace FMOD
{
	namespace Studio
	{
		class System;
		class EventDescription;
		class EventInstance;
	}
}

class UFMODAsset;
class UFMODEvent;

// Which FMOD Studio system to use
namespace EFMODSystemContext
{
	enum Type
	{
		// For use auditioning sounds within the editor
		Auditioning,

		// For use in PIE and in-game
		Runtime,

		// Max number of types
		Max
	};
}

/**
 * The public interface to this module
 */
class IFMODStudioModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IFMODStudioModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IFMODStudioModule >( "FMODStudio" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "FMODStudio" );
	}

	/**
	 * Get a pointer to the runtime studio system (only valid in-game or in PIE)
	 */
	virtual FMOD::Studio::System* GetStudioSystem(EFMODSystemContext::Type Context) = 0;

	/**
	 * Set system paused (for PIE pause)
	 */
	virtual void SetSystemPaused(bool paused) = 0;

	/**
	 * Called when user changes any studio settings
	 */
	virtual void RefreshSettings() = 0;

	/**
	 * Called when we enter of leave PIE mode
	 */
	virtual void SetInPIE(bool bInPIE, bool bSimulating) = 0;

	/**
	 * Look up an asset given its name
	 */
	virtual UFMODAsset* FindAssetByName(const FString& Name) = 0;

	/**
	 * Look up an event given its name
	 */
	virtual UFMODEvent* FindEventByName(const FString& Name) = 0;

	/**
	 * Get an event description.
	 * The system type can control which Studio system to use, or leave it as System_Max for it to choose automatically.
	 */
	virtual FMOD::Studio::EventDescription* GetEventDescription(const UFMODEvent* Event, EFMODSystemContext::Type Context = EFMODSystemContext::Max) = 0;

	/**
	 * Create a single auditioning instance using the auditioning system
	 */
	virtual FMOD::Studio::EventInstance* CreateAuditioningInstance(const UFMODEvent* Event) = 0;

	/**
	 * Stop any auditioning instance
	 */
	virtual void StopAuditioningInstance() = 0;

	/** This event is fired after all banks were reloaded */
	virtual FSimpleMulticastDelegate& BanksReloadedEvent() = 0;

	/** Returns whether sound is enabled for the game */
	virtual bool UseSound() = 0;

};
