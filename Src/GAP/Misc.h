#ifndef _GAP_MISC_H
#define _GAP_MISC_H

#include <stdio.h>

#include <windows.h>

typedef struct tagIntOption 
{
    char *str;
    int   value;
} IntOption;

#define DEF_POS ((UINT)-32000)

#define CD_SIG "[CD]"

extern char pafDir[MAX_PATH];
extern int  CDDrive;

BOOL CenterWindow(HWND hwndChild, HWND hwndParent);
char* GetTimeString(DWORD time, char* str);
char* GetShortTimeString(DWORD time, char* str);
void UpdateWholeWindow(HWND hwnd);
BOOL LoadWndPos(const char* wnd, int* px, int* py);
void LoadWndSize(const char* wnd, int* px, int* py);
void SaveWndPos(HWND hwnd, const char* wnd, BOOL ontop);
void SaveWndSize(HWND hwnd, const char* wnd);
void LoadLastPlaylist(HWND hwnd);
void SaveLastPlaylist(HWND hwnd);
int StrLenEx(LPCTSTR str);
LPTSTR StrCatEx(LPTSTR str1, LPCTSTR str2);
LPTSTR StrExNum(LPTSTR str, DWORD num);
char* GetFileTitleEx(const char* path);
char* ReplaceAppFileTitle(char* str, DWORD size, const char* rstr);
char* ReplaceAppFileExtension(char* str, DWORD size, const char* rstr);
char* GetHomeDirectory(char* str, DWORD size);
BOOL  dirRecursive;
int   CALLBACK BrowseRecursiveProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
int   CALLBACK BrowseProc(HWND hwnd,UINT uMsg,LPARAM lParam,LPARAM lpData);
void CorrectDirString(char* dir);
void CorrectExtString(char* ext);
char* GetFileExtension(const char* filename);
void CutFileExtension(char* filename);
void CutFileTitle(char* filename);
BOOL FileExists(const char* fname);
BOOL EnsureDirPresence(const char* fname);
void CutDirString(char* dir);
BOOL DirectoryExists(const char* dir);
BOOL CreateDirectoryRecursive(const char* dir);
void AppendDefExt(char* filename, const char* filter);
void FillACMTagsComboBox(HWND hwnd);
long fsize(FILE* f);
void ClearMessageQueue(HWND hwnd, DWORD timeout);
void ProcessMessages(HWND hwnd, BOOL bUpdateParent);
HWND ShowWaitWindow(HWND hwnd, const char* title, const char* msg);
void CloseWaitWindow(HWND hwnd);
long CountFiles(char* str);
BOOL IsCDPath(const char* resname);
void RefineResName(char* str, const char* resname);
void GetCDDriveRoot(char* root, int num);
DWORD PerformActionForDir(HWND hwnd, DWORD (*Action)(HWND,const char*), BOOL recursive);
void SetCheckBox(HWND hwnd, int cbId, BOOL cbVal);
BOOL GetCheckBox(HWND hwnd, int cbId);
int  GetRadioButton(HWND hwnd, int first, int last);

#endif
