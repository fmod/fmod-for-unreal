// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#pragma once

struct FMODInteriorSettings
{
	// Whether these interior settings are the default values for the world
	uint32 bIsWorldSettings : 1;

	// The desired volume of sounds outside the volume when the player is inside the volume
	float ExteriorVolume;

	// The time over which to interpolate from the current volume to the desired volume of sounds outside the volume when the player enters the volume
	float ExteriorTime;

	// The desired LPF frequency cutoff in hertz of sounds outside the volume when the player is inside the volume
	float ExteriorLPF;

	// The time over which to interpolate from the current LPF to the desired LPF of sounds outside the volume when the player enters the volume
	float ExteriorLPFTime;

	// The desired volume of sounds inside the volume when the player is outside the volume
	float InteriorVolume;

	// The time over which to interpolate from the current volume to the desired volume of sounds inside the volume when the player enters the volume
	float InteriorTime;

	// The desired LPF frequency cutoff in hertz of sounds inside  the volume when the player is outside the volume
	float InteriorLPF;

	// The time over which to interpolate from the current LPF to the desired LPF of sounds inside the volume when the player enters the volume
	float InteriorLPFTime;

	FMODInteriorSettings::FMODInteriorSettings()
		: bIsWorldSettings(false)
		, ExteriorVolume(1.0f)
		, ExteriorTime(0.5f)
		, ExteriorLPF(MAX_FILTER_FREQUENCY)
		, ExteriorLPFTime(0.5f)
		, InteriorVolume(1.0f)
		, InteriorTime(0.5f)
		, InteriorLPF(MAX_FILTER_FREQUENCY)
		, InteriorLPFTime(0.5f)
	{
	}

	bool operator ==(const FInteriorSettings& Other) const
	{
		return (this->bIsWorldSettings == Other.bIsWorldSettings)
			&& (this->ExteriorVolume == Other.ExteriorVolume)
			&& (this->ExteriorTime == Other.ExteriorTime)
			&& (this->ExteriorLPF == Other.ExteriorLPF)
			&& (this->ExteriorLPFTime == Other.ExteriorLPFTime)
			&& (this->InteriorVolume == Other.InteriorVolume)
			&& (this->InteriorTime == Other.InteriorTime)
			&& (this->InteriorLPF == Other.InteriorLPF)
			&& (this->InteriorLPFTime == Other.InteriorLPFTime);
	}
	bool operator !=(const FInteriorSettings& Other) const
	{
		return !(*this == Other);
	}

	FMODInteriorSettings& operator =(FInteriorSettings Other)
	{
		bIsWorldSettings = Other.bIsWorldSettings;
		ExteriorVolume = Other.ExteriorVolume;
		ExteriorTime = Other.ExteriorTime;
		ExteriorLPF = Other.ExteriorLPF;
		ExteriorLPFTime = Other.ExteriorLPFTime;
		InteriorVolume = Other.InteriorVolume;
		InteriorTime = Other.InteriorTime;
		InteriorLPF = Other.InteriorLPF;
		InteriorLPFTime = Other.InteriorLPFTime;
		return *this;
	}
};

/** A direct copy of FListener (which doesn't have external linkage, unfortunately) **/
struct FFMODListener
{
	FTransform Transform;
	FVector Velocity;

	struct FMODInteriorSettings InteriorSettings;
	/** The volume the listener resides in */
	class AAudioVolume* Volume;

	/** The times of interior volumes fading in and out */
	double InteriorStartTime;
	double InteriorEndTime;
	double ExteriorEndTime;
	double InteriorLPFEndTime;
	double ExteriorLPFEndTime;
	float InteriorVolumeInterp;
	float InteriorLPFInterp;
	float ExteriorVolumeInterp;
	float ExteriorLPFInterp;

	FVector GetUp() const		{ return Transform.GetUnitAxis(EAxis::Z); }
	FVector GetFront() const	{ return Transform.GetUnitAxis(EAxis::Y); }
	FVector GetRight() const	{ return Transform.GetUnitAxis(EAxis::X); }

	/**
	 * Works out the interp value between source and end
	 */
	float Interpolate( const double EndTime );

	/**
	 * Gets the current state of the interior settings for the listener
	 */
	void UpdateCurrentInteriorSettings();

	/** 
	 * Apply the interior settings to ambient sounds
	 */
	void ApplyInteriorSettings( class AAudioVolume* Volume, const FInteriorSettings& Settings );

	FFMODListener()
		: Transform(FTransform::Identity)
		, Velocity(ForceInit)
		, Volume(NULL)
		, InteriorStartTime(0.0)
		, InteriorEndTime(0.0)
		, ExteriorEndTime(0.0)
		, InteriorLPFEndTime(0.0)
		, ExteriorLPFEndTime(0.0)
		, InteriorVolumeInterp(0.f)
		, InteriorLPFInterp(0.f)
		, ExteriorVolumeInterp(0.f)
		, ExteriorLPFInterp(0.f)
	{
	}
};
