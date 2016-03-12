#ifndef _GAP_PLAYDIALOG_H
#define _GAP_PLAYDIALOG_H

#include <windows.h>

extern BOOL isTrackBarSliderLocked;
extern HWND playDialog,
            toolbarWnd;
extern BOOL opAutoLoad,
            opAutoPlay,
			opPlayerOnTop,
			opEasyMove,
			opTimeElapsed;
extern int  opDropSupport;

LRESULT CALLBACK PlayDialog(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm);
DWORD GetPlaybackPos(void);
void OnDropFiles(HWND hwnd, HDROP hdrop);
void UpdateTrackBar(HWND hwnd);
void TrackMainMenu(HWND hwnd);
void ShowPlaybackTime(void);
void InitStats(void);
void PlayDlgToolbarPlayToggle(BOOL playing);
void ResetPlaybackPos(void);
void ResetStats(void);
void ShowStats(void);
void ShowPlaybackPos(DWORD pos, DWORD time);
int Quit(HWND hwnd);
DWORD AcceptFiles
(
	HWND hwnd, 
	HDROP hdrop, 
	const char* title, 
	const char* header, 
	DWORD (*Action)(HWND,const char*)
);

#endif
