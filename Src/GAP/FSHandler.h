#ifndef _GAP_FSHANDLER_H
#define _GAP_FSHANDLER_H

#include <windows.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

void SaveNode(HWND hwnd, AFile* node, const char* fname);

FSHandle* FSOpenForPlayback(HWND hwnd, AFile* node, DWORD* rate, WORD* channels, WORD* bits);
FSHandle* __stdcall FSOpenFileAsSection(RFPlugin* plugin, const char* rfName, const char* rfDataString, DWORD start, DWORD length);
FSHandle* __stdcall FSOpenFile(AFile* node);
BOOL  __stdcall FSCloseFile(FSHandle* f);
DWORD __stdcall FSSetFilePointer(FSHandle* f, LONG pos, DWORD method);
DWORD __stdcall FSGetFilePointer(FSHandle* f);
DWORD __stdcall FSGetFileSize(FSHandle* f);
BOOL  __stdcall FSEndOfFile(FSHandle* f);
BOOL  __stdcall FSReadFile(FSHandle* f, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead);

extern FSHandler appFSHandler;

#endif
