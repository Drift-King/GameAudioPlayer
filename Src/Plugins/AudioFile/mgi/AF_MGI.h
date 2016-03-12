#ifndef _AF_MGI_H
#define _AF_MGI_H

#include <windows.h>

#define IDSTR_MGI "\x8F\xC2\x35\x3F"

#pragma pack(1)

typedef struct tagMGIHeader
{
	char  szID[4];
	DWORD dwUnknown1;
	DWORD dwNumSecIndices;
	DWORD dwUnknown2;
	DWORD dwNumIntIndices;
} MGIHeader;

typedef struct tagMGIIntDesc
{
	DWORD dwIndex;
	DWORD dwSection;
} MGIIntDesc;

typedef struct tagMGISecDesc
{
	DWORD dwStart;
	DWORD dwIndex;
	DWORD dwOutSize;
} MGISecDesc;

#pragma pack()

#endif
