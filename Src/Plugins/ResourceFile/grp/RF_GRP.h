#ifndef _RF_GRP_H
#define _RF_GRP_H

#include <windows.h>

#define IDSTR_KenSilverman "KenSilverman"

#pragma pack (1)

typedef struct tagGRPHeader
{
	char  id[12];
	DWORD numFiles;
} GRPHeader;

typedef struct tagGRPDirEntry
{
	char  fileName[12];
	DWORD fileSize;
} GRPDirEntry;

#pragma pack()

#endif
