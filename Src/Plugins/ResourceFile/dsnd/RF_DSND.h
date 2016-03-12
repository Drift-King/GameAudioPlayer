#ifndef _RF_DSND_H
#define _RF_DSND_H

#include <windows.h>

#define IDSTR_DSND   "DSND"
#define PIG_REC_SIZE (17)

#pragma pack (1)

typedef struct tagDSNDHeader
{
	char  id[4];
	DWORD dummy;
	DWORD nFiles;
} DSNDHeader;

typedef struct tagPIGHeader
{
	DWORD nFiles;
	DWORD nSoundFiles;
} PIGHeader;

typedef struct tagDSNDDirEntry
{
	char  fileName[8];
	DWORD nSamples; // ???
	DWORD fileSize; // ???
	DWORD fileStart;
} DSNDDirEntry;

#pragma pack()

#endif
