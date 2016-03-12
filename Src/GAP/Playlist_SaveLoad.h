#ifndef _GAP_PLAYLIST_SAVELOAD_H
#define _GAP_PLAYLIST_SAVELOAD_H

#include <windows.h>

extern BOOL opTreatCD,opPutCD;

DWORD LoadPlaylist(HWND hwnd, const char *fname);
void SavePlaylist(HWND hwnd, const char *fname);
void Load(HWND hwnd);
void Save(HWND hwnd);

#endif
