#ifndef _OCULUSSPATIALIZERSETTINGS_H_
#define _OCULUSSPATIALIZERSETTINGS_H_


#ifndef OVRA_EXPORT
#  ifdef _WIN32
#    define OVRA_EXPORT __declspec( dllexport )
#  elif defined __APPLE__ 
#    define OVRA_EXPORT 
#  elif defined __linux__
#    define OVRA_EXPORT __attribute__((visibility("default")))
#  else
#    error not implemented
#  endif
#endif

extern "C"
{

OVRA_EXPORT bool OSP_FMOD_Initialize(int SampleRate, int BufferLength);
OVRA_EXPORT bool OSP_FMOD_SetEarlyReflectionsEnabled(int Enabled);
OVRA_EXPORT bool OSP_FMOD_SetLateReverberationEnabled(int Enabled);
OVRA_EXPORT bool OSP_FMOD_SetGlobalScale(float Scale);
OVRA_EXPORT bool OSP_FMOD_SetBypass(int Enabled);
OVRA_EXPORT bool OSP_FMOD_SetGain(float gain_dB);
//
// Set room parameters
// Size: in meters, default is  8.0 x 2.5 x 5.0
// Reflections: (0.0 = fully anechoic, 1.0 = fully reflective, but we cap at 0.95)
//
OVRA_EXPORT bool OSP_FMOD_SetSimpleBoxRoomParameters(
    float Width, float Height, float Depth,
    float RefLeft, float RefRight,
    float RefUp, float RefDown,
    float RefNear, float RefFar);
};


#endif // _OCULUSSPATIALIZERSETTINGS_H_