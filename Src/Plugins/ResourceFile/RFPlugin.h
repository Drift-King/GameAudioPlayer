#ifndef _ANX_RF_PLUGIN_H
#define _ANX_RF_PLUGIN_H

#include <windows.h>

#define RFP_VER 0x100 // v1.00

#define RFF_VERIFIABLE (0x1)

typedef struct tagRFHandle
{
    LPVOID rfPlugin;
    LPVOID rfHandleData;
} RFHandle;

typedef struct tagRFile
{
    struct tagRFile *next;
    char rfDataString[256];
} RFile;

typedef struct tagRFPlugin
{
	DWORD rfVer;
    DWORD rfFlags;
    HINSTANCE hDllInst;
	HWND hMainWindow;
    char *szINIFileName;
    char *Description;
    char *Version;
    char *rfExtensions;
    char *rfID;
    void (__stdcall *Config)(HWND hwnd);
    void (__stdcall *About)(HWND hwnd);
    void (__stdcall *Init)(void);
    void (__stdcall *Quit)(void);
    RFile* (__stdcall *GetFileList)(const char* resname);
    BOOL (__stdcall *FreeFileList)(RFile* list);
    RFHandle* (__stdcall *OpenFile)(const char* resname, const char* rfDataString);
    BOOL  (__stdcall *CloseFile)(RFHandle* rf);
    DWORD (__stdcall *GetFileSize)(RFHandle* rf);
    DWORD (__stdcall *GetFilePointer)(RFHandle* rf);
    BOOL  (__stdcall *EndOfFile)(RFHandle* rf);
    DWORD (__stdcall *SetFilePointer)(RFHandle* rf, LONG lDistanceToMove, DWORD dwMoveMethod);
    BOOL  (__stdcall *ReadFile)(RFHandle* rf, LPVOID lpBuffer, DWORD toRead, LPDWORD read);
} RFPlugin;

#define RFPLUGIN(p) ((RFPlugin*)((p)->rfPlugin))

#ifndef PRIVEC
#define PRIVEC(x) ((1<<29) | (x))
#endif

#endif
