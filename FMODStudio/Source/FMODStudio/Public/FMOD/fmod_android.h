#ifndef _FMOD_ANDROID_H
#define _FMOD_ANDROID_H

/*
[ENUM]
[
    [DESCRIPTION]
    Cores available for mapping threads onto.

    [REMARKS]

    [SEE_ALSO]
    FMOD_ANDROID_THREADAFFINITY
]
*/
typedef enum FMOD_THREAD
{
    FMOD_THREAD_DEFAULT = 0,        /* Use default thread assignment. */
    FMOD_THREAD_CORE0   = 1 << 0,
    FMOD_THREAD_CORE1   = 1 << 1,
    FMOD_THREAD_CORE2   = 1 << 2,
    FMOD_THREAD_CORE3   = 1 << 3,
    FMOD_THREAD_CORE4   = 1 << 4,
    FMOD_THREAD_CORE5   = 1 << 5,
    FMOD_THREAD_CORE6   = 1 << 6,
    FMOD_THREAD_CORE7   = 1 << 7,
} FMOD_THREAD;


/*
[STRUCTURE]
[
    [DESCRIPTION]
    Mapping of cores to threads.

    [REMARKS]

    [SEE_ALSO]
    FMOD_THREAD
    FMOD_Android_SetThreadAffinity
]
*/
typedef struct FMOD_ANDROID_THREADAFFINITY
{
    unsigned int mixer;             /* Software mixer thread. */
    unsigned int stream;            /* Stream thread. */
    unsigned int nonblocking;       /* Asynchronous sound loading thread. */
    unsigned int file;              /* File thread. */
    unsigned int geometry;          /* Geometry processing thread. */
    unsigned int profiler;          /* Profiler threads. */
    unsigned int studioUpdate;      /* Studio update thread. */
    unsigned int studioLoadBank;    /* Studio bank loading thread. */
    unsigned int studioLoadSample;  /* Studio sample loading thread. */
} FMOD_ANDROID_THREADAFFINITY;


/*
[API]
[
    [DESCRIPTION]
    Control which core particular FMOD threads are created on.

    [PARAMETERS]
    'affinity'    Pointer to a structure that describes the affinity for each thread.

    [REMARKS]
    Call before System::init or affinity values will not apply.

    [SEE_ALSO]
    FMOD_ANDROID_THREADAFFINITY
]
*/
extern "C" FMOD_RESULT F_API FMOD_Android_SetThreadAffinity(FMOD_ANDROID_THREADAFFINITY *affinity);

#endif /* _FMOD_ANDROID_H */
