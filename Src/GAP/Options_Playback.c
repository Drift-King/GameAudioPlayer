/*
 * Game Audio Player source code: playback options
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
#include <prsht.h>

#include "Globals.h"
#include "Misc.h"
#include "Options.h"
#include "Options_Playback.h"
#include "Errors.h"
#include "Player.h"
#include "resource.h"

void SetBufferSizeText(HWND hwnd, DWORD bs)
{
    char str[256];

    wsprintf(str,"Memory required: %lu KB",bs/512);
    SetDlgItemText(hwnd,ID_BUFFERSIZE,str);
}

void SetBufferSize(HWND hwnd, DWORD bs)
{
    SendDlgItemMessage(hwnd,ID_BUFFER,TBM_SETPOS,(WPARAM)TRUE,(LPARAM)(bs/1024));
    SetBufferSizeText(hwnd,bs);
}

DWORD GetBufferSize(HWND hwnd)
{
    DWORD bs;

    bs=1024*(DWORD)SendDlgItemMessage(hwnd,ID_BUFFER,TBM_GETPOS,0,0L);
    return bs;
}

void SetWaveDev(HWND hwnd, UINT wd)
{
    int i,num;

    num=(int)SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_GETCOUNT,0,0L);
    for (i=0;i<num;i++)
    {
		if ((UINT)SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_GETITEMDATA,(WPARAM)i,0L)==wd)
		{
			SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_SETCURSEL,(WPARAM)i,0L);
			break;
		}
    }
}

UINT GetWaveDev(HWND hwnd)
{
    int     i;
    DWORD   wd;

    i=(int)SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_GETCURSEL,0,0L);
    wd=(DWORD)SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_GETITEMDATA,(WPARAM)i,0L);
    if (wd==CB_ERR)
		return WAVE_MAPPER;
    else
		return (UINT)wd;
}

void SetPriorityClassCtrl(HWND hwnd, DWORD pc)
{
    LONG pc_pos=1;

    if (pc==IDLE_PRIORITY_CLASS)
		pc_pos=0;
    else if (pc==NORMAL_PRIORITY_CLASS)
		pc_pos=1;
    else if (pc==HIGH_PRIORITY_CLASS)
		pc_pos=2;
    else if (pc==REALTIME_PRIORITY_CLASS)
		pc_pos=3;
    SendDlgItemMessage(hwnd,ID_PCLASS,TBM_SETPOS,(WPARAM)TRUE,(LPARAM)pc_pos);
}

void SetPlaybackPriorityCtrl(HWND hwnd, int pp)
{
    int pp_pos=ID_PNORMAL;

    if (pp==THREAD_PRIORITY_IDLE)
		pp_pos=ID_PIDLE;
    else if (pp==THREAD_PRIORITY_LOWEST)
		pp_pos=ID_PLOWEST;
    else if (pp==THREAD_PRIORITY_BELOW_NORMAL)
		pp_pos=ID_PBELOWNORMAL;
    else if (pp==THREAD_PRIORITY_NORMAL)
		pp_pos=ID_PNORMAL;
    else if (pp==THREAD_PRIORITY_ABOVE_NORMAL)
		pp_pos=ID_PABOVENORMAL;
    else if (pp==THREAD_PRIORITY_HIGHEST)
		pp_pos=ID_PHIGHEST;
    else if (pp==THREAD_PRIORITY_TIME_CRITICAL)
		pp_pos=ID_PTIMECRITICAL;
    CheckRadioButton(hwnd,ID_PIDLE,ID_PTIMECRITICAL,pp_pos);
}

void SetMainPriorityCtrl(HWND hwnd, int mp)
{
    int mp_pos=ID_MNORMAL;

    if (mp==THREAD_PRIORITY_IDLE)
		mp_pos=ID_MIDLE;
    else if (mp==THREAD_PRIORITY_LOWEST)
		mp_pos=ID_MLOWEST;
    else if (mp==THREAD_PRIORITY_BELOW_NORMAL)
		mp_pos=ID_MBELOWNORMAL;
    else if (mp==THREAD_PRIORITY_NORMAL)
		mp_pos=ID_MNORMAL;
    else if (mp==THREAD_PRIORITY_ABOVE_NORMAL)
		mp_pos=ID_MABOVENORMAL;
    else if (mp==THREAD_PRIORITY_HIGHEST)
		mp_pos=ID_MHIGHEST;
    else if (mp==THREAD_PRIORITY_TIME_CRITICAL)
		mp_pos=ID_MTIMECRITICAL;
    CheckRadioButton(hwnd,ID_MIDLE,ID_MTIMECRITICAL,mp_pos);
}

DWORD GetPriorityClassCtrl(HWND hwnd)
{
    LONG pc_pos;

    pc_pos=(LONG)SendDlgItemMessage(hwnd,ID_PCLASS,TBM_GETPOS,0,0L);
    switch ((char)pc_pos)
    {
		case 0:
			return IDLE_PRIORITY_CLASS;
		case 1:
			return NORMAL_PRIORITY_CLASS;
		case 2:
			return HIGH_PRIORITY_CLASS;
		case 3:
			return REALTIME_PRIORITY_CLASS;
    }

    return NORMAL_PRIORITY_CLASS;
}

int GetPlaybackPriorityCtrl(HWND hwnd)
{
	switch (GetRadioButton(hwnd,ID_PIDLE,ID_PTIMECRITICAL))
	{
		case ID_PIDLE:
			return THREAD_PRIORITY_IDLE;
		case ID_PLOWEST:
			return THREAD_PRIORITY_LOWEST;
		case ID_PBELOWNORMAL:
			return THREAD_PRIORITY_BELOW_NORMAL;
		case ID_PNORMAL:
			return THREAD_PRIORITY_NORMAL;
		case ID_PABOVENORMAL:
			return THREAD_PRIORITY_ABOVE_NORMAL;
		case ID_PHIGHEST:
			return THREAD_PRIORITY_HIGHEST;
		case ID_PTIMECRITICAL:
			return THREAD_PRIORITY_TIME_CRITICAL;
		default: // ???
			break;
	}

	return THREAD_PRIORITY_NORMAL;
}

int GetMainPriorityCtrl(HWND hwnd)
{
	switch (GetRadioButton(hwnd,ID_MIDLE,ID_MTIMECRITICAL))
	{
		case ID_MIDLE:
			return THREAD_PRIORITY_IDLE;
		case ID_MLOWEST:
			return THREAD_PRIORITY_LOWEST;
		case ID_MBELOWNORMAL:
			return THREAD_PRIORITY_BELOW_NORMAL;
		case ID_MNORMAL:
			return THREAD_PRIORITY_NORMAL;
		case ID_MABOVENORMAL:
			return THREAD_PRIORITY_ABOVE_NORMAL;
		case ID_MHIGHEST:
			return THREAD_PRIORITY_HIGHEST;
		case ID_MTIMECRITICAL:
			return THREAD_PRIORITY_TIME_CRITICAL;
		default: // ???
			break;
	}

	return THREAD_PRIORITY_NORMAL;
}

void SetPlaybackDefaults(HWND hwnd)
{
    SetWaveDev(hwnd,DEF_WAVEDEV);
    SetBufferSize(hwnd,DEF_BUFFERSIZE*1024);
	SetPriorityClassCtrl(hwnd,DEF_PRIORITYCLASS);
	SetMainPriorityCtrl(hwnd,DEF_MAINPRIORITY);
    SetPlaybackPriorityCtrl(hwnd,DEF_PLAYBACKPRIORITY);
}

void ApplyPlaybackOptions(HWND hwnd)
{
	ResetWaveDev(hwnd,GetWaveDev(hwnd));
    ResetBufferSize(hwnd,GetBufferSize(hwnd));
	SetPriorityClass(GetCurrentProcess(),GetPriorityClassCtrl(hwnd));
    SetThreadPriority(GetCurrentThread(),GetMainPriorityCtrl(hwnd));
	SetPlaybackPriority(GetPlaybackPriorityCtrl(hwnd));
}

#define TICFREQ (3)

void SetTicFreq(HWND hwnd, int tbid)
{
	DWORD freq,rmin,rmax,length;
	RECT  rect;

	rmin=(DWORD)SendDlgItemMessage(hwnd,tbid,TBM_GETRANGEMIN,0,0L);
	rmax=(DWORD)SendDlgItemMessage(hwnd,tbid,TBM_GETRANGEMAX,0,0L);
	SendDlgItemMessage(hwnd,tbid,TBM_GETCHANNELRECT,0,(LPARAM)&rect);
	length=(DWORD)(rect.right-rect.left);
	freq=MulDiv((rmax-rmin),TICFREQ,length);
	SendDlgItemMessage(hwnd,tbid,TBM_SETTICFREQ,(WPARAM)freq,0L);
}

LRESULT CALLBACK PlaybackOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    LPNMHDR	lpnmhdr;
    UINT	i,index,devsnum,tbpos;
    WAVEOUTCAPS woc;
    MMRESULT	mmr;

    switch (umsg)
    {
		case WM_INITDIALOG:
			devsnum=waveOutGetNumDevs();
			if (devsnum==0)
				MessageBox(GetFocus(),"No WaveOut devices found.\nPlayback impossible.",szAppName,MB_OK | MB_ICONEXCLAMATION);
			index=SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)"Wave Mapper (default)");
			SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_SETITEMDATA,(WPARAM)index,(LPARAM)(DWORD)WAVE_MAPPER);
			for (i=0;i<devsnum;i++)
			{
				if ((mmr=waveOutGetDevCaps(i,&woc,sizeof(woc))) != MMSYSERR_NOERROR)
					ReportMMError(hwnd,mmr,"Cannot get WaveOut device capabilities.");
				else
				{
					index=SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)woc.szPname);
					SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_SETITEMDATA,(WPARAM)index,(LPARAM)(DWORD)i);
					if (i==waveDeviceID)
						SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_SETCURSEL,index,0L);
				}
			}
			if (SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_GETCURSEL,0,0L)==CB_ERR)
				SendDlgItemMessage(hwnd,ID_WAVEDEVS,CB_SETCURSEL,0,0L);

			SendDlgItemMessage(hwnd,ID_BUFFER,TBM_SETRANGE,(WPARAM)TRUE,(LPARAM)MAKELONG(BUFFERSIZE_MIN,BUFFERSIZE_MAX));
			SetTicFreq(hwnd,ID_BUFFER);
			SetBufferSize(hwnd,bufferSize);
			SendDlgItemMessage(hwnd,ID_PCLASS,TBM_SETRANGE,(WPARAM)TRUE,(LPARAM)MAKELONG(0,3));
			SendDlgItemMessage(hwnd,ID_PCLASS,TBM_SETTICFREQ,(WPARAM)1,(LPARAM)1);
			SetPriorityClassCtrl(hwnd,GetPriorityClass(GetCurrentProcess()));
			SetMainPriorityCtrl(hwnd,GetThreadPriority(GetCurrentThread()));
			SetPlaybackPriorityCtrl(hwnd,playbackPriority);

			DisableApply(hwnd);

			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
					TrackPropPage(hwnd,0);
					ApplyPlaybackOptions(hwnd);
					DisableApply(hwnd);
					SetWindowLong(hwnd,DWL_MSGRESULT,PSNRET_NOERROR);
					break;
				case PSN_RESET:
					DisableApply(hwnd);
					TrackPropPage(hwnd,0);
					break;
				default:
					break;
			}
			break;
		case WM_HSCROLL:
			switch (GetDlgCtrlID((HWND)lparm))
			{
				case ID_BUFFER:
					tbpos=(DWORD)SendDlgItemMessage(hwnd,ID_BUFFER,TBM_GETPOS,0,0L);
					SetBufferSizeText(hwnd,tbpos*1024);
					EnableApply(hwnd);
					break;
				case ID_PCLASS:
					EnableApply(hwnd);
					break;
				default:
					break;
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_DEFAULTS:
					SetPlaybackDefaults(hwnd);
					EnableApply(hwnd);
					break;
				case ID_PIDLE:
				case ID_PLOWEST:
				case ID_PBELOWNORMAL:
				case ID_PNORMAL:
				case ID_PABOVENORMAL:
				case ID_PHIGHEST:
				case ID_PTIMECRITICAL:
				case ID_MIDLE:
				case ID_MLOWEST:
				case ID_MBELOWNORMAL:
				case ID_MNORMAL:
				case ID_MABOVENORMAL:
				case ID_MHIGHEST:
				case ID_MTIMECRITICAL:
					EnableApply(hwnd);
					break;
				case ID_WAVEDEVS:
					if (HIWORD(wparm)==CBN_SELCHANGE)
						EnableApply(hwnd);
					break;
				default:
					break;
			}
			break;
		default:
			break;
    }
    return FALSE;
}
