#ifndef _ACM_STREAM_H
#define _ACM_STREAM_H

#include <windows.h>

BOOL __stdcall ACMStreamGetInfo
(
	DWORD  readFunc,
	DWORD  fileHandle,
	DWORD *rate,
	WORD  *channels,
	WORD  *bits,
	DWORD *size
);

DWORD __stdcall ACMStreamInit(DWORD readFunc, DWORD fileHandle);
void  __stdcall ACMStreamShutdown(DWORD acm);
DWORD __stdcall ACMStreamReadAndDecompress(DWORD acm, char* buff, DWORD count);

#endif
