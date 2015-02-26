/*
    fmod_studio.hpp - FMOD Studio API
    Copyright (c), Firelight Technologies Pty, Ltd. 2015.

    This header defines the C++ API. If you are programming in C use fmod_studio.h.
*/

#ifndef FMOD_STUDIO_HPP
#define FMOD_STUDIO_HPP

#include "fmod_studio_common.h"
#include "fmod_studio.h"

#include "fmod.hpp"

namespace FMOD
{

namespace Studio
{
    typedef FMOD_GUID ID;

    class System;

    class Bank;

    class Bus;
    class VCA;
    class EventDescription;

    class EventInstance;
    class ParameterInstance;
    class CueInstance;

    FMOD_RESULT F_API parseID(const char *idString, ID *id);

    class System
    {
    private:
        System();   // Constructor made private so user cannot statically instance a System class. System::create must be used.

    public:
        static FMOD_RESULT F_API create(System **system, unsigned int headerVersion = FMOD_VERSION);
        FMOD_RESULT F_API setAdvancedSettings(FMOD_STUDIO_ADVANCEDSETTINGS *settings);
        FMOD_RESULT F_API getAdvancedSettings(FMOD_STUDIO_ADVANCEDSETTINGS *settings);
        FMOD_RESULT F_API initialize(int maxchannels, FMOD_STUDIO_INITFLAGS studioflags, FMOD_INITFLAGS flags, void *extradriverdata);
        FMOD_RESULT F_API release();

        // Handle validity
        bool F_API isValid() const;

        // Update processing
        FMOD_RESULT F_API update();

        // Low-level API access
        FMOD_RESULT F_API getLowLevelSystem(FMOD::System **system) const;

        // Asset retrieval
        // These functions load data from registered banks if necessary, unless mode is set to FMOD_STUDIO_LOAD_PROHIBITED.
        FMOD_RESULT F_API getEvent(const char *path, EventDescription **event) const;
        FMOD_RESULT F_API getBus(const char *path, Bus **bus) const;
        FMOD_RESULT F_API getVCA(const char *path, VCA **vca) const;
        FMOD_RESULT F_API getBank(const char *path, Bank **bank) const;
        FMOD_RESULT F_API getEventByID(const ID *id, EventDescription **event) const;
        FMOD_RESULT F_API getBusByID(const ID *id, Bus **bus) const;
        FMOD_RESULT F_API getVCAByID(const ID *id, VCA **vca) const;
        FMOD_RESULT F_API getBankByID(const ID *id, Bank **bank) const;
        FMOD_RESULT F_API getSoundInfo(const char *key, FMOD_STUDIO_SOUND_INFO *info) const;

        // Path lookup
        FMOD_RESULT F_API lookupID(const char *path, ID *id) const;
        FMOD_RESULT F_API lookupPath(const ID *id, char *path, int size, int *retrieved) const;

        // Listener control
        FMOD_RESULT F_API getListenerAttributes(FMOD_3D_ATTRIBUTES *attributes) const;
        FMOD_RESULT F_API setListenerAttributes(const FMOD_3D_ATTRIBUTES *attributes);

        // Bank control
        FMOD_RESULT F_API loadBankFile(const char *filename, FMOD_STUDIO_LOAD_BANK_FLAGS flags, Bank **bank);
        FMOD_RESULT F_API loadBankMemory(const char *buffer, int length, FMOD_STUDIO_LOAD_MEMORY_MODE mode, FMOD_STUDIO_LOAD_BANK_FLAGS flags, Bank **bank);
        FMOD_RESULT F_API loadBankCustom(const FMOD_STUDIO_BANK_INFO *info, FMOD_STUDIO_LOAD_BANK_FLAGS flags, Bank **bank);
        FMOD_RESULT F_API unloadAll();

        // General functionality
        FMOD_RESULT F_API getCPUUsage(FMOD_STUDIO_CPU_USAGE *usage) const;
        FMOD_RESULT F_API getBufferUsage(FMOD_STUDIO_BUFFER_USAGE *usage) const;
        FMOD_RESULT F_API resetBufferUsage();
        FMOD_RESULT F_API registerPlugin(const FMOD_DSP_DESCRIPTION *description);
        FMOD_RESULT F_API unregisterPlugin(const char *name);

        // Enumeration
        FMOD_RESULT F_API getBankCount(int *count) const;
        FMOD_RESULT F_API getBankList(Bank **array, int capacity, int *count) const;

