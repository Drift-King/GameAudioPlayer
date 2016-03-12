#ifndef _TREESEL_TREESEL_H
#define _TREESEL_TREESEL_H

#include <windows.h>


#define FILE_SIZE_UNKNOWN -1L
#define INCLUDE_ALL 3
#define INCLUDE_SELECTED 0

typedef void* HDIRSEL;
typedef void* HDIRECTORY;
typedef int (__stdcall* RegexpTester) (LPSTR string, LPSTR regexp);

typedef struct tagNLItem {
        struct tagNLItem *next;
        char str[256];
} NLItem;


// Directory Tree routines
extern "C" __declspec(dllexport) HDIRSEL __stdcall    CreateDirSelector (RegexpTester match);
extern "C" __declspec(dllexport) long __stdcall       GetDirectoryIndex (HDIRSEL dirSel, const char* path);
extern "C" __declspec(dllexport) HDIRECTORY __stdcall AddDirectory (HDIRSEL dirSel, const char* path);
extern "C" __declspec(dllexport) int __stdcall        AddFile (HDIRSEL dirSel, const char* fullName, long size, long dirIndex);
extern "C" __declspec(dllexport) int __stdcall        AddFileToDirectory (HDIRSEL dirSel, const char* name, long size, HDIRECTORY dir);
extern "C" __declspec(dllexport) int __stdcall        ShowDialog (HDIRSEL dirSel);
extern "C" __declspec(dllexport) void __stdcall       DestroyDirSelector (HDIRSEL dirSel);

extern "C" __declspec(dllexport) NLItem* __stdcall    CreateNameList (HDIRSEL dirSel, int includeWhat);
extern "C" __declspec(dllexport) void __stdcall       DestroyNameList (NLItem* first);

// Misc
extern "C" __declspec(dllexport) BOOL __stdcall       WildMatch (LPSTR str, LPSTR wildcard);
extern "C" __declspec(dllexport) void __stdcall       CenterInParent (HWND child);

#endif