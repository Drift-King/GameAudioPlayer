/*
 * Game Audio Player source code: playlist options
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

#include <stdio.h>

#include <windows.h>
#include <commctrl.h>
#include <prsht.h>

#include "Globals.h"
#include "Misc.h"
#include "Options.h"
#include "Options_Playlist.h"
#include "Player.h"
#include "Playlist.h"
#include "PlayDialog.h"
#include "Playlist_NodeOps.h"
#include "Playlist_SaveLoad.h"
#include "resource.h"

void SetRepeatType(HWND hwnd, int rt)
{
	SetCheckBox(hwnd,ID_REPEAT,(rt!=ID_REPEAT));
	CheckRadioButton(hwnd,ID_REPEAT_ONE,ID_REPEAT_ALL,rt);
	EnableWindow(GetDlgItem(hwnd,ID_REPEAT_ONE),(rt!=ID_REPEAT));
	EnableWindow(GetDlgItem(hwnd,ID_REPEAT_ALL),(rt!=ID_REPEAT));
}

int GetRepeatType(HWND hwnd)
{
    if (GetCheckBox(hwnd,ID_REPEAT))
		return GetRadioButton(hwnd,ID_REPEAT_ONE,ID_REPEAT_ALL);
	else
		return ID_REPEAT;
}

void SetIntroScanLength(HWND hwnd, UINT isl)
{
    char str[20];

    wsprintf(str,"%u",isl);
    SetDlgItemText(hwnd,ID_INTROSCAN_EDIT,str);
    SendDlgItemMessage(hwnd,ID_INTROSCAN_UPDN,UDM_SETPOS,0,(LPARAM)MAKELONG(isl,0));
}

UINT GetIntroScanLength(HWND hwnd)
{
	char str[20];
    UINT isl;

    SendDlgItemMessage(hwnd,ID_INTROSCAN_EDIT,WM_GETTEXT,(WPARAM)20,(LPARAM)(LPSTR)str);
    sscanf(str,"%u",&isl);
    return  isl;
}

void SetPlaylistDefaults(HWND hwnd)
{
    SetCheckBox(hwnd,ID_PLAYNEXT,TRUE);
    SetCheckBox(hwnd,ID_INTROSCAN,FALSE);
    SetIntroScanLength(hwnd,DEF_INTROSCANLENGTH);
    SetRepeatType(hwnd,ID_REPEAT_ALL);
    SetCheckBox(hwnd,ID_SHUFFLE,FALSE);
    SetCheckBox(hwnd,ID_AUTOLOAD,TRUE);
    EnableWindow(GetDlgItem(hwnd,ID_AUTOPLAY),TRUE);
    SetCheckBox(hwnd,ID_AUTOPLAY,FALSE);
	SetCheckBox(hwnd,ID_NOTIME,TRUE);
    SetCheckBox(hwnd,ID_TREATCD,TRUE);
    EnableWindow(GetDlgItem(hwnd,ID_CDDRIVE),TRUE);
    SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_SETCURSEL,(WPARAM)DEF_CDDRIVE,0L);
    SetCheckBox(hwnd,ID_PUTCD,TRUE);
	CheckRadioButton(hwnd,ID_DROP_OFF,ID_DROP_LOAD,ID_DROP_ASK);
	CheckRadioButton(hwnd,ID_PLAYLIST_DIALOG,ID_PLAYLIST_WINDOW,ID_PLAYLIST_WINDOW);
}

void ApplyPlaylistOptions(HWND hwnd)
{
    opPlayNext=GetCheckBox(hwnd,ID_PLAYNEXT);
    opRepeatType=GetRepeatType(hwnd);
    opShuffle=GetCheckBox(hwnd,ID_SHUFFLE);
    opIntroScan=GetCheckBox(hwnd,ID_INTROSCAN);
    introScanLength=1000*GetIntroScanLength(hwnd);
    opAutoLoad=GetCheckBox(hwnd,ID_AUTOLOAD);
    opAutoPlay=GetCheckBox(hwnd,ID_AUTOPLAY);
	opNoTime=GetCheckBox(hwnd,ID_NOTIME);
    opTreatCD=GetCheckBox(hwnd,ID_TREATCD);
    if (opTreatCD)
		CDDrive=(int)SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_GETCURSEL,(WPARAM)0,0L);
    opPutCD=GetCheckBox(hwnd,ID_PUTCD);
	opDropSupport=GetRadioButton(hwnd,ID_DROP_OFF,ID_DROP_LOAD);
	DragAcceptFiles(playDialog,(BOOL)(opDropSupport!=ID_DROP_OFF));
	DragAcceptFiles(playlistWnd,(BOOL)(opDropSupport!=ID_DROP_OFF));
	opPlaylistType=GetRadioButton(hwnd,ID_PLAYLIST_DIALOG,ID_PLAYLIST_WINDOW);
}

LRESULT CALLBACK PlaylistOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    char       drive;
    char       root[3];
    LPNMHDR    lpnmhdr;

    switch (umsg)
    {
		case WM_INITDIALOG:
			SetCheckBox(hwnd,ID_PLAYNEXT,opPlayNext);
			SetCheckBox(hwnd,ID_INTROSCAN,opIntroScan);
			SetRepeatType(hwnd,opRepeatType);
			SetCheckBox(hwnd,ID_SHUFFLE,opShuffle);
			SetCheckBox(hwnd,ID_AUTOLOAD,opAutoLoad);
			EnableWindow(GetDlgItem(hwnd,ID_AUTOPLAY),opAutoLoad);
			SetCheckBox(hwnd,ID_AUTOPLAY,(opAutoLoad)?opAutoPlay:FALSE);

			SetCheckBox(hwnd,ID_NOTIME,opNoTime);
			SetCheckBox(hwnd,ID_TREATCD,opTreatCD);
			root[1]=':';
			root[2]=0;
			for (drive='A';drive<='Z';drive++)
			{
				root[0]=drive;
				if (GetDriveType(root)==DRIVE_CDROM)
					SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_ADDSTRING,(WPARAM)0,(LPARAM)root);
			}
			SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_SETCURSEL,(WPARAM)0,0L);
			EnableWindow(GetDlgItem(hwnd,ID_CDDRIVE),opTreatCD);
			SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_SETCURSEL,(WPARAM)CDDrive,0L);
			if (SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_GETCURSEL,0,0L)==CB_ERR)
				SendDlgItemMessage(hwnd,ID_CDDRIVE,CB_SETCURSEL,(WPARAM)0,0L);
			SetCheckBox(hwnd,ID_PUTCD,opPutCD);

			SendDlgItemMessage(hwnd,ID_INTROSCAN_UPDN,UDM_SETBUDDY,(WPARAM)GetDlgItem(hwnd,ID_INTROSCAN_EDIT),0L);
			SendDlgItemMessage(hwnd,ID_INTROSCAN_UPDN,UDM_SETBASE,(WPARAM)10,0L);
			SendDlgItemMessage(hwnd,ID_INTROSCAN_UPDN,UDM_SETRANGE,0,(LPARAM)MAKELONG(UD_MAXVAL,max(0,UD_MINVAL)));
			SetIntroScanLength(hwnd,introScanLength/1000);
			CheckRadioButton(hwnd,ID_DROP_OFF,ID_DROP_LOAD,opDropSupport);
			CheckRadioButton(hwnd,ID_PLAYLIST_DIALOG,ID_PLAYLIST_WINDOW,opPlaylistType);

			DisableApply(hwnd);

			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
					TrackPropPage(hwnd,1);
					ApplyPlaylistOptions(hwnd);
					DisableApply(hwnd);
					SetWindowLong(hwnd,DWL_MSGRESULT,PSNRET_NOERROR);
					break;
				case PSN_RESET:
					DisableApply(hwnd);
					TrackPropPage(hwnd,1);
					break;
				default:
					break;
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_DEFAULTS:
					SetPlaylistDefaults(hwnd);
					EnableApply(hwnd);
					break;
				case ID_TREATCD:
					EnableWindow(GetDlgItem(hwnd,ID_CDDRIVE),GetCheckBox(hwnd,ID_TREATCD));
					EnableApply(hwnd);
					break;
				case ID_AUTOLOAD:
					EnableWindow(GetDlgItem(hwnd,ID_AUTOPLAY),GetCheckBox(hwnd,ID_AUTOLOAD));
					if (!GetCheckBox(hwnd,ID_AUTOLOAD))
						SetCheckBox(hwnd,ID_AUTOPLAY,FALSE);
					EnableApply(hwnd);
					break;
				case ID_INTROSCAN_EDIT:
					if (HIWORD(wparm)==EN_CHANGE)
						EnableApply(hwnd);
					break;
				case ID_CDDRIVE:
					if (HIWORD(wparm)==CBN_SELCHANGE)
						EnableApply(hwnd);
					break;
				case ID_PLAYNEXT:
				case ID_SHUFFLE:
				case ID_INTROSCAN:
				case ID_INTROSCAN_UPDN:
				case ID_REPEAT_ONE:
				case ID_REPEAT_ALL:
				case ID_AUTOPLAY:
				case ID_NOTIME:
				case ID_PUTCD:
				case ID_DROP_OFF:
				case ID_DROP_ASK:
				case ID_DROP_ADD:
				case ID_DROP_SCAN:
				case ID_DROP_LOAD:
				case ID_PLAYLIST_DIALOG:
				case ID_PLAYLIST_WINDOW:
					EnableApply(hwnd);
					break;
				case ID_REPEAT:
					EnableWindow(GetDlgItem(hwnd,ID_REPEAT_ONE),GetCheckBox(hwnd,ID_REPEAT));
					EnableWindow(GetDlgItem(hwnd,ID_REPEAT_ALL),GetCheckBox(hwnd,ID_REPEAT));
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
