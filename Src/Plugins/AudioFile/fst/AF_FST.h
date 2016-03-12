#ifndef _AF_FST_H
#define _AF_FST_H

#include <windows.h>

#define IDSTR_2TSF "2TSF"

#pragma pack(1)

typedef struct tagFSTHeader
{
	char  szID[4];
	DWORD dwWidth;
	DWORD dwHeight;
	DWORD dwUnknown1;
	DWORD nFrames;
	DWORD dwFrameRate;
	DWORD dwRate;
	WORD  wBits;
	WORD  wUnknown2;
} FSTHeader;

typedef struct tagFSTFrameEntry
{
	DWORD dwImageSize;
	WORD  wSoundSize;
} FSTFrameEntry;

#pragma pack()

#endif
