#ifndef _AF_CMP_H
#define _AF_CMP_H

#include <windows.h>

#define IDSTR_FCMP "FCMP"

#pragma pack(1)

typedef struct tagCMPHeader
{
	char  szID[4];
	DWORD dwDataSize;
	DWORD dwRate;
	WORD  wBits;
} CMPHeader;

#pragma pack()

#endif
