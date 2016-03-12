#ifndef _AF_SOL_H
#define _AF_SOL_H

#include <windows.h>

#define IDSTR_SOL "SOL\0"

#define SOL_COMPRESSED (0x1)
#define SOL_16BIT      (0x4)
#define SOL_SIGNED     (0x8)
#define SOL_STEREO     (0x10)

#define SOL_ID_8D (0x8D)
#define SOL_ID_D  (0x0D)

#pragma pack(1)

typedef struct tagSOLHeader
{
	BYTE  pad;
	BYTE  shift;
    char  id[4];
    WORD  rate;
    BYTE  flags;
    DWORD size;
} SOLHeader;

#pragma pack()

#endif
