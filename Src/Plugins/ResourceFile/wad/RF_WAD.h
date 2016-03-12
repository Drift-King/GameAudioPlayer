#ifndef _RF_WAD_H
#define _RF_WAD_H

#include <windows.h>

#define IDSTR_IWAD "IWAD"
#define IDSTR_PWAD "PWAD"

#pragma pack (1)

typedef struct tagWADHeader
{
	char  id[4];
	DWORD numLumps;
	DWORD dirStart;
} WADHeader;

typedef struct tagWADDirEntry
{
	DWORD lumpStart;
	DWORD lumpSize;
	char  lumpName[8];
} WADDirEntry;

#pragma pack()

#endif
