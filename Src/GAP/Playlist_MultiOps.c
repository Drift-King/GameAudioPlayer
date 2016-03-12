/*
 * Game Audio Player source code: multifile operations
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
#include <commdlg.h>
#include <shlobj.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Misc.h"
#include "ListView.h"
#include "Progress.h"
#include "FSHandler.h"
#include "Player.h"
#include "Errors.h"
#include "Convert.h"
#include "Options_Saving.h"
#include "Playlist_MultiOps.h"
#include "Playlist_NodeOps.h"
#include "resource.h"

WORD waveTag,dlgWaveTag;
int  multiSaveFileName;
char defTemplateDir[MAX_PATH],defTemplateFileTitle[MAX_PATH],defTemplateFileExt[20];
char dlgTemplateDir[MAX_PATH],dlgTemplateFileTitle[MAX_PATH],dlgTemplateFileExt[20];

char szSaveAudioFile[]="Save Audio File";
char szConvertAudioFile[]="Convert Audio File";
char szSaverTemplate[]="File Name Template For MultiSaving";
char szConverterTemplate[]="File Name Template For MultiConverting";

void Remover(HWND hwnd)
{
	UINT   selcount;
	int    index;
	BOOL   wasStopped;
	AFile *node;

	if (playList==NULL)
		return;

	selcount=ListView_GetSelectedCount(playList);
	if (selcount=0)
		return;
	wasStopped=FALSE;
	index=-1;
	while ((index=ListView_GetNextItem(playList,index,LVNI_ALL | LVNI_SELECTED))!=-1)
		ListView_SetItemState(playList,index,LVIS_CUT,LVIS_CUT);
	index=0;
	while ((index=ListView_GetNextItem(playList,index-1,LVNI_ALL | LVNI_CUT))!=-1)
	{
		if (!wasStopped)
		{
			node=GetListViewNode(index);
			if ((isPlaying) && (curNode==node))
			{
				Stop();
				wasStopped=TRUE;
			}
		}
		DeleteNode(index);
	}
	if (wasStopped)
		Play(curNode);
}

void ShowCurrentTemplate(HWND hwnd)
{
	char str[MAX_PATH],tstr[MAX_PATH];

	GetDefTemplateDir(hwnd,str,sizeof(str));
	GetDefTemplateFileTitle(hwnd,tstr,sizeof(tstr));
	lstrcat(str,tstr);
	if (lstrcmp(tstr,AFTITLE))
		lstrcat(str,"0000");
	GetDefTemplateFileExt(hwnd,tstr,sizeof(tstr));
	lstrcat(str,tstr);
	SetDlgItemText(hwnd,ID_TEMPLATE,str);
}

LRESULT CALLBACK TemplateDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	BROWSEINFO bi;
	LPITEMIDLIST lpidl;
	char str[MAX_PATH];
	char szPath[MAX_PATH];
	char lpszTitle[]="Select directory for template:";

    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetWindowText(hwnd,((BOOL)lparm)?szConverterTemplate:szSaverTemplate);
			SetDefTemplateDir(hwnd,dlgTemplateDir);
			SetDefTemplateFileTitle(hwnd,dlgTemplateFileTitle);
			SetDefTemplateFileExt(hwnd,dlgTemplateFileExt);
			ShowCurrentTemplate(hwnd);
			EnableWindow(GetDlgItem(hwnd,ID_WAVFORMAT),(BOOL)lparm);
			EnableWindow(GetDlgItem(hwnd,ID_WAVETAGS),(BOOL)lparm);
			if ((BOOL)lparm)
			{
				FillACMTagsComboBox(GetDlgItem(hwnd,ID_WAVETAGS));
				SetWaveTag(hwnd,dlgWaveTag);
			}
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_DEFAULTS:
					SetDefTemplateDir(hwnd,defTemplateDir);
					SetDefTemplateFileTitle(hwnd,defTemplateFileTitle);
					SetDefTemplateFileExt(hwnd,defTemplateFileExt);
					ShowCurrentTemplate(hwnd);
					SetWaveTag(hwnd,waveTag);
					break;
				case ID_CUSTOMDIR:
				case ID_CUSTOMTITLE:
				case ID_CUSTOMEXT:
					if (HIWORD(wparm)!=EN_CHANGE)
						break;
					ShowCurrentTemplate(hwnd);
					break;
				case ID_DIRCURRENT:
					SetDlgItemText(hwnd,ID_CUSTOMDIR,CURDIR);
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMDIR),FALSE);
					EnableWindow(GetDlgItem(hwnd,ID_BROWSEDIR),FALSE);
					break;
				case ID_DIRCUSTOM:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMDIR),TRUE);
					EnableWindow(GetDlgItem(hwnd,ID_BROWSEDIR),TRUE);
					SetFocus(GetDlgItem(hwnd,ID_CUSTOMDIR));
					ShowCurrentTemplate(hwnd);
					break;
				case ID_AFTITLE:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),FALSE);
					SetDlgItemText(hwnd,ID_CUSTOMTITLE,AFTITLE);
					break;
				case ID_RFTITLE:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),FALSE);
					SetDlgItemText(hwnd,ID_CUSTOMTITLE,RFTITLE);
					break;
				case ID_TITLECUSTOM:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMTITLE),TRUE);
					SendDlgItemMessage(hwnd,ID_CUSTOMTITLE,WM_GETTEXT,(WPARAM)sizeof(str),(LPARAM)(LPSTR)str);
					if ((!lstrcmp(str,AFTITLE)) || (!lstrcmp(str,RFTITLE)))
						SetDlgItemText(hwnd,ID_CUSTOMTITLE,"");
					SetFocus(GetDlgItem(hwnd,ID_CUSTOMTITLE));
					ShowCurrentTemplate(hwnd);
					break;
				case ID_EXTDEFAULT:
					SetDlgItemText(hwnd,ID_CUSTOMEXT,DEFEXT);
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMEXT),FALSE);
					break;
				case ID_EXTCUSTOM:
					EnableWindow(GetDlgItem(hwnd,ID_CUSTOMEXT),TRUE);
					SendDlgItemMessage(hwnd,ID_CUSTOMEXT,WM_GETTEXT,(WPARAM)sizeof(str),(LPARAM)(LPSTR)str);
					if (!lstrcmp(str,DEFEXT))
						SetDlgItemText(hwnd,ID_CUSTOMEXT,"");
					SetFocus(GetDlgItem(hwnd,ID_CUSTOMEXT));
					ShowCurrentTemplate(hwnd);
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
				case IDOK:
					GetDefTemplateDir(hwnd,dlgTemplateDir,sizeof(dlgTemplateDir));
					GetDefTemplateFileTitle(hwnd,dlgTemplateFileTitle,sizeof(dlgTemplateFileTitle));
					GetDefTemplateFileExt(hwnd,dlgTemplateFileExt,sizeof(dlgTemplateFileExt));
					dlgWaveTag=GetWaveTag(hwnd);
					EndDialog(hwnd,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
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

BOOL  templateReady;
DWORD fileNameNum;

void ResetSaveFileNameEx(void)
{
	templateReady=FALSE;
	fileNameNum=0;
}

void ComposeSaveFileName
(
	char* filename, 
	AFile* node, 
	DWORD num, 
	char* dir, 
	char* title, 
	char* ext
)
{
	char str[256];

	CorrectDirString(dir);
	lstrcpy(filename,dir);
	if (!lstrcmp(title,AFTITLE))
	{
		lstrcat(filename,GetFileTitleEx(node->afName));
		if (ext!=NULL)
			CutFileExtension(filename);
		if (num!=-1)
		{
			wsprintf(str,"%0.4lu",num);
			lstrcat(filename,str);
		}
	}
	else if (!lstrcmp(title,RFTITLE))
	{
		lstrcat(filename,GetFileTitleEx(node->rfName));
		if (ext!=NULL)
			CutFileExtension(filename);
		if (num!=-1)
		{
			wsprintf(str,"%0.4lu",num);
			lstrcat(filename,str);
		}
	}
	else
	{
		lstrcat(filename,title);
		if (ext!=NULL)
			CutFileExtension(filename);
		if (num!=-1)
		{
			wsprintf(str,"%0.4lu",num);
			lstrcat(filename,str);
		}
	}
	if (ext!=NULL)
	{
		if (lstrcmp(ext,DEFEXT))
		{
			CorrectExtString(ext);
			lstrcat(filename,ext);
		}
	}
}

UINT APIENTRY ConvertFileDlgHook(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR lpnmhdr;

	switch (uiMsg)
	{
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lParam;
			switch (lpnmhdr->code)
			{
				case CDN_INITDONE:
					FillACMTagsComboBox(GetDlgItem(hdlg,ID_WAVETAGS));
					SetWaveTag(hdlg,dlgWaveTag);
					break;
				case CDN_FILEOK:
					dlgWaveTag=GetWaveTag(hdlg);
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

BOOL GetSaveFileNameEx
(
	HWND hwnd, 
	int how, 
	char* ext, 
	AFile* node, 
	char* filename, 
	DWORD maxfilename, 
	WORD* tag
)
{
	OPENFILENAME ofn={0};
    char  szDirName[MAX_PATH];
	BOOL  flag;

	switch (how)
	{
		case ID_ASKFILENAME:
			GetCurrentDirectory(sizeof(szDirName), szDirName);
			filename[0]=0;
			ComposeSaveFileName(filename,node,-1,"",(lstrcmpi(node->afName,NO_AFNAME))?AFTITLE:RFTITLE,(ext==NULL)?NULL:"");
			AppendDefExt(filename,ext);
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = ext;
			ofn.nFilterIndex = 1;
			ofn.lpstrFile = filename;
			ofn.lpstrTitle = (tag==NULL)?szSaveAudioFile:szConvertAudioFile;
			ofn.lpstrDefExt = NULL;
			ofn.nMaxFile = maxfilename;
			ofn.lpstrFileTitle = NULL;
			ofn.lpstrInitialDir = szDirName;
			if (tag!=NULL)
			{
				ofn.lpfnHook=(LPOFNHOOKPROC)ConvertFileDlgHook;
				ofn.hInstance=hInst;
				ofn.lpTemplateName="ConvertFileDlg";
			}
			ofn.Flags = OFN_NODEREFERENCELINKS |
						OFN_PATHMUSTEXIST |
					    OFN_SHAREAWARE |
						OFN_OVERWRITEPROMPT |
						OFN_EXPLORER |
						OFN_HIDEREADONLY |
						((tag!=NULL)?(OFN_ENABLEHOOK | OFN_ENABLETEMPLATE):0);
			if (!GetSaveFileName(&ofn))
			{
				if (CommDlgExtendedError()==FNERR_INVALIDFILENAME)
				{
					ofn.lpstrFile = NULL;
					if (!GetSaveFileName(&ofn))
						return FALSE;
				}
				else
					return FALSE;
			}
			AppendDefExt(filename,ext);
			if (tag!=NULL)
				*tag=dlgWaveTag;
			break;
		case ID_ASKTEMPLATE:
			if (!templateReady)
			{
				if (!DialogBoxParam(hInst,"FileNameTemplateDialog",hwnd,TemplateDlgProc,(LPARAM)(tag!=NULL)))
				{
					SetCancelAllFlag();
					return FALSE;
				}
				templateReady=TRUE;
			}
			flag=FALSE;
			do {
				fileNameNum++;
				if (!flag)
				{
					if (lstrcmp(dlgTemplateFileTitle,AFTITLE))
						flag=TRUE;
					else
						fileNameNum--;
				}
				ComposeSaveFileName(filename,node,(flag)?fileNameNum:(-1),dlgTemplateDir,dlgTemplateFileTitle,dlgTemplateFileExt);
				AppendDefExt(filename,ext);
				flag=TRUE;
			} while (FileExists(filename)); // add-on num check !!!
			if (tag!=NULL)
				*tag=dlgWaveTag;
			break;
		case ID_USETEMPLATE:
			flag=FALSE;
			do {
				fileNameNum++;
				if (!flag)
				{
					if (lstrcmp(defTemplateFileTitle,AFTITLE))
						flag=TRUE;
					else
						fileNameNum--;
				}
				ComposeSaveFileName(filename,node,(flag)?fileNameNum:(-1),defTemplateDir,defTemplateFileTitle,defTemplateFileExt);
				AppendDefExt(filename,ext);
				flag=TRUE;
			} while (FileExists(filename)); // add-on num check !!!
			if (tag!=NULL)
				*tag=waveTag;
			break;
		default: // ???
			break;
	}
	return TRUE;
}

void Saver(HWND hwnd)
{
	HWND   pwnd;
	char   szFile[MAX_PATH],dir[MAX_PATH],*ext;
    AFile *node;
	int    index,selcount,isel;

	if (playList==NULL)
		return;

	if ((selcount=ListView_GetSelectedCount(playList))==1)
	{
		node=GetListViewNode(GetCurrentSelection());
		if (node==NULL)
			return;
		ext=(char*)LocalAlloc(LPTR,MAX_FILTER);
		if (ext==NULL)
			return;
		lstrcpy(ext,"");
		StrCatEx(ext,(AFPLUGIN(node)!=NULL)?(AFPLUGIN(node)->afExtensions):NULL);
		StrCatEx(ext,"All Files (*.*)\0*.*\0");
		GetCurrentDirectory(sizeof(dir),dir);
		SetCurrentDirectory(szAppDirectory);
		if (GetSaveFileNameEx(hwnd,
							  ID_ASKFILENAME,
							  ext,
							  node,
							  szFile,
							  sizeof(szFile),
							  NULL
							 )
		   )
		{
			pwnd=OpenProgress(hwnd,PDT_SINGLE,"Saving Audio File");
			SaveNode(pwnd,node,szFile);
			CloseProgress();
		}
		SetCurrentDirectory(dir);
		LocalFree(ext);
    }
	else if (selcount>1)
	{
		ResetSaveFileNameEx();
		isel=0;
		ext=(char*)LocalAlloc(LPTR,MAX_FILTER);
		if (ext==NULL)
			return;
		pwnd=OpenProgress(hwnd,PDT_DOUBLE,"Saving Audio Files");
		ShowMasterProgressHeaderMsg("MultiSaving...");
		ShowMasterProgress(isel,selcount);
		index=-1;
		GetCurrentDirectory(sizeof(dir),dir);
		SetCurrentDirectory(szAppDirectory);
		while ((index=ListView_GetNextItem(playList,index,LVNI_ALL | LVNI_SELECTED))!=-1)
		{
			if (IsAllCancelled())
				break;
			node=GetListViewNode(index);
			lstrcpy(ext,"");
			StrCatEx(ext,(AFPLUGIN(node)!=NULL)?(AFPLUGIN(node)->afExtensions):NULL);
			StrCatEx(ext,"All Files (*.*)\0*.*\0");
			if (GetSaveFileNameEx(pwnd,
								  multiSaveFileName,
								  ext,
								  node,
								  szFile,
								  sizeof(szFile),
								  NULL
								 )
			   )
			{
				ResetCancelFlag();
				SaveNode(pwnd,node,szFile);
			}
			ShowMasterProgress(++isel,selcount);
		}
		SetCurrentDirectory(dir);
		CloseProgress();
		LocalFree(ext);
	}
    UpdateWholeWindow(playList);
	SetFocus(hwnd);
}

void Converter(HWND hwnd)
{
	HWND   pwnd;
	WORD   tag;
	char   szFile[MAX_PATH],dir[MAX_PATH];
	char   ext[]="Microsoft Waveform Files (*.WAV)\0*.wav\0All Files (*.*)\0*.*\0";
    AFile *node;
	int    index,selcount,isel;

	if (playList==NULL)
		return;

	if ((selcount=ListView_GetSelectedCount(playList))==1)
	{
		node=GetListViewNode(GetCurrentSelection());
		if (node==NULL)
			return;
		GetCurrentDirectory(sizeof(dir),dir);
		SetCurrentDirectory(szAppDirectory);
		if (GetSaveFileNameEx(hwnd,
							  ID_ASKFILENAME,
							  ext,
							  node,
							  szFile,
							  sizeof(szFile),
							  &tag
							 )
		   )
		{
			pwnd=OpenProgress(hwnd,PDT_SINGLE,"Converting Audio File");
			ConvertNode(pwnd,node,szFile,tag);
			CloseProgress();
		}
		SetCurrentDirectory(dir);
    }
	else if (selcount>1)
	{
		ResetSaveFileNameEx();
		isel=0;
		pwnd=OpenProgress(hwnd,PDT_DOUBLE,"Converting Audio Files");
		ShowMasterProgressHeaderMsg("MultiConverting...");
		ShowMasterProgress(isel,selcount);
		index=-1;
		GetCurrentDirectory(sizeof(dir),dir);
		SetCurrentDirectory(szAppDirectory);
		while ((index=ListView_GetNextItem(playList,index,LVNI_ALL | LVNI_SELECTED))!=-1)
		{
			if (IsAllCancelled())
				break;
			node=GetListViewNode(index);
			if (GetSaveFileNameEx(pwnd,
								  multiSaveFileName,
								  ext,
								  node,
								  szFile,
								  sizeof(szFile),
								  &tag
								 )
			   )
			{
				ResetCancelFlag();
				ConvertNode(pwnd,node,szFile,tag);
			}
			ShowMasterProgress(++isel,selcount);
		}
		SetCurrentDirectory(dir);
		CloseProgress();
	}
    UpdateWholeWindow(playList);
	SetFocus(hwnd);
}
