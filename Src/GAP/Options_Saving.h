#ifndef _GAP_OPTIONS_SAVING_H
#define _GAP_OPTIONS_SAVING_H

#include <windows.h>

#define DEF_TEMPLATEDIR ".\\Saved\\"
#define DEF_TEMPLATEFILETITLE AFTITLE
#define DEF_TEMPLATEFILEEXT DEFEXT
#define DEF_WAVETAG (WAVE_FORMAT_PCM)

void SetDefTemplateDir(HWND hwnd, char* dir);
void GetDefTemplateDir(HWND hwnd, char* dir, DWORD size);
void SetDefTemplateFileTitle(HWND hwnd, char* filetitle);
void GetDefTemplateFileTitle(HWND hwnd, char* filetitle, DWORD size);
void SetDefTemplateFileExt(HWND hwnd, char* fileext);
void GetDefTemplateFileExt(HWND hwnd, char* fileext, DWORD size);
WORD GetWaveTag(HWND hwnd);
void SetWaveTag(HWND hwnd, WORD tag);
LRESULT CALLBACK SavingOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm);

#endif
