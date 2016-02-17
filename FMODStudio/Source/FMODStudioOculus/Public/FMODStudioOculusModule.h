// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#pragma once

#include "ModuleManager.h"

/**
 * The public interface to this module
 */
class IFMODStudioOculusModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IFMODStudioOculusModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IFMODStudioOculusModule >( "FMODStudioOculus" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "FMODStudioOculus" );
	}

	/** Returns true if Oculus plugin is enabled and has been created successfully */
	virtual bool IsRunning() = 0;

	/** Called from Studio system on initialization */
	virtual void OnInitialize() = 0;
};