        // Recording and playback
        FMOD_RESULT F_API flushCommands();
        FMOD_RESULT F_API startRecordCommands(const char *filename, FMOD_STUDIO_RECORD_COMMANDS_FLAGS flags);
        FMOD_RESULT F_API stopRecordCommands();
        FMOD_RESULT F_API playbackCommands(const char *filename);

        // Callbacks
        FMOD_RESULT F_API setCallback(FMOD_STUDIO_SYSTEM_CALLBACK callback, FMOD_STUDIO_SYSTEM_CALLBACK_TYPE callbackmask = 0xFFFFFFFF);
        FMOD_RESULT F_API getUserData(void **userData) const;
        FMOD_RESULT F_API setUserData(void *userData);

    };

    class EventDescription
    {
    private:
        EventDescription();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        // Property access
        FMOD_RESULT F_API getID(ID *id) const;
        FMOD_RESULT F_API getPath(char *path, int size, int *retrieved) const;
        FMOD_RESULT F_API getParameterCount(int *count) const;
        FMOD_RESULT F_API getParameterByIndex(int index, FMOD_STUDIO_PARAMETER_DESCRIPTION *parameter) const;
        FMOD_RESULT F_API getParameter(const char *name, FMOD_STUDIO_PARAMETER_DESCRIPTION *parameter) const;
        FMOD_RESULT F_API getUserPropertyCount(int *count) const;
        FMOD_RESULT F_API getUserPropertyByIndex(int index, FMOD_STUDIO_USER_PROPERTY *property) const;
        FMOD_RESULT F_API getUserProperty(const char *name, FMOD_STUDIO_USER_PROPERTY *property) const;
        FMOD_RESULT F_API getLength(int *length) const;
        FMOD_RESULT F_API getMinimumDistance(float *distance) const;
        FMOD_RESULT F_API getMaximumDistance(float *distance) const;

        FMOD_RESULT F_API isOneshot(bool *oneshot) const;
        FMOD_RESULT F_API isStream(bool *isStream) const;
        FMOD_RESULT F_API is3D(bool *is3D) const;

        // Playback control
        FMOD_RESULT F_API createInstance(EventInstance **instance) const;
        FMOD_RESULT F_API getInstanceCount(int *count) const;
        FMOD_RESULT F_API getInstanceList(EventInstance **array, int capacity, int *count) const;

        // Sample data loading control
        FMOD_RESULT F_API loadSampleData();
        FMOD_RESULT F_API unloadSampleData();
        FMOD_RESULT F_API getSampleLoadingState(FMOD_STUDIO_LOADING_STATE *state) const;

        // Convenience functions
        FMOD_RESULT F_API releaseAllInstances();

        // Callbacks
        FMOD_RESULT F_API setCallback(FMOD_STUDIO_EVENT_CALLBACK callback);
        FMOD_RESULT F_API getUserData(void **userData) const;
        FMOD_RESULT F_API setUserData(void *userData);
    };

    class EventInstance
    {
    private:
        EventInstance();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        // Property access
        FMOD_RESULT F_API getDescription(EventDescription **description) const;

        // Playback control
        FMOD_RESULT F_API getVolume(float *volume) const;
        FMOD_RESULT F_API setVolume(float volume);

        FMOD_RESULT F_API getPitch(float *pitch) const;
        FMOD_RESULT F_API setPitch(float pitch);

        FMOD_RESULT F_API get3DAttributes(FMOD_3D_ATTRIBUTES *attributes) const;
        FMOD_RESULT F_API set3DAttributes(const FMOD_3D_ATTRIBUTES *attributes);

        FMOD_RESULT F_API getProperty(FMOD_STUDIO_EVENT_PROPERTY index, float* value) const;
        FMOD_RESULT F_API setProperty(FMOD_STUDIO_EVENT_PROPERTY index, float value);

        FMOD_RESULT F_API getPaused(bool *paused) const;
        FMOD_RESULT F_API setPaused(bool paused);

        FMOD_RESULT F_API start();
        FMOD_RESULT F_API stop(FMOD_STUDIO_STOP_MODE mode);

        FMOD_RESULT F_API getTimelinePosition(int *position) const;
        FMOD_RESULT F_API setTimelinePosition(int position);

        FMOD_RESULT F_API getPlaybackState(FMOD_STUDIO_PLAYBACK_STATE *state) const;

