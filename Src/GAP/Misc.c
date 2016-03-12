/*
 * Game Audio Player source code: miscellaneous helper functions
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
#include <string.h>
#include <windows.h>
#include <mmreg.h>
#include <msacm.h>

#include <commctrl.h>
#include <shlobj.h>

#include "Misc.h"
#include "Globals.h"
#include "Playlist.h"
#include "Progress.h"
#include "Playlist_MultiOps.h"
#include "Playlist_SaveLoad.h"
#include "Playlist_NodeOps.h"
#include "PlayDialog.h"
#include "resource.h"

#define ID_RECURSIVE (14146)

int CDDrive;

BOOL CenterWindow(HWND hwndChild, HWND hwndParent)
{
    RECT    rcWorkArea,rcChild,rcParent;
    int     cxChild,cyChild,cxParent,cyParent;
    int     xNew,yNew;

    GetWindowRect(hwndChild,&rcChild);
    cxChild=rcChild.right-rcChild.left;
    cyChild=rcChild.bottom-rcChild.top;
    SystemParametersInfo(SPI_GETWORKAREA,0,&rcWorkArea,0);
    if (hwndParent!=NULL)
    {
		GetWindowRect(hwndParent,&rcParent);
		cxParent=rcParent.right-rcParent.left;
		cyParent=rcParent.bottom-rcParent.top;
    }
    else
    {
		rcParent.left=rcWorkArea.left;
		rcParent.top=rcWorkArea.top;
		cxParent=rcWorkArea.right-rcWorkArea.left;
		cyParent=rcWorkArea.bottom-rcWorkArea.top;
    }

    xNew=rcParent.left+((cxParent-cxChild)/2);
    if (xNew<rcWorkArea.left)
		xNew=rcWorkArea.left;
    else if ((xNew+cxChild)>rcWorkArea.right)
		xNew=rcWorkArea.right-cxChild;

    yNew=rcParent.top+((cyParent-cyChild)/2);
    if (yNew<rcWorkArea.top)
		yNew=rcWorkArea.top;
    else if ((yNew+cyChild)>rcWorkArea.bottom)
		yNew=rcWorkArea.bottom-cyChild;

    return SetWindowPos(hwndChild,
						NULL,
						xNew, yNew,
						0,0,
						SWP_NOSIZE | SWP_NOZORDER);
}

char* GetTimeString(DWORD time, char* str)
{
    DWORD sec,min,hour;

    sec=time%60;
    min=(time/60)%60;
    hour=time/3600;
    wsprintf(str,"%02lu:%02lu:%02lu",hour,min,sec);

    return str;
}

char* GetShortTimeString(DWORD time, char* str)
{
    DWORD sec,min,hour;

    sec=time%60;
    min=(time/60)%60;
    hour=time/3600;
    if (hour!=0)
		wsprintf(str,"%lu:%02lu:%02lu",hour,min,sec);
    else
		wsprintf(str,"%lu:%02lu",min,sec);

    return str;
}

void UpdateWholeWindow(HWND hwnd)
{
    InvalidateRect(hwnd,NULL,TRUE);
    UpdateWindow(hwnd);
}

BOOL CorrectWndPosition(int* px, int* py)
{
    RECT rcWorkArea;
    BOOL retval=TRUE;

    SystemParametersInfo(SPI_GETWORKAREA,0,&rcWorkArea,0);
    if ((*px<rcWorkArea.left) || (*px>rcWorkArea.right))
    {
		*px=DEF_POS/*rcWorkArea.left*/;
		retval=FALSE;
    }
    if ((*py<rcWorkArea.top) || (*py>rcWorkArea.bottom))
    {
		*py=DEF_POS/*rcWorkArea.top*/;
		retval=FALSE;
    }

    return retval;
}

BOOL LoadWndPos(const char* wnd, int* px, int* py)
{
	char str[40];

    *px=(int)GetPrivateProfileInt(wnd,"PosX",DEF_POS,szINIFileName);
    *py=(int)GetPrivateProfileInt(wnd,"PosY",DEF_POS,szINIFileName);
    CorrectWndPosition(px,py);
	GetPrivateProfileString(wnd,"AlwaysOnTop","off",str,sizeof(str),szINIFileName);

	return ((BOOL)(lstrcmpi(str,"off")));
}

