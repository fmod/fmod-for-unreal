// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#pragma once

#include "fmod.hpp"

FMOD_RESULT F_CALLBACK FMODLogCallback(FMOD_DEBUG_FLAGS flags, const char *file, int line, const char *func, const char *message);

void AcquireFMODFileSystem();
void ReleaseFMODFileSystem();
void AttachFMODFileSystem(FMOD::System *system);