        FMOD_RESULT F_API getChannelGroup(ChannelGroup **group) const;

        FMOD_RESULT F_API release();

        FMOD_RESULT F_API isVirtual(bool *virtualState) const;

        FMOD_RESULT F_API getParameter(const char *name, ParameterInstance **parameter) const;
        FMOD_RESULT F_API getParameterByIndex(int index, ParameterInstance **parameter) const;
        FMOD_RESULT F_API getParameterCount(int *count) const;

        FMOD_RESULT F_API setParameterValue(const char *name, float value);
        FMOD_RESULT F_API setParameterValueByIndex(int index, float value);

        FMOD_RESULT F_API getCue(const char *name, CueInstance **cue) const;
        FMOD_RESULT F_API getCueByIndex(int index, CueInstance **cue) const;
        FMOD_RESULT F_API getCueCount(int *count) const;

        // Callbacks
        FMOD_RESULT F_API setCallback(FMOD_STUDIO_EVENT_CALLBACK callback);

        FMOD_RESULT F_API getUserData(void **userData) const;
        FMOD_RESULT F_API setUserData(void *userData);
    };

    class CueInstance
    {
    private:
        CueInstance();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        FMOD_RESULT F_API trigger();
    };

    class ParameterInstance
    {
    private:
        ParameterInstance();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        // Property access
        FMOD_RESULT F_API getDescription(FMOD_STUDIO_PARAMETER_DESCRIPTION *description) const;

        // Playback control
        FMOD_RESULT F_API getValue(float *value) const;
        FMOD_RESULT F_API setValue(float value);
    };

    class Bus
    {
    private:
        Bus();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        // Property access
        FMOD_RESULT F_API getID(ID *id) const;
        FMOD_RESULT F_API getPath(char *path, int size, int *retrieved) const;

        // Playback control
        FMOD_RESULT F_API getFaderLevel(float *level) const;
        FMOD_RESULT F_API setFaderLevel(float level);

        FMOD_RESULT F_API getPaused(bool *paused) const;
        FMOD_RESULT F_API setPaused(bool paused);

        FMOD_RESULT F_API getMute(bool *paused) const;
        FMOD_RESULT F_API setMute(bool paused);

        FMOD_RESULT F_API stopAllEvents(FMOD_STUDIO_STOP_MODE mode);

        // Low-level API access
        FMOD_RESULT F_API lockChannelGroup();
        FMOD_RESULT F_API unlockChannelGroup();
        FMOD_RESULT F_API getChannelGroup(FMOD::ChannelGroup **channelgroup) const;
    };

    class VCA
    {
    private:
        VCA();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        // Property access
        FMOD_RESULT F_API getID(ID *id) const;
        FMOD_RESULT F_API getPath(char *path, int size, int *retrieved) const;

        // Playback control
        FMOD_RESULT F_API getFaderLevel(float *level) const;
        FMOD_RESULT F_API setFaderLevel(float level);
    };

    class Bank
    {
    private:
        Bank();   // Constructor made private so user cannot statically instance the class.

    public:
        // Handle validity
        bool F_API isValid() const;

        // Property access
        FMOD_RESULT F_API getID(ID *id) const;
        FMOD_RESULT F_API getPath(char *path, int size, int *retrieved) const;

        // Loading control
        FMOD_RESULT F_API unload();
        FMOD_RESULT F_API loadSampleData();
        FMOD_RESULT F_API unloadSampleData();

        FMOD_RESULT F_API getLoadingState(FMOD_STUDIO_LOADING_STATE *state) const;
        FMOD_RESULT F_API getSampleLoadingState(FMOD_STUDIO_LOADING_STATE *state) const;

        // Enumeration
        FMOD_RESULT F_API getStringCount(int *count) const;
        FMOD_RESULT F_API getStringInfo(int index, ID *id, char *path, int size, int *retrieved) const;
        FMOD_RESULT F_API getEventCount(int *count) const;
        FMOD_RESULT F_API getEventList(EventDescription **array, int capacity, int *count) const;
        FMOD_RESULT F_API getBusCount(int *count) const;
        FMOD_RESULT F_API getBusList(Bus **array, int capacity, int *count) const;
        FMOD_RESULT F_API getVCACount(int *count) const;
        FMOD_RESULT F_API getVCAList(VCA **array, int capacity, int *count) const;
    };

} // namespace Studio

} // namespace FMOD

#endif //FMOD_STUDIO_HPP
