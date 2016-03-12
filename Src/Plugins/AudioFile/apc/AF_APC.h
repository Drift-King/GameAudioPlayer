#ifndef _AF_APC_H
#define _AF_APC_H

#include <windows.h>

#define IDSTR_APC       "CRYO_APC"
#define IDSTR_VER120    "1.20"
#define IDSTR_HNM6      "HNM6"
#define IDSTR_COPYRIGHT "Pascal URRO  R&D-Copyright CRYO-"

#pragma pack(1)

typedef struct tagAPCHeader
{
	char  szID[8];
	char  szVersion[4];
	DWORD dwOutSize;
	DWORD dwRate;
	LONG  lSample1;
	LONG  lSample2;
	DWORD dwStereo;
} APCHeader;

#pragma pack()

#endif
