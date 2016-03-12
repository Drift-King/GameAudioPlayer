#ifndef _AF_WADSFX_H
#define _AF_WADSFX_H

#include <windows.h>

#define IDSTR_3 "\x3\0"
#define IDSTR_0 "\0\0"

#pragma pack (1)

typedef struct tagSBSoundHeader
{
	char id[2];
	WORD rate;
	WORD nSamples;
	char dummy[2];
} SBSoundHeader;

#pragma pack()

#endif
