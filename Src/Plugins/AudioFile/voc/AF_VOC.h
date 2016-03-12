#ifndef _AF_VOC_H
#define _AF_VOC_H

#include <windows.h>

#define IDSTR_VOC "Creative Voice File\x1A"

#define	VOC_TERM	 (0)
#define	VOC_DATA	 (1)
#define	VOC_CONT	 (2)
#define	VOC_SILENCE	 (3)
#define	VOC_MARKER	 (4)
#define	VOC_TEXT	 (5)
#define	VOC_LOOP	 (6)
#define	VOC_LOOPEND	 (7)
#define	VOC_EXTENDED (8)
#define	VOC_NEWDATA  (9)

#define VOC_CT_8BIT  (0)
#define VOC_CT_4BIT  (1)
#define VOC_CT_26BIT (2)
#define VOC_CT_2BIT  (3)

#define VOCBLOCKSIZE(vbh) ((vbh).sizel+((vbh).sizeh<<16))

#pragma pack(1)

typedef struct tagVOCHeader
{
    char id[20];
    WORD datastart;
	BYTE ver_minor;
	BYTE ver_major;
	WORD ver_checksum;
} VOCHeader;

typedef struct tagVOCBlockHeader
{
	BYTE type;
	WORD sizel;
	BYTE sizeh;
} VOCBlockHeader;

typedef struct tagVOCDataHeader
{
	BYTE sr;
	BYTE ct;
} VOCDataHeader;

typedef struct tagVOCNewDataHeader
{
	DWORD rate;
	BYTE  bits;
	BYTE  channels;
	BYTE  unknown[6];
} VOCNewDataHeader;

typedef struct tagVOCSilenceHeader
{
	WORD length;
	BYTE sr;
} VOCSilenceHeader;

#pragma pack()

#endif