void LoadWndSize(const char* wnd, int* px, int* py)
{
    *px=(int)GetPrivateProfileInt(wnd,"SizeX",DEF_POS,szINIFileName);
    *py=(int)GetPrivateProfileInt(wnd,"SizeY",DEF_POS,szINIFileName);
}

void SaveWndPos(HWND hwnd, const char* wnd, BOOL ontop)
{
    RECT rect;
    char str[40];

    GetWindowRect(hwnd,&rect);
    if (CorrectWndPosition((int*)&(rect.left),(int*)&(rect.top)))
    {
		wsprintf(str,"%d",(int)rect.left);
		WritePrivateProfileString(wnd,"PosX",str,szINIFileName);
		wsprintf(str,"%d",(int)rect.top);
		WritePrivateProfileString(wnd,"PosY",str,szINIFileName);
    }
	if (ontop!=-1)
		WritePrivateProfileString(wnd,"AlwaysOnTop",(ontop)?"on":"off",szINIFileName);
}

void SaveWndSize(HWND hwnd, const char* wnd)
{
    RECT rect;
    char str[40];

    GetWindowRect(hwnd,&rect);
	wsprintf(str,"%d",(int)(rect.right-rect.left));
	WritePrivateProfileString(wnd,"SizeX",str,szINIFileName);
	wsprintf(str,"%d",(int)(rect.bottom-rect.top));
	WritePrivateProfileString(wnd,"SizeY",str,szINIFileName);
}

void LoadLastPlaylist(HWND hwnd)
{
    char playlist[MAX_PATH];

    GetPrivateProfileString("Playlist","LastPlaylist","",playlist,MAX_PATH,szINIFileName);
    if (lstrcmpi(playlist,""))
    {
		OpenProgress(hwnd,PDT_SINGLE,"Playlist loading");
		LoadPlaylist(hwnd,playlist);
		CloseProgress();
		curNode=GetNodeByIndex((int)GetPrivateProfileInt("Playlist","LastNode",0,szINIFileName));
		if (curNode==NULL)
			curNode=list_start;
		ShowStats();
    }
}

void SaveLastPlaylist(HWND hwnd)
{
    char str[40];

    WritePrivateProfileString("Playlist","LastPlaylist",list_filename,szINIFileName);
    wsprintf(str,"%d",GetNodePos(curNode));
    WritePrivateProfileString("Playlist","LastNode",str,szINIFileName);
}

int StrLenEx(LPCTSTR str)
{
    int len;

    if (str==NULL)
		return 0;

	len=0;
    while (str[len]!='\0')
		len+=lstrlen(str+len)+1;

    return len;
}

LPTSTR StrCatEx(LPTSTR str1, LPCTSTR str2)
{
    int pos1,pos2;

    if ((str1==NULL) || (str2==NULL))
		return str1;

    pos1=StrLenEx(str1);
    pos2=0;
    while (str2[pos2]!='\0')
    {
		lstrcat(str1+pos1,str2+pos2);
		pos1+=lstrlen(str2+pos2);
		str1[pos1++]=0;
		str1[pos1]=0;
		pos2+=lstrlen(str2+pos2)+1;
    }

    return str1;
}

LPTSTR StrExNum(LPTSTR str, DWORD num)
{
	if (str==NULL)
		return NULL;

	for (;num>0;num--)
	{
		if (*str==0)
			break;
		str+=lstrlen(str)+1;
	}

	return str;
}

char* GetFileTitleEx(const char* path)
{
    char *last;

	last=max(strrchr(path,'\\'),strrchr(path,'/'));
	if (last==NULL)
		return (char*)path;
    else
		return (last+1);
}

char* ReplaceAppFileTitle(char* str, DWORD size, const char* rstr)
{
	GetModuleFileName(NULL,str,size);
	lstrcpy(GetFileTitleEx(str),rstr);
	return str;
}

