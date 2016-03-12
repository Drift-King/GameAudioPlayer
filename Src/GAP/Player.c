/*
 * Game Audio Player source code: streaming waveform player
 *
 * Copyright (C) 1998-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <mmreg.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Messages.h"
#include "Misc.h"
#include "Options_Playback.h"
#include "Errors.h"
#include "Player.h"
#include "PlayDialog.h"
#include "FSHandler.h"
#include "Playlist.h"
#include "Playlist_NodeOps.h"
#include "ListView.h"
#include "Plugins.h"
#include "resource.h"

BOOL	   isPlaying,isPaused,killPlaybackThread;
HANDLE	   playbackThreadHandle=INVALID_HANDLE_VALUE;
DWORD	   curBufferSize,seekNeeded;
FSHandle  *curAFile;
HWAVEOUT   waveDevice;
DWORD	   curTime,curTimeOrigin;
char	  *waveBuffer1;
char	  *waveBuffer2;
WAVEHDR    waveHdr1,waveHdr2;

CRITICAL_SECTION csPlayback;

DWORD	   bufferSize,
		   introScanLength;
UINT	   waveDeviceID,
		   timerDelay;
int		   opRepeatType,
		   playbackPriority;
BOOL	   opPlayNext,
		   opShuffle,
	       opIntroScan;

BOOL AllocBuffers(DWORD bs)
{
    waveBuffer1=(char*)GlobalAlloc(GPTR,bs);
    waveBuffer2=(char*)GlobalAlloc(GPTR,bs);
	curBufferSize=bs;

    return TRUE;
}

BOOL FreeBuffers(void)
{
    GlobalFree(waveBuffer1);
    GlobalFree(waveBuffer2);
	curBufferSize=0;

    return TRUE;
}

void InitPlayer(void)
{
    srand(GetTickCount()%RAND_MAX);
    curTime=0;
    curTimeOrigin=-1;
    isPlaying=FALSE;
    isPaused=FALSE;
	seekNeeded=-1;
	killPlaybackThread=FALSE;
	playbackThreadHandle=INVALID_HANDLE_VALUE;
    curAFile=NULL;
	InitializeCriticalSection(&csPlayback);
}

void ShutdownPlayer(void)
{
    Stop();
	DeleteCriticalSection(&csPlayback);
}

void PlayNext(void)
{
	if (!opPlayNext)
		return;
    if (curNode==NULL)
		return;

    if (opShuffle)
		Play(GetNodeByIndex(rand()%list_size));
    else if (opRepeatType==ID_REPEAT_ONE)
		Play(curNode);
    else if (curNode->next!=NULL)
		Play(curNode->next);
    else if (opRepeatType==ID_REPEAT_ALL)
		Play(list_start);
}

DWORD WINAPI __stdcall PlaybackThread(LPVOID b)
{
	BOOL  isEndOfStream;
	DWORD seekPos,bufferPos,oldTimeOrigin;

	isEndOfStream=FALSE;
	bufferPos=-1;
	while (!(*((BOOL*)b)))
	{
		if ((seekPos=InterlockedExchange(&seekNeeded,-1))!=-1)
		{
			EnterCriticalSection(&csPlayback);
			waveOutReset(waveDevice);
			waveOutUnprepareHeader(waveDevice,&waveHdr1,sizeof(WAVEHDR));
			waveOutUnprepareHeader(waveDevice,&waveHdr2,sizeof(WAVEHDR));
			waveHdr1.dwFlags=WHDR_DONE;
			waveHdr2.dwFlags=WHDR_DONE;
			bufferPos=-1;
			isEndOfStream=FALSE;
			curTime=seekPos;
			curTimeOrigin=-1;
			LeaveCriticalSection(&csPlayback);
			AFPLUGIN(curNode)->Seek(curAFile,seekPos);
		}
		if (isPaused)
		{
			PostMessage(playDialog,WM_GAP_SHOW_PLAYBACK_TIME,0,0);
			Sleep(10);
			continue;
		}
		if (!isTrackBarSliderLocked)
		{
			if ((oldTimeOrigin=InterlockedExchange(&curTimeOrigin,-1))!=-1)
			{
				InterlockedExchange(&curTimeOrigin,timeGetTime());
				InterlockedExchange(&curTime,curTime+curTimeOrigin-oldTimeOrigin); // ???
				PostMessage(playDialog,WM_GAP_SHOW_PLAYBACK_POS,curTime,curNode->afTime);
			}
		}
		if ((opIntroScan) && (curTime>=introScanLength))
		{
			PostMessage(playDialog,WM_GAP_END_OF_PLAYBACK,0,0);
			return 0;
		}
		else if (isEndOfStream)
		{
			if (
				((waveHdr1.dwFlags) & WHDR_DONE) &&
				((waveHdr2.dwFlags) & WHDR_DONE)
			   )
			{
				PostMessage(playDialog,WM_GAP_END_OF_PLAYBACK,0,0);
				return 0;
			}
			Sleep(10);
			continue;
		}
		else if ((waveHdr1.dwFlags) & WHDR_DONE)
		{
			if (bufferPos!=-1)
				InterlockedExchange(&curTime,bufferPos);
			waveOutUnprepareHeader(waveDevice,&waveHdr1,sizeof(WAVEHDR));
			waveHdr1.lpData=(LPSTR)waveBuffer1;
			waveHdr1.dwBufferLength=AFPLUGIN(curNode)->FillPCMBuffer(curAFile,waveBuffer1,curBufferSize,&bufferPos);
			if (waveHdr1.dwBufferLength==0)
			{
				isEndOfStream=TRUE;
				continue;
			}
			waveHdr1.dwFlags=0L;
			waveOutPrepareHeader(waveDevice,&waveHdr1,sizeof(WAVEHDR));
			waveOutWrite(waveDevice,&waveHdr1,sizeof(WAVEHDR));
			if (curTimeOrigin==-1)
				InterlockedExchange(&curTimeOrigin,timeGetTime());
		}
		else if ((waveHdr2.dwFlags) & WHDR_DONE)
		{
			if (bufferPos!=-1)
				InterlockedExchange(&curTime,bufferPos);
			waveOutUnprepareHeader(waveDevice,&waveHdr2,sizeof(WAVEHDR));
			waveHdr2.lpData=(LPSTR)waveBuffer2;
			waveHdr2.dwBufferLength=AFPLUGIN(curNode)->FillPCMBuffer(curAFile,waveBuffer2,curBufferSize,&bufferPos);
			if (waveHdr2.dwBufferLength==0)
			{
				isEndOfStream=TRUE;
				continue;
			}
			waveHdr2.dwFlags=0L;
			waveOutPrepareHeader(waveDevice,&waveHdr2,sizeof(WAVEHDR));
			waveOutWrite(waveDevice,&waveHdr2,sizeof(WAVEHDR));
			if (curTimeOrigin==-1)
				InterlockedExchange(&curTimeOrigin,timeGetTime());
		}
		else
			Sleep(20);
	}
	return 0;
}

void Seek(void)
{
    if (curNode==NULL)
		return;
    if (curAFile==NULL)
		return;
    if (AFPLUGIN(curNode)->Seek==NULL)
		return;

	InterlockedExchange(&seekNeeded,GetPlaybackPos());
}

void Play(AFile* node)
{
    MMRESULT mmr;
    DWORD    rate;
    WORD     channels,bits;
	WAVEFORMATEX curWaveFormat;
	DWORD	 playbackThreadID;

    if (node==NULL)
		return;

    if (isPlaying)
		Stop();
    curNode=node;
    UpdatePlaylistSelection();
	if (curNode->afTime==-1)
	{
		if (AFPLUGIN(curNode)!=NULL)
			if (AFPLUGIN(curNode)->GetTime!=NULL)
				curNode->afTime=AFPLUGIN(curNode)->GetTime(curNode);
    }
    ShowStats();
    if ((curAFile=FSOpenForPlayback(playDialog,node,&rate,&channels,&bits))==NULL)
		return;
    curWaveFormat.wFormatTag=WAVE_FORMAT_PCM;
    curWaveFormat.nChannels=channels;
    curWaveFormat.nSamplesPerSec=rate;
    curWaveFormat.wBitsPerSample=bits;
    curWaveFormat.nBlockAlign=curWaveFormat.nChannels*(curWaveFormat.wBitsPerSample/8);
    curWaveFormat.nAvgBytesPerSec=curWaveFormat.nSamplesPerSec*curWaveFormat.nBlockAlign;
    curWaveFormat.cbSize=0;
    if ((mmr=waveOutOpen(&waveDevice,waveDeviceID,&curWaveFormat,0,0,CALLBACK_NULL))!=MMSYSERR_NOERROR)
    {
		Stop();
		ReportMMError(playDialog,mmr,"Cannot open WaveOut device.");
		return;
    }
    AllocBuffers(bufferSize);
    waveHdr1.dwFlags=WHDR_DONE;
    waveHdr2.dwFlags=WHDR_DONE;
    curTime=GetPlaybackPos();
    if (AFPLUGIN(curNode)->Seek!=NULL)
		AFPLUGIN(curNode)->Seek(curAFile,curTime);
    curTimeOrigin=-1;
    ShowPlaybackPos(curTime,curNode->afTime);
    isPaused=FALSE;
    isPlaying=TRUE;
	PlayDlgToolbarPlayToggle(TRUE);
	PlaylistToolbarPlayToggle(TRUE);
	killPlaybackThread=FALSE;
	playbackThreadHandle=(HANDLE)CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)PlaybackThread,(LPVOID)&killPlaybackThread,0,&playbackThreadID);
	if (playbackThreadHandle==NULL)
	{
		playbackThreadHandle=INVALID_HANDLE_VALUE;
		Stop();
		MessageBox(GetFocus(),"Cannot start playback thread.",szAppName,MB_OK | MB_ICONERROR);
		return;
	}
	else
		SetThreadPriority(playbackThreadHandle,playbackPriority);
    ShowStats();
}

void Next(void)
{
    if (curNode==NULL)
		return;

    if (isPlaying)
    {
		if (curNode->next!=NULL)
			Play(curNode->next);
		else
			Play(list_start);
    }
    else
    {
		if (curNode->next!=NULL)
			curNode=curNode->next;
		else
			curNode=list_start;
		UpdatePlaylistSelection();
		ShowStats();
    }
}

void Previous(void)
{
    if (curNode==NULL)
		return;
    if (isPlaying)
    {
		if (curNode->prev!=NULL)
			Play(curNode->prev);
		else
			Play(list_end);
    }
    else
    {
		if (curNode->prev!=NULL)
			curNode=curNode->prev;
		else
	    curNode=list_end;
		UpdatePlaylistSelection();
		ShowStats();
    }
}

void Pause(void)
{
    if (!isPlaying)
		return;

    if (!isPaused)
    {
		waveOutPause(waveDevice);
		isPaused=TRUE;
		InterlockedExchange(&curTime,curTime+timeGetTime()-curTimeOrigin); // ???
		ShowPlaybackPos(curTime,curNode->afTime);
		PlayDlgToolbarPlayToggle(FALSE);
		PlaylistToolbarPlayToggle(FALSE);
    }
    else
    {
		waveOutRestart(waveDevice);
		isPaused=FALSE;
		InterlockedExchange(&curTimeOrigin,timeGetTime());
		PlayDlgToolbarPlayToggle(TRUE);
		PlaylistToolbarPlayToggle(TRUE);
    }
    ShowStats();
}

void Stop(void)
{
	if (playbackThreadHandle!=INVALID_HANDLE_VALUE)
	{
		killPlaybackThread=TRUE;
		if (WaitForSingleObject(playbackThreadHandle,INFINITE)==WAIT_TIMEOUT) // ???
		{
			MessageBox(GetFocus(),"Timeout while waiting for playback thread to terminate.",szAppName,MB_OK | MB_ICONERROR);
			TerminateThread(playbackThreadHandle,0);
		}
		CloseHandle(playbackThreadHandle);
		playbackThreadHandle=INVALID_HANDLE_VALUE;
		killPlaybackThread=FALSE;
	}
    isPlaying=FALSE;
    isPaused=FALSE;
    PlayDlgToolbarPlayToggle(FALSE);
	PlaylistToolbarPlayToggle(FALSE);
    ResetPlaybackPos();
    curTime=0;
    curTimeOrigin=-1;
    waveOutReset(waveDevice);
    waveOutUnprepareHeader(waveDevice,&waveHdr1,sizeof(WAVEHDR));
    waveOutUnprepareHeader(waveDevice,&waveHdr2,sizeof(WAVEHDR));
    waveOutClose(waveDevice);
    if ((curNode!=NULL) && (AFPLUGIN(curNode)!=NULL))
		AFPLUGIN(curNode)->ShutdownPlayback(curAFile);
	FSCloseFile(curAFile);
    curAFile=NULL;
	FreeBuffers();
    ShowStats();
}

void ResetWaveDev(HWND hwnd, UINT nwdid)
{
    if (waveDeviceID==nwdid)
		return;

    if (isPlaying)
		MessageBox(hwnd,"WaveOut device will be changed only after the current playback finishes.",szAppName,MB_OK | MB_ICONEXCLAMATION);
    waveDeviceID=nwdid;
}

void ResetBufferSize(HWND hwnd, DWORD nbs)
{
    if (bufferSize==nbs)
		return;

    if (isPlaying)
		MessageBox(hwnd,"Playback buffer size will be changed only after the current playback finishes.",szAppName,MB_OK | MB_ICONEXCLAMATION);
    bufferSize=nbs;
}

void SetPlaybackPriority(int pp)
{
	if (playbackThreadHandle!=INVALID_HANDLE_VALUE)
		SetThreadPriority(playbackThreadHandle,pp);
	playbackPriority=pp;
}