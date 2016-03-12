#ifndef _GAP_FDAT_FUNCS_H
#define _GAP_FDAT_FUNCS_H

#include <windows.h>

#include "..\RFPlugin.h"


RFHandle* __stdcall PluginOpenFile (const char* resname, LPCSTR rfDataString);
BOOL  __stdcall PluginCloseFile (RFHandle* rf);
DWORD __stdcall PluginGetFileSize (RFHandle* rf);
DWORD __stdcall PluginGetFilePointer (RFHandle* rf);
BOOL  __stdcall PluginEndOfFile (RFHandle* rf);
DWORD __stdcall PluginSetFilePointer (RFHandle* rf, LONG lDistanceToMove,DWORD dwMoveMethod);
BOOL  __stdcall PluginReadFile (RFHandle* rf, LPVOID lpBuffer,DWORD toRead,LPDWORD read);

#endif