char* ReplaceAppFileExtension(char* str, DWORD size, const char* rstr)
{
	GetModuleFileName(NULL,str,size);
	lstrcpy(GetFileExtension(str),rstr);
	return str;
}

char* GetHomeDirectory(char* str, DWORD size)
{
	return ReplaceAppFileTitle(str,size,"");
}

HWND  checkbox;
BOOL  dirRecursive;
HFONT recursiveFont;

LRESULT CALLBACK RecursiveCheckBoxProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    switch (umsg)
    {
		case WM_DESTROY:
			dirRecursive=GetCheckBox(GetParent(hwnd),ID_RECURSIVE);
			DeleteObject(recursiveFont);
			break;
		default:
			break;
    }
    return CallWindowProc((WNDPROC)GetWindowLong(hwnd,GWL_USERDATA),hwnd,umsg,wparm,lparm);
}

int CALLBACK BrowseRecursiveProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	int     left,top,width,height;
	RECT    rect;
	LONG    oldproc;

	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			SetWindowText(hwnd,(LPCSTR)lpData);
			checkbox=GetDlgItem(hwnd,ID_RECURSIVE);
			if (checkbox!=NULL)
			{
				GetWindowRect(checkbox,&rect);
				DestroyWindow(checkbox);
				MapWindowPoints(NULL,hwnd,(LPPOINT)&rect,2);
				left=rect.left;
				top=rect.top;
				width=rect.right-rect.left;
				height=rect.bottom-rect.top;
			}
			else
			{
				left=10;
				top=10;
				width=200;
				height=20;
			}
			checkbox=CreateWindowEx(0,"Button","Recurse subdirectories",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,left,top,width,height,hwnd,(HMENU)ID_RECURSIVE,hInst,0);
			recursiveFont=CreateFont(8,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"MS Sans Serif");
			SendDlgItemMessage(hwnd,ID_RECURSIVE,WM_SETFONT,(WPARAM)recursiveFont,MAKELPARAM(TRUE,0));
			CheckDlgButton(hwnd,ID_RECURSIVE,TRUE);
			dirRecursive=TRUE;
			oldproc=SetWindowLong(checkbox,GWL_WNDPROC,(LONG)RecursiveCheckBoxProc);
			SetWindowLong(checkbox,GWL_USERDATA,oldproc);
			break;
		default:
			return 0;
	}
	return 1;
}

int CALLBACK BrowseProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			SendMessage(hwnd,BFFM_SETSELECTION,TRUE,(LPARAM)lpData);
			break;
		default:
			return 0;
	}
	return 1;
}

void CorrectDirString(char* dir)
{
	if ((dir==NULL) || (dir[0]==0))
		return;
	if (dir[lstrlen(dir)-1]!='\\')
		lstrcat(dir,"\\");
}

void CorrectExtString(char* ext)
{
	if ((ext==NULL) || (ext[0]==0))
		return;
	if (!lstrcmp(ext,DEFEXT))
		return;
	if (ext[0]=='.')
		return;
	memmove(ext+1,ext,lstrlen(ext)+1);
	ext[0]='.';
}

char* GetFileExtension(const char* filename)
{
	char *ch;

	ch=strrchr(filename,'.');
	if ((ch>strrchr(filename,'\\')) && (ch>strrchr(filename,'/')))
		return ch;
	else
		return NULL;
}

void CutFileExtension(char* filename)
{
	char *ch;

	ch=GetFileExtension(filename);
	if (ch!=NULL)
		*ch=0;
}

void CutFileTitle(char* filename)
{
	char *ch;

	ch=GetFileTitleEx(filename);
	if (ch!=NULL)
		*ch=0;
}

BOOL FileExists(const char* fname)
{
	HANDLE file;
	BOOL   retval;

	retval=(file=CreateFile(fname,0,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_READONLY,NULL))!=INVALID_HANDLE_VALUE;
	CloseHandle(file);
	return retval;
}

void CutDirString(char* dir)
{
	if (dir[lstrlen(dir)-1]=='\\')
		dir[lstrlen(dir)-1]=0;
	if (dir[lstrlen(dir)-1]=='/')
		dir[lstrlen(dir)-1]=0;
}

