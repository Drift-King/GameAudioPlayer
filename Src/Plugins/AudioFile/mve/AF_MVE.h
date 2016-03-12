#ifndef _AF_MVE_H
#define _AF_MVE_H

#include <windows.h>

#define IDSTR_MVE "Interplay MVE File\x1A\0"

#define MVE_BT_SOUNDINFO  (0x00)
#define MVE_BT_SOUNDFRAME (0x01)
#define MVE_BT_IMAGEINFO  (0x02)
#define MVE_BT_IMAGEFRAME (0x03)
#define MVE_BT_END        (0x05) // ???

#define MVE_SBT_END       (0x01)
#define MVE_SBT_SOUNDINFO (0x03)
#define MVE_SBT_SOUNDDATA (0x08)

#define MVE_SI_STEREO     (0x01)
#define MVE_SI_16BIT      (0x02)
#define MVE_SI_COMPRESSED (0x04)

#define MVE_HEADER_MAGIC1 (0x001A)
#define MVE_HEADER_MAGIC2 (0x0100)
#define MVE_HEADER_MAGIC3 (0xFFFFEDCCL)

#pragma pack(1)

typedef struct tagMVEHeader
{
    char szID[20];
	WORD magic1;
	WORD magic2;
	WORD magic3;
} MVEHeader;

typedef struct tagMVEBlockHeader
{
	WORD size;
	BYTE type;
	BYTE subtype;
} MVEBlockHeader;

typedef struct tagMVESoundHeader
{
	WORD  iFrame;
	WORD  magic;
	WORD  outSize;
} MVESoundHeader;

typedef struct tagMVESoundInfo
{
	WORD  unknown1;
	BYTE  flags;
	BYTE  unknown2;
	WORD  rate;
	DWORD blockSize;
} MVESoundInfo;

#pragma pack()

#endif
