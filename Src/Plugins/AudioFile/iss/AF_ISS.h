#ifndef _AF_ISS_H
#define _AF_ISS_H

#include <windows.h>

#define IDSTR_ISS     "IMA_ADPCM_Sound"
#define IDSTR_VER1000 "1.000"

#pragma pack(1)

typedef struct tagISSBlockHeader
{
	SHORT sample;
	SHORT index;
} ISSBlockHeader;

#pragma pack()

#endif