BOOL DirectoryExists(const char* dir)
{
	SHFILEINFO sfi;

	return SHGetFileInfo(dir,0,&sfi,sizeof(sfi),SHGFI_ATTRIBUTES);
}

BOOL CreateDirectoryRecursive(const char* dir)
{
	char str[MAX_PATH];

	if (DirectoryExists(dir))
		return TRUE;
	else
	{
		lstrcpy(str,dir);
		CutDirString(str);
		*GetFileTitleEx(str)=0;
		CreateDirectoryRecursive(str);
		return CreateDirectory(dir,NULL);
	}
}

BOOL EnsureDirPresence(const char* fname)
{
	char dir[MAX_PATH],str[MAX_PATH+100];

	lstrcpy(dir,fname);
	*GetFileTitleEx(dir)=0;
	if (!DirectoryExists(dir))
	{
		wsprintf(str,"Directory %s does not exist.\nWould you like to create it?",dir);
		switch (MessageBox(GetFocus(),str,szAppName,MB_YESNOCANCEL | MB_ICONQUESTION))
		{
			case IDYES:
				return CreateDirectoryRecursive(dir);
			case IDNO:
			default: // ???
				return FALSE;
			case IDCANCEL:
				SetCancelAllFlag();
				return FALSE;
		}
	}
	else
		return TRUE;
}

void AppendDefExt(char* filename, const char* filter)
{
	char str[MAX_PATH],*ch;

	if (
		(GetFileExtension(filename)==NULL) &&
		(filter!=NULL)
	   )
	{
		lstrcpy(str,filter+lstrlen(filter)+1);
		ch=strchr(str,';');
		if (ch!=NULL)
			*ch=0;
		ch=strchr(str,'.');
		if (ch==NULL)
			ch=str;
		lstrcat(filename,ch);
	}
}

BOOL CALLBACK acmFormatTagEnumFillComboBox
(
	HACMDRIVERID hadid,
	LPACMFORMATTAGDETAILS paftd,
	DWORD dwInstance,
	DWORD fdwSupport
)
{
	int  index;
	char str[256];

	if (paftd->dwFormatTag!=WAVE_FORMAT_PCM)
	{
		lstrcpy(str,paftd->szFormatTag);
		index=(int)SendMessage((HWND)dwInstance,CB_ADDSTRING,0,(LPARAM)str);
		if (index!=CB_ERR)
			SendMessage((HWND)dwInstance,CB_SETITEMDATA,(WPARAM)index,(LPARAM)(paftd->dwFormatTag));
	}

	return TRUE;
}

void FillACMTagsComboBox(HWND hwnd)
{
	int index;
	ACMFORMATTAGDETAILS aftd={0};

	SendMessage(hwnd,CB_RESETCONTENT,0,0L);
	index=(int)SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"PCM");
	if (index!=CB_ERR)
		SendMessage(hwnd,CB_SETITEMDATA,(WPARAM)index,(LPARAM)WAVE_FORMAT_PCM);
	aftd.cbStruct=sizeof(aftd);
	aftd.dwFormatTag=WAVE_FORMAT_UNKNOWN;
	if (acmFormatTagEnum(NULL,&aftd,(ACMFORMATTAGENUMCB)acmFormatTagEnumFillComboBox,(DWORD)hwnd,0L)!=MMSYSERR_NOERROR)
		return;
}

long fsize(FILE* f)
{
	long pos,size;

	pos=ftell(f);
	fseek(f,0L,SEEK_END);
	size=ftell(f);
	fseek(f,pos,SEEK_SET);

	return size;
}

void ClearMessageQueue(HWND hwnd, DWORD timeout)
{
	MSG   msg;
	DWORD end;

	end=GetTickCount()+timeout;
	while (
			(PeekMessage(&msg,hwnd,0,0,PM_REMOVE)) &&
			(end>GetTickCount())
		  )
	{}
}

void ProcessMessages(HWND hwnd, BOOL bUpdateParent)
{
	MSG msg;

	if (PeekMessage(&msg,hwnd,0,0,PM_REMOVE))
    {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
    }
	if (bUpdateParent)
		UpdateWindow(GetParent(hwnd));
}

