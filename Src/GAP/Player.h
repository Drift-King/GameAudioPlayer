#ifndef _GAP_PLAYER_H
#define _GAP_PLAYER_H

#include <windows.h>
#include <mmsystem.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

extern BOOL      isPlaying,isPaused;
extern DWORD     bufferSize,
				 introScanLength;
extern UINT      waveDeviceID;
extern int       opRepeatType,
				 playbackPriority;
extern BOOL      opPlayNext,
                 opShuffle,
                 opIntroScan;

extern CRITICAL_SECTION csPlayback;

void  InitPlayer(void);
void  ShutdownPlayer(void);
void  Play(AFile* node);
void  Next(void);
void  PlayNext(void);
void  Previous(void);
void  Pause(void);
void  Stop(void);
void  Seek(void);
void  ResetWaveDev(HWND hwnd, UINT nwdid);
void  ResetBufferSize(HWND hwnd, DWORD nbs);
void  SetPlaybackPriority(int pp);

#endif
