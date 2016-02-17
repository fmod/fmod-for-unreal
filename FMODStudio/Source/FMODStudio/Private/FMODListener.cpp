// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#include "FMODStudioPrivatePCH.h"
#include "FMODListener.h"
#include <string.h>

float FFMODListener::Interpolate( const double EndTime )
{
	if( FApp::GetCurrentTime() < InteriorStartTime )
	{
		return( 0.0f );
	}

	if( FApp::GetCurrentTime() >= EndTime )
	{
		return( 1.0f );
	}

	float InterpValue = ( float )( ( FApp::GetCurrentTime() - InteriorStartTime ) / ( EndTime - InteriorStartTime ) );
	return( InterpValue );
}

void FFMODListener::UpdateCurrentInteriorSettings()
{
	// Store the interpolation value, not the actual value
	InteriorVolumeInterp = Interpolate( InteriorEndTime );
	ExteriorVolumeInterp = Interpolate( ExteriorEndTime );
	InteriorLPFInterp = Interpolate( InteriorLPFEndTime );
	ExteriorLPFInterp = Interpolate( ExteriorLPFEndTime );
}

void FFMODListener::ApplyInteriorSettings( class AAudioVolume* InVolume, const FInteriorSettings& Settings )
{
	// Note: FInteriorSettings operator!= is not exported, so just do a memcmp instead
	if( InVolume != Volume || (0 != memcmp(&Settings, &InteriorSettings, sizeof(FInteriorSettings))) )
	{
		// Use previous/ current interpolation time if we're transitioning to the default worldsettings zone.
		InteriorStartTime = FApp::GetCurrentTime();
		InteriorEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.InteriorTime : Settings.InteriorTime);
		ExteriorEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.ExteriorTime : Settings.ExteriorTime);
		InteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.InteriorLPFTime : Settings.InteriorLPFTime);
		ExteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.ExteriorLPFTime : Settings.ExteriorLPFTime);

		Volume = InVolume;
		InteriorSettings = Settings;
	}
}

