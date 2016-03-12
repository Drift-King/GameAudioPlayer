#ifndef _AF_ASF_H
#define _AF_ASF_H

#include <windows.h>

#define IDSTR_1SNh "1SNh"
#define IDSTR_1SNd "1SNd"
#define IDSTR_1SNl "1SNl"
#define IDSTR_1SNe "1SNe"
#define IDSTR_EACS "EACS"

// compression
#define EACS_PCM            (0x00)
#define EACS_IMA_ADPCM      (0x02)

// type
#define EACS_MULTIBLOCK     (0x00)
#define EACS_SINGLEBLOCK    (0xFF)

#pragma pack(1)

typedef struct tagASFBlockHeader
{
	char  id[4];
	DWORD size;
} ASFBlockHeader;

typedef struct tagEACSHeader
{
	char  id[4];
	DWORD dwRate;
	BYTE  bBits;
	BYTE  bChannels;
	BYTE  bCompression;
	BYTE  bType; // ???
	DWORD dwNumSamples;
	DWORD dwLoopStart;
	DWORD dwLoopLength;
	DWORD dwDataStart;
	DWORD dwUnknown;
} EACSHeader;

#pragma pack()

#endif
