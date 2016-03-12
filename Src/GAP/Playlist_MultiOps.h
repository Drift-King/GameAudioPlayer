#ifndef _GAP_PLAYLIST_MULTIOPS_H
#define _GAP_PLAYLIST_MULTIOPS_H

#include <windows.h>

#define AFTITLE "<AFTitle>"
#define RFTITLE "<RFTitle>"
#define DEFEXT  "<Default>"
#define CURDIR  ".\\"

extern WORD waveTag,dlgWaveTag;
extern int  multiSaveFileName;
extern char defTemplateDir[MAX_PATH],defTemplateFileTitle[MAX_PATH],defTemplateFileExt[20];
extern char dlgTemplateDir[MAX_PATH],dlgTemplateFileTitle[MAX_PATH],dlgTemplateFileExt[20];

void Remover(HWND hwnd);
void Saver(HWND hwnd);
void Converter(HWND hwnd);

#endif