LRESULT CALLBACK WaitDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			return TRUE;
		case WM_NOTIFY:
			if (((LPNMHDR)lParam)->code==NM_CLICK)
				ClearMessageQueue(hwnd,10); // ???
			break;
		default:
			break;
    }
    return FALSE;
}

HWND ShowWaitWindow(HWND hwnd, const char* title, const char* msg)
{
	HWND waitwnd;

	waitwnd=CreateDialog(hInst,"Wait",hwnd,(DLGPROC)WaitDlgProc);
	SetWindowText(waitwnd,title);
	SetDlgItemText(waitwnd,ID_WAITMSG,msg);
	ShowWindow(waitwnd,SW_SHOWNORMAL);

	return waitwnd;
}

void CloseWaitWindow(HWND hwnd)
{
	DestroyWindow(hwnd);
}

long CountFiles(char* str)
{
	long num=0;

    str+=lstrlen(str)+1;
    while (str[0]!='\0')
	{
		num++;
		str+=lstrlen(str)+1;
	}

	return num;
}

BOOL IsCDPath(const char* resname)
{
    char root[3];

    lstrcpyn(root,resname,3);

    return (BOOL)(GetDriveType(root)==DRIVE_CDROM);
}

void RefineResName(char* str, const char* resname)
{
    char *cd,root[3];

    cd=strstr(resname,CD_SIG);
    if ((cd==NULL) || ((cd!=resname) && (cd[-1]!=' ')))
		lstrcpy(str,resname);
    else
    {
		GetCDDriveRoot(root,CDDrive);
		lstrcpy(str,root);
		lstrcat(str,cd+lstrlen(CD_SIG));
    }
}

void GetCDDriveRoot(char* root, int num)
{
    char drive;
    int  i;

    root[1]=':';
    root[2]=0;
    i=0;
    for (drive='A';drive<='Z';drive++)
    {
		root[0]=drive;
		if (GetDriveType(root)==DRIVE_CDROM)
		{
			if (i==num)
				return;
			else
				i++;
		}
    }
    lstrcpy(root,"");
}

char pafDir[MAX_PATH];
WIN32_FIND_DATA pafdW32fd;

DWORD PerformActionForDir(HWND hwnd, DWORD (*Action)(HWND,const char*), BOOL recursive)
{
	HANDLE sh;
	char  *name;
	DWORD  sum;

	sum=0;
	name=pafDir+lstrlen(pafDir);
	lstrcat(pafDir,"*.*");
    sh=FindFirstFile(pafDir,&pafdW32fd);
	*name=0;
    if (sh!=INVALID_HANDLE_VALUE)
    {
		do
		{
			if (IsAllCancelled())
				break;
			if (pafdW32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if ((recursive) && (pafdW32fd.cFileName[0]!='.'))
				{
					name=pafDir+lstrlen(pafDir);
					lstrcat(pafDir,pafdW32fd.cFileName);
					lstrcat(pafDir,"\\");
					sum+=PerformActionForDir(hwnd,Action,recursive);
					*name=0;
				}
			}
			else
			{
				name=pafDir+lstrlen(pafDir);
				lstrcat(pafDir,pafdW32fd.cFileName);
				ShowProgressHeaderMsg(pafDir);
				ResetCancelFlag();
				sum+=Action(hwnd,pafDir);
				*name=0;
			}
		}
		while (FindNextFile(sh,&pafdW32fd));
		FindClose(sh);
    }

	return sum;
}

void SetCheckBox(HWND hwnd, int cbId, BOOL cbVal)
{
    CheckDlgButton(hwnd,cbId,(cbVal)?BST_CHECKED:BST_UNCHECKED);
}

BOOL GetCheckBox(HWND hwnd, int cbId)
{
    return (BOOL)(IsDlgButtonChecked(hwnd,cbId)==BST_CHECKED);
}

int GetRadioButton(HWND hwnd, int first, int last)
{
    int i;

    for (i=first;i<=last;i++)
		if (IsDlgButtonChecked(hwnd,i)==BST_CHECKED)
			return i;

    return first;
}
