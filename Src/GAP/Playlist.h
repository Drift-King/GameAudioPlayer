#ifndef _GAP_PLAYLIST_H
#define _GAP_PLAYLIST_H

#include <windows.h>

extern int  opPlaylistType;
extern BOOL inEdit;
extern HWND playlistWnd,playlistToolbar;

void ShowPlaylist(HWND hwnd);
void InitPlaylist(void);
int  AskSavePlaylist(HWND hwnd);
void Selection(HWND hwnd, int num);
void PlaylistToolbarPlayToggle(BOOL playing);

#endif
