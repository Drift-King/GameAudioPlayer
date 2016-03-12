#ifndef _RF_BAG_H
#define _RF_BAG_H

#include <windows.h>

#define IDSTR_GABA "GABA"

#define ID_BAG_STEREO     (0x1)
#define ID_BAG_PCM        (0x2) // ???
#define ID_BAG_16BIT      (0x4)
#define ID_BAG_COMPRESSED (0x8)
#define ID_BAG_MP3        (0x20)

#pragma pack (1)

typedef struct tagBAGHeader
{
	char  szID[4];
	DWORD dwDummy;
	DWORD dwFileAmount;
	DWORD dwPad;
} BAGHeader;

typedef struct tagBAGDirEntry
{
	char  szFileName[32];
	DWORD dwStart;
	DWORD dwSize;
	DWORD dwSampleRate;
	DWORD dwFlags;
	DWORD dwBlockAlign;
	char  pad[12];
} BAGDirEntry;

#pragma pack()

#endif
