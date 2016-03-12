#ifndef _ANX_AF_PLUGIN_H
#define _ANX_AF_PLUGIN_H

#include <windows.h>

#define NO_AFNAME "<Unknown>"

#define AFP_VER 0x100 // v1.00

#define AFF_VERIFIABLE (0x1)

typedef struct tagAFile
{
    struct tagAFile *prev,*next;
    LPVOID  afPlugin;
    char    afName[256];
    char    afID[10];
    char    afDataString[256];
	DWORD   afTime;
	LPVOID  rfPlugin;
	char    rfName[MAX_PATH];
    char    rfID[10];
    char    rfDataString[256];
    DWORD   fsStart,fsLength;
	DWORD   dwData;
} AFile;

#include "..\ResourceFile\RFPlugin.h"

typedef struct tagFSHandle
{
    RFHandle *rf;
    DWORD     start,length;
    AFile    *node;
    LPVOID    afData;
} FSHandle;

typedef struct tagFSHandler
{
    FSHandle* (__stdcall *OpenFile)(AFile* node);
    BOOL  (__stdcall *CloseFile)(FSHandle* fsh);
    DWORD (__stdcall *GetFileSize)(FSHandle* fsh);
    DWORD (__stdcall *GetFilePointer)(FSHandle* fsh);
    BOOL  (__stdcall *EndOfFile)(FSHandle* fsh);
    DWORD (__stdcall *SetFilePointer)(FSHandle* fsh, LONG lDistanceToMove, DWORD dwMoveMethod);
    BOOL  (__stdcall *ReadFile)(FSHandle* fsh, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead);
} FSHandler;

typedef struct tagSearchPattern
{
	DWORD fileIDsize;
	char *fileID;
} SearchPattern;

typedef struct tagAFPlugin
{
	DWORD afVer;
    DWORD afFlags;
    HINSTANCE hDllInst;
	HWND hMainWindow;
    char *szINIFileName;
    char *Description;
    char *Version;
    char *afExtensions;
    char *afID;
    DWORD numPatterns;
    SearchPattern *patterns;
    FSHandler* fsh;
    void (__stdcall *Config)(HWND hwnd);
    void (__stdcall *About)(HWND hwnd);
    void (__stdcall *Init)(void);
    void (__stdcall *Quit)(void);
    void (__stdcall *InfoBox)(AFile* node, HWND hwnd);
    BOOL (__stdcall *InitPlayback)(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits);
    BOOL (__stdcall *ShutdownPlayback)(FSHandle* f);
    DWORD (__stdcall *FillPCMBuffer)(FSHandle* f, char* buffer, DWORD buffsize, DWORD* buffpos);
    AFile* (__stdcall *CreateNodeForID)(HWND hwnd, RFHandle* rf, DWORD ipattern, DWORD pos, DWORD *newpos);
    AFile* (__stdcall *CreateNodeForFile)(FSHandle* file, const char* rfName, const char* rfDataString);
    DWORD (__stdcall *GetTime)(AFile* node);
    void (__stdcall *Seek)(FSHandle* f, DWORD pos);
} AFPlugin;

#define AFPLUGIN(node) ((AFPlugin*)((node)->afPlugin))

#ifndef PRIVEC
#define PRIVEC(x) ((1<<29) | (x))
#endif

#ifndef CORRALIGN
#define CORRALIGN(size,align) ((size)-(size)%(align))
#endif

#endif
