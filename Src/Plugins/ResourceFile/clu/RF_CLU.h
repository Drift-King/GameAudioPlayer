#ifndef _RF_CLU_H
#define _RF_CLU_H

#include <windows.h>

#define IDSTR_CLU "\xFF\xF0\xFF\xF0"

#pragma pack(1)

typedef struct tagCLUHeader
{
	DWORD dwNumber;
	char  szID[4];
} CLUHeader;

typedef struct tagCLUDirEntry
{
	DWORD dwStart;
	DWORD dwSize;
} CLUDirEntry;

#pragma pack()

#endif
