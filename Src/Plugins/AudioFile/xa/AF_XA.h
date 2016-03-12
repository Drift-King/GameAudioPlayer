#ifndef _AF_XA_H
#define _AF_XA_H

#include <windows.h>

#define IDSTR_XAI "XAI\0"
#define IDSTR_XAJ "XAJ\0"

#pragma pack(1)

typedef struct tagXAHeader
{
	char  szID[4];
	DWORD dwOutSize;
	WORD  wTag;
	WORD  wChannels;
	DWORD dwRate;
	DWORD dwAvgByteRate;
	WORD  wAlign;
	WORD  wBits;
} XAHeader;

#pragma pack()

#endif
