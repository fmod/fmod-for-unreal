// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2016.

#pragma once

#include "fmod.h"

FMOD_RESULT F_CALLBACK FMODLogCallback(FMOD_DEBUG_FLAGS flags, const char *file, int line, const char *func, const char *message);
FMOD_RESULT F_CALLBACK FMODOpen(const char *name, unsigned int *filesize, void **handle, void * /*userdata*/);
FMOD_RESULT F_CALLBACK FMODClose(void *handle, void * /*userdata*/);
FMOD_RESULT F_CALLBACK FMODRead(void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void * /*userdata*/);
FMOD_RESULT F_CALLBACK FMODSeek(void *handle, unsigned int pos, void * /*userdata*/);

