/*
 * Game Audio Player source code: saving options
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
#include <prsht.h>
#include <shlobj.h>

#include "Globals.h"
#include "Misc.h"
#include "Options.h"
#include "Options_Saving.h"
#include "Playlist_MultiOps.h"
#include "resource.h"

void SetDefTemplateDir(HWND hwnd, char* dir)
{
	BOOL current;

	CorrectDirString(dir);
	current=!lstrcmpi(dir,CURDIR);
	CheckRadioButton(hwnd,ID_DIRCURRENT,ID_DIRCUSTOM,(current)?ID_DIRCURRENT:ID_DIRCUSTOM);
	EnableWindow(GetDlgItem(hwnd,ID_CUSTOMDIR),!current);
	EnableWindow(GetDlgItem(hwnd,ID_BROWSEDIR),!current);
	SetDlgItemText(hwnd,ID_CUSTOMDIR,dir);
}

void GetDefTemplateDir(HWND hwnd, char* dir, DWORD size)
{
	switch (GetRadioButton(hwnd,ID_DIRCURRENT,ID_DIRCUSTOM))
	{
		case ID_DIRCURRENT:
			lstrcpyn(dir,CURDIR,size);
			break;
		case ID_DIRCUSTOM:
			SendDlgItemMessage(hwnd,ID_CUSTOMDIR,WM_GETTEXT,(WPARAM)size,(LPARAM)(LPSTR)dir);
			break;
		default: // ???
			lstrcpyn(dir,DEF_TEMPLATEDIR,size);
	}
	CorrectDirString(dir);
}

void SetDefTemplateFileTitle(HWND hwnd, char* filetitle)
{
	BOOL aftitle,rftitle;

	aftitle=!lstrcmp(filetitle,AFTITLE);
	rftitle=!lstrcmp(filetitle,RFTITLE);
	CheckRadioButton(hwnd,ID_AFTITLE,ID_TITLECUSTOM,(aftitle)?ID_AFTITLE:((rftitle)?ID_RFTITLE:ID_TITLECUSTOM));
	EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),(!aftitle) && (!rftitle));
	SetDlgItemText(hwnd,ID_CUSTOMTITLE,filetitle);
}

void GetDefTemplateFileTitle(HWND hwnd, char* filetitle, DWORD size)
{
	switch (GetRadioButton(hwnd,ID_AFTITLE,ID_TITLECUSTOM))
	{
		case ID_AFTITLE:
			lstrcpyn(filetitle,AFTITLE,size);
			break;
		case ID_RFTITLE:
			lstrcpyn(filetitle,RFTITLE,size);
			break;
		case ID_TITLECUSTOM:
			SendDlgItemMessage(hwnd,ID_CUSTOMTITLE,WM_GETTEXT,(WPARAM)size,(LPARAM)(LPSTR)filetitle);
			break;
		default: // ???
			lstrcpyn(filetitle,DEF_TEMPLATEFILETITLE,size);
	}
}

void SetDefTemplateFileExt(HWND hwnd, char* fileext)
{
	BOOL defext;

	CorrectExtString(fileext);
	defext=!lstrcmp(fileext,DEFEXT);
	CheckRadioButton(hwnd,ID_EXTDEFAULT,ID_EXTCUSTOM,(defext)?ID_EXTDEFAULT:ID_EXTCUSTOM);
	EnableWindow(GetDlgItem(hwnd,ID_CUSTOMEXT),!defext);
	SetDlgItemText(hwnd,ID_CUSTOMEXT,fileext);
}

void GetDefTemplateFileExt(HWND hwnd, char* fileext, DWORD size)
{
	switch (GetRadioButton(hwnd,ID_EXTDEFAULT,ID_EXTCUSTOM))
	{
		case ID_EXTDEFAULT:
			lstrcpyn(fileext,DEFEXT,size);
			break;
		case ID_EXTCUSTOM:
			SendDlgItemMessage(hwnd,ID_CUSTOMEXT,WM_GETTEXT,(WPARAM)size,(LPARAM)(LPSTR)fileext);
			CorrectExtString(fileext);
			break;
		default:
			lstrcpyn(fileext,DEF_TEMPLATEFILEEXT,size);
	}
}

WORD GetWaveTag(HWND hwnd)
{
	int  index;
	WORD wt;

	index=(int)SendDlgItemMessage(hwnd,ID_WAVETAGS,CB_GETCURSEL,0,(LPARAM)0L);
	wt=(WORD)SendDlgItemMessage(hwnd,ID_WAVETAGS,CB_GETITEMDATA,(WPARAM)index,0L);
	if (wt!=CB_ERR)
		return wt;
	else
		return WAVE_FORMAT_PCM;
}

void SetWaveTag(HWND hwnd, WORD tag)
{
	int count,index;

	count=SendDlgItemMessage(hwnd,ID_WAVETAGS,CB_GETCOUNT,0,0L);
	if (count!=CB_ERR)
	{
		for (index=0;index<count;index++)
		{
			if ((WORD)SendDlgItemMessage(hwnd,ID_WAVETAGS,CB_GETITEMDATA,(WPARAM)index,0L)==tag)
			{
				SendDlgItemMessage(hwnd,ID_WAVETAGS,CB_SETCURSEL,(WPARAM)index,0L);
				return;
			}
		}
	}
	SendDlgItemMessage(hwnd,ID_WAVETAGS,CB_SETCURSEL,(WPARAM)0,0L);
}

LRESULT CALLBACK SavingOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    LPNMHDR		 lpnmhdr;
	BROWSEINFO   bi;
	LPITEMIDLIST lpidl;
	char	     str[MAX_PATH];
	char	     szPath[MAX_PATH];
	char	     lpszTitle[]="Select directory for template:";

    switch (umsg)
    {
		case WM_INITDIALOG:
			CheckRadioButton(hwnd,ID_ASKFILENAME,ID_USETEMPLATE,multiSaveFileName);
			SetDefTemplateDir(hwnd,defTemplateDir);
			SetDefTemplateFileTitle(hwnd,defTemplateFileTitle);
			SetDefTemplateFileExt(hwnd,defTemplateFileExt);
			FillACMTagsComboBox(GetDlgItem(hwnd,ID_WAVETAGS));
			SetWaveTag(hwnd,waveTag);

			DisableApply(hwnd);

			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
					TrackPropPage(hwnd,2);
					multiSaveFileName=GetRadioButton(hwnd,ID_ASKFILENAME,ID_USETEMPLATE);
					GetDefTemplateDir(hwnd,defTemplateDir,sizeof(defTemplateDir));
					GetDefTemplateFileTitle(hwnd,defTemplateFileTitle,sizeof(defTemplateFileTitle));
					GetDefTemplateFileExt(hwnd,defTemplateFileExt,sizeof(defTemplateFileExt));
					waveTag=GetWaveTag(hwnd);
					DisableApply(hwnd);
					SetWindowLong(hwnd,DWL_MSGRESULT,PSNRET_NOERROR);
					break;
				case PSN_RESET:
					DisableApply(hwnd);
					TrackPropPage(hwnd,2);
					break;
				default:
					break;
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_DEFAULTS:
					CheckRadioButton(hwnd,ID_ASKFILENAME,ID_USETEMPLATE,ID_ASKTEMPLATE);
					SetDefTemplateDir(hwnd,DEF_TEMPLATEDIR);
					SetDefTemplateFileTitle(hwnd,DEF_TEMPLATEFILETITLE);
					SetDefTemplateFileExt(hwnd,DEF_TEMPLATEFILEEXT);
					SetWaveTag(hwnd,DEF_WAVETAG);
					EnableApply(hwnd);
					break;
				case ID_CUSTOMDIR:
				case ID_CUSTOMTITLE:
				case ID_CUSTOMEXT:
					if (HIWORD(wparm)==EN_CHANGE)
						EnableApply(hwnd);
					break;
				case ID_WAVETAGS:
					if (HIWORD(wparm)==CBN_SELCHANGE)
						EnableApply(hwnd);
					break;
				case ID_ASKFILENAME:
				case ID_ASKTEMPLATE:
				case ID_USETEMPLATE:
					EnableApply(hwnd);
					break;
				case ID_DIRCURRENT:
					SetDlgItemText(hwnd,ID_CUSTOMDIR,CURDIR);
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMDIR),FALSE);
					EnableWindow(GetDlgItem(hwnd,ID_BROWSEDIR),FALSE);
					EnableApply(hwnd);
					break;
				case ID_DIRCUSTOM:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMDIR),TRUE);
					EnableWindow(GetDlgItem(hwnd,ID_BROWSEDIR),TRUE);
					SetFocus(GetDlgItem(hwnd,ID_CUSTOMDIR));
					EnableApply(hwnd);
					break;
				case ID_AFTITLE:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),FALSE);
					SetDlgItemText(hwnd,ID_CUSTOMTITLE,AFTITLE);
					EnableApply(hwnd);
					break;
				case ID_RFTITLE:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),FALSE);
					SetDlgItemText(hwnd,ID_CUSTOMTITLE,RFTITLE);
					EnableApply(hwnd);
					break;
				case ID_TITLECUSTOM:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),TRUE);
					SendDlgItemMessage(hwnd,ID_CUSTOMTITLE,WM_GETTEXT,(WPARAM)sizeof(str),(LPARAM)(LPSTR)str);
					if ((!lstrcmp(str,AFTITLE)) || (!lstrcmp(str,RFTITLE)))
						SetDlgItemText(hwnd,ID_CUSTOMTITLE,"");
					SetFocus(GetDlgItem(hwnd,ID_CUSTOMTITLE));
					EnableApply(hwnd);
					break;
				case ID_EXTDEFAULT:
					SetDlgItemText(hwnd,ID_CUSTOMEXT,DEFEXT);
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMEXT),FALSE);
					EnableApply(hwnd);
					break;
				case ID_EXTCUSTOM:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMEXT),TRUE);
					SendDlgItemMessage(hwnd,ID_CUSTOMEXT,WM_GETTEXT,(WPARAM)sizeof(str),(LPARAM)(LPSTR)str);
					if (!lstrcmp(str,DEFEXT))
						SetDlgItemText(hwnd,ID_CUSTOMEXT,"");
					SetFocus(GetDlgItem(hwnd,ID_CUSTOMEXT));
					EnableApply(hwnd);
					break;
				case ID_BROWSEDIR:
					lstrcpy(szPath,"");
					bi.hwndOwner=hwnd;
					bi.pidlRoot=NULL;
					bi.pszDisplayName=szPath;
					bi.lpszTitle=lpszTitle;
					bi.ulFlags=BIF_RETURNONLYFSDIRS;
					bi.lpfn=BrowseProc;
					GetHomeDirectory(str,sizeof(str));
					if (str[lstrlen(str)-1]=='\\')
						str[lstrlen(str)-1]=0;
					bi.lParam=(LPARAM)str;
					if ((lpidl=SHBrowseForFolder(&bi))!=NULL)
					{
						SHGetPathFromIDList(lpidl,szPath);
						CorrectDirString(szPath);
						SetDlgItemText(hwnd,ID_CUSTOMDIR,szPath);
					}
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
