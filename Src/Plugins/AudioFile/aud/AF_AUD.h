#ifndef _AF_AUD_H
#define _AF_AUD_H

#include <windows.h>

#define AUD_STEREO	   (0x1)
#define AUD_16BIT	   (0x2)
#define AUD_IMA_ADPCM  (99)
#define AUD_WS_ADPCM   (1)
#define ID_DEAF 	   (0x0000DEAF)
#define IDSTR_DEAF	   "\xAF\xDE\x00\x00"

#define IDSTR_FORM	   "FORM"
#define IDSTR_WVQA	   "WVQA"
#define IDSTR_VQHD	   "VQHD"
#define IDSTR_FINF	   "FINF"
#define IDSTR_VQFR	   "VQFR"
#define IDSTR_SND0	   "SND0"
#define IDSTR_SND1	   "SND1"
#define IDSTR_SND2	   "SND2"

#define VQA_VER_1	   (0x0001)
#define VQA_VER_2	   (0x0002)
#define VQA_VER_3	   (0x0003)

#pragma pack(1)

typedef struct tagAUDHeader
{
    WORD  rate;
    DWORD size;
    DWORD outsize;
    BYTE  flags;
    BYTE  type;
} AUDHeader;

typedef struct tagAUDHeaderOld
{
    WORD  rate;
    DWORD size;
    BYTE  flags;
    BYTE  type;
} AUDHeaderOld;

typedef struct tagAUDChunkHeader
{
    WORD  size;
    WORD  outsize;
    DWORD id;
} AUDChunkHeader;

typedef struct tagFORMHeader
{
	char  szID[4];
	DWORD size;
	char  szType[4];
} FORMHeader;

typedef struct tagFORMChunkHeader
{
	char  szID[4];
	DWORD size;
} FORMChunkHeader;

typedef struct tagVQAHeader
{
	WORD  version;
	WORD  unknown1;
	WORD  numFrames;
	WORD  width;
	WORD  height;
	WORD  unknown2;
	WORD  unknown3;
	WORD  unknown4;
	WORD  unknown5;
	DWORD unknown6;
	WORD  unknown7;
	WORD  rate;
	BYTE  channels;
	BYTE  bits;
	char  unknown8[14];
} VQAHeader;

typedef struct tagSND1Header
{
	WORD outSize;
	WORD size;
} SND1Header;

#pragma pack()

#define SWAPDWORD(x) ((((x)&0xFF)<<24)+(((x)>>24)&0xFF)+(((x)>>8)&0xFF00)+(((x)<<8)&0xFF0000))
#define CORRLARGE(x) (((x)>=0x40000000)?((x)-0x40000000):(x))

#endif
