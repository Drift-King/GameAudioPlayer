#ifndef _RF_BLB_H
#define _RF_BLB_H

#include <windows.h>

#define IDSTR_BLB "\x40\x49\x00\x02"
#define ID_BLB	  0x07

#define ID_BLB_TYPE_SOUND 0x07
#define ID_BLB_TYPE_MUSIC 0x08
#define ID_BLB_TYPE_VIDEO 0x0A

#define ID_BLB_ACTION_NONE       0x01
#define ID_BLB_ACTION_COMPRESSED 0x03
#define ID_BLB_ACTION_FAKE       0x65

#pragma pack(1)

typedef struct tagBLBHeader
{
	char  szID[4];
	BYTE  bID;
	BYTE  bUnknown;
	WORD  wDataSize;
	DWORD dwFileSize;
	DWORD dwNumber;
} BLBHeader;

typedef struct tagBLBDirEntry
{
	BYTE  bType;
	BYTE  bAction;
	WORD  wDataIndex;
	DWORD dwUnknown;
	DWORD dwStart;
	DWORD dwFileID; // for fake files
	DWORD dwOutSize;
} BLBDirEntry;

#pragma pack()

#endif
