// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2015.

#include "FMODStudioPrivatePCH.h"
#include "FMODFileCallbacks.h"
#include "FMODUtils.h"

FMOD_RESULT F_CALLBACK FMODLogCallback(FMOD_DEBUG_FLAGS flags, const char *file, int line, const char *func, const char *message)
{
	if (flags & (FMOD_DEBUG_LEVEL_WARNING | FMOD_DEBUG_LEVEL_ERROR))
	{
		UE_LOG(LogFMOD, Warning, TEXT("%s(%d) - %s"), UTF8_TO_TCHAR(file), line, UTF8_TO_TCHAR(message));
	}
	else
	{
		UE_LOG(LogFMOD, Verbose, TEXT("%s(%d) - %s"), UTF8_TO_TCHAR(file), line, UTF8_TO_TCHAR(message));
	}
	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMODOpen(const char *name, unsigned int *filesize, void **handle, void * /*userdata*/)
{
	if (name)
	{
		FArchive* Archive = IFileManager::Get().CreateFileReader(UTF8_TO_TCHAR(name));
		UE_LOG(LogFMOD, Verbose, TEXT("FMODOpen Opening '%s' returned archive %p"), UTF8_TO_TCHAR(name), Archive);
		if (!Archive)
		{
			return FMOD_ERR_FILE_NOTFOUND;
		}
		*filesize = Archive->TotalSize();
		*handle = Archive;
		UE_LOG(LogFMOD, Verbose, TEXT("  TotalSize = %d"), *filesize);
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMODClose(void *handle, void * /*userdata*/)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}


	FArchive* Archive = (FArchive*)handle;
	UE_LOG(LogFMOD, Verbose, TEXT("FMODClose Closing archive %p"), Archive);
	delete Archive;

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMODRead(void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void * /*userdata*/)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	if (bytesread)
	{
		FArchive* Archive = (FArchive*)handle;

		int64 BytesLeft = Archive->TotalSize() - Archive->Tell();
		int64 ReadAmount = FMath::Min((int64)sizebytes, BytesLeft);

		Archive->Serialize(buffer, ReadAmount);
		*bytesread = (unsigned int)ReadAmount;
		if (ReadAmount < (int64)sizebytes)
		{
			UE_LOG(LogFMOD, Verbose, TEXT(" -> EOF "));
			return FMOD_ERR_FILE_EOF;
		}
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMODSeek(void *handle, unsigned int pos, void * /*userdata*/)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}
   
	FArchive* Archive = (FArchive*)handle;
	Archive->Seek(pos);

	return FMOD_OK;
}
