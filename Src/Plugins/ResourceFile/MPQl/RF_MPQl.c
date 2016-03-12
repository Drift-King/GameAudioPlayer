/*
 * MPQ plug-in (using Storm.dll and filelists) source code
 * (Blizzard Entertainment resources: Diablo, StarCraft, HellFire, BroodWar)
 *
 * Copyright (C) 1999-2000 ANX Software
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
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>

#include "..\RFPlugin.h"

#include "RF_MPQl.h"
#include "resource.h"

typedef struct tagMPQHandle
{
    DWORD     hMPQ,hFile;
    BOOL      eof;
} MPQHandle;

#define MPQHANDLE(rf) ((MPQHandle*)((rf)->rfHandleData))

typedef struct tagCheckedArchive
{
	char  *resname;
	DWORD  hMPQ;
	FILE  *listfile;
} CheckedArchive;

RFPlugin Plugin;

const char stormName[]="storm.dll";

HINSTANCE  hDllStorm;
LONG       stormUsed;
BOOL       stormChanged;

BOOL  (__stdcall *SFileOpenFileEx)(DWORD hMPQ, const char *fname, DWORD flags, DWORD *phFILE);
BOOL  (__stdcall *SFileCloseFile)(DWORD hFILE);
DWORD (__stdcall *SFileGetFileSize)(DWORD hFILE, DWORD flags);
DWORD (__stdcall *SFileSetFilePointer)(DWORD hFILE, LONG pos, DWORD, DWORD method);
BOOL  (__stdcall *SFileReadFile)(DWORD hFILE, void *buffer, DWORD size, DWORD p4, DWORD p5);
BOOL  (__stdcall *SFileCloseArchive)(DWORD hMPQ);
BOOL  (__stdcall *SFileOpenArchive)(const char *fname, DWORD p2, DWORD p3, DWORD* phMPQ);
BOOL  (__stdcall *SFileDestroy)(void);

const char szSFileOpenFileEx[]="SFileOpenFileEx";
const char szSFileCloseFile[]="SFileCloseFile";
const char szSFileGetFileSize[]="SFileGetFileSize";
const char szSFileSetFilePointer[]="SFileSetFilePointer";
const char szSFileReadFile[]="SFileReadFile";
const char szSFileCloseArchive[]="SFileCloseArchive";
const char szSFileOpenArchive[]="SFileOpenArchive";
const char szSFileDestroy[]="SFileDestroy";

const WORD  ordSFileOpenFileEx=0x010C;
const WORD  ordSFileCloseFile=0x00FD;
const WORD  ordSFileGetFileSize=0x0109;
const WORD  ordSFileSetFilePointer=0x010F;
const WORD  ordSFileReadFile=0x010D;
const WORD  ordSFileCloseArchive=0x00FC;
const WORD  ordSFileOpenArchive=0x010A;
const WORD  ordSFileDestroy=0x0106;

typedef struct tagIntOption 
{
    char* str;
    int   value;
} IntOption;

char stormPath[MAX_PATH],listPath[MAX_PATH];
int  mpqFileList;
BOOL opAskSelect,
	 opCheckExt,
	 opCheckHeader;

IntOption mpqFileListOptions[]=
{
    {"default", ID_USEDEFLIST},
    {"ask",	ID_ASKLIST}
};

char* GetFileTitleEx(const char* path)
{
    char* name;

    name=strrchr(path,'\\');
    if (name==NULL)
		name=strrchr(path,'/');
	if (name==NULL)
		return (char*)path;
    else
		return (name+1);
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

void CorrectDirString(char* dir)
{
	if ((dir==NULL) || (dir[0]==0))
		return;
	if (dir[lstrlen(dir)-1]!='\\')
		lstrcat(dir,"\\");
}

char* ReplaceModuleFileTitle(char* str, DWORD size, const char* rstr)
{
	GetModuleFileName(Plugin.hDllInst,str,size);
	lstrcpy(GetFileTitleEx(str),rstr);

	return str;
}

void GetDefStormPath(char* path, DWORD size)
{
    ReplaceModuleFileTitle(path,size,stormName);
}

void GetDefListPath(char* path, DWORD size)
{
    ReplaceModuleFileTitle(path,size,"MPQLists\\");
}

int GetIntOption(const char* key, IntOption* pio, int num, int def)
{
    int i;
    char str[512];

    GetPrivateProfileString(Plugin.Description,key,pio[def].str,str,sizeof(str),Plugin.szINIFileName);
    for (i=0;i<num;i++)
		if (!lstrcmp(pio[i].str,str))
			return pio[i].value;

    return pio[def].value;
}

HINSTANCE LoadStorm(const char* path)
{
	char defpath[MAX_PATH];

	if (GetFileTitleEx(stormPath)!=stormPath)
		return LoadLibrary(stormPath);
	else
	{
		GetDefStormPath(defpath,sizeof(defpath));
		lstrcpy(GetFileTitleEx(defpath),stormPath);
		return LoadLibrary(defpath);
	}

}

BOOL FreeStorm(void)
{
	if (hDllStorm==NULL)
		return TRUE;
	if (SFileDestroy==NULL)
		return FALSE;

	return (
			(SFileDestroy()) &&
			(FreeLibrary(hDllStorm))
		   );
}

BOOL GetStormFunctions(HINSTANCE dll)
{
	if (dll==NULL)
		return FALSE;

	SFileOpenFileEx=NULL;
	SFileCloseFile=NULL;
	SFileGetFileSize=NULL;
	SFileSetFilePointer=NULL;
	SFileReadFile=NULL;
	SFileCloseArchive=NULL;
	SFileOpenArchive=NULL;
	SFileDestroy=NULL;

    SFileOpenFileEx=(BOOL (__stdcall *)(DWORD, const char*, DWORD, DWORD*))GetProcAddress(hDllStorm,szSFileOpenFileEx);
    SFileCloseFile=(BOOL (__stdcall *)(DWORD))GetProcAddress(hDllStorm,szSFileCloseFile);
    SFileGetFileSize=(DWORD (__stdcall *)(DWORD, DWORD))GetProcAddress(hDllStorm,szSFileGetFileSize);
    SFileSetFilePointer=(DWORD (__stdcall *)(DWORD, LONG, DWORD, DWORD))GetProcAddress(hDllStorm,szSFileSetFilePointer);
    SFileReadFile=(BOOL (__stdcall *)(DWORD, void*, DWORD, DWORD, DWORD))GetProcAddress(hDllStorm,szSFileReadFile);
    SFileCloseArchive=(BOOL (__stdcall *)(DWORD))GetProcAddress(hDllStorm,szSFileCloseArchive);
    SFileOpenArchive=(BOOL (__stdcall *)(const char*, DWORD, DWORD, DWORD*))GetProcAddress(hDllStorm,szSFileOpenArchive);
    SFileDestroy=(BOOL (__stdcall *)(void))GetProcAddress(hDllStorm,szSFileDestroy);

	if (
		(SFileOpenFileEx==NULL) ||
		(SFileCloseFile==NULL) ||
		(SFileGetFileSize==NULL) ||
		(SFileSetFilePointer==NULL) ||
		(SFileReadFile==NULL) ||
		(SFileCloseArchive==NULL) ||
		(SFileOpenArchive==NULL) ||
		(SFileDestroy==NULL)
       )
	{
		SFileOpenFileEx=(BOOL (__stdcall *)(DWORD, const char*, DWORD, DWORD*))GetProcAddress(hDllStorm,(LPCSTR)ordSFileOpenFileEx);
		SFileCloseFile=(BOOL (__stdcall *)(DWORD))GetProcAddress(hDllStorm,(LPCSTR)ordSFileCloseFile);
		SFileGetFileSize=(DWORD (__stdcall *)(DWORD, DWORD))GetProcAddress(hDllStorm,(LPCSTR)ordSFileGetFileSize);
		SFileSetFilePointer=(DWORD (__stdcall *)(DWORD, LONG, DWORD, DWORD))GetProcAddress(hDllStorm,(LPCSTR)ordSFileSetFilePointer);
		SFileReadFile=(BOOL (__stdcall *)(DWORD, void*, DWORD, DWORD, DWORD))GetProcAddress(hDllStorm,(LPCSTR)ordSFileReadFile);
		SFileCloseArchive=(BOOL (__stdcall *)(DWORD))GetProcAddress(hDllStorm,(LPCSTR)ordSFileCloseArchive);
		SFileOpenArchive=(BOOL (__stdcall *)(const char*, DWORD, DWORD, DWORD*))GetProcAddress(hDllStorm,(LPCSTR)ordSFileOpenArchive);
		SFileDestroy=(BOOL (__stdcall *)(void))GetProcAddress(hDllStorm,(LPCSTR)ordSFileDestroy);

		if (
			(SFileOpenFileEx==NULL) ||
			(SFileCloseFile==NULL) ||
			(SFileGetFileSize==NULL) ||
			(SFileSetFilePointer==NULL) ||
			(SFileReadFile==NULL) ||
			(SFileCloseArchive==NULL) ||
			(SFileOpenArchive==NULL) ||
			(SFileDestroy==NULL)
		   )
			return FALSE;
	}

	return TRUE;
}

void InitStorm(void)
{
	if (stormChanged)
	{
		if (stormUsed==0)
		{
			if (!FreeStorm())
				MessageBox(GetFocus(),"Failed to free STORM library.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
			hDllStorm=LoadStorm(stormPath);
			if (hDllStorm==NULL)
				MessageBox(GetFocus(),"Failed to load STORM library.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
			else
			{
				if (!GetStormFunctions(hDllStorm))
					MessageBox(GetFocus(),"Failed to get STORM functions.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
			}
			stormChanged=FALSE;
		}
	}
}

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
    char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		lstrcpy(stormPath,"Storm.dll");
		GetDefListPath(listPath,sizeof(listPath));
		mpqFileList=ID_ASKLIST;
		opAskSelect=TRUE;
		opCheckExt=TRUE;
		opCheckHeader=FALSE;
		return;
    }
    GetPrivateProfileString(Plugin.Description,"STORM.DLL","Storm.dll",stormPath,sizeof(stormPath),Plugin.szINIFileName);
	GetDefListPath(listPath,sizeof(listPath));
	GetPrivateProfileString(Plugin.Description,"ListPath",listPath,listPath,sizeof(listPath),Plugin.szINIFileName);
	CorrectDirString(listPath);
    mpqFileList=GetIntOption("MPQFileList",mpqFileListOptions,sizeof(mpqFileListOptions)/sizeof(IntOption),1);
    GetPrivateProfileString(Plugin.Description,"AskSelect","on",str,40,Plugin.szINIFileName);
    opAskSelect=LoadOptionBool(str);
    GetPrivateProfileString(Plugin.Description,"CheckExt","on",str,40,Plugin.szINIFileName);
    opCheckExt=LoadOptionBool(str);
    GetPrivateProfileString(Plugin.Description,"CheckHeader","off",str,40,Plugin.szINIFileName);
    opCheckHeader=LoadOptionBool(str);
	hDllStorm=NULL;
	stormUsed=0;
	stormChanged=TRUE;
	InitStorm();
}

void WriteIntOption(const char* key, IntOption* pio, int num, int value)
{
    int i;

    for (i=0;i<num;i++)
		if (pio[i].value==value)
		{
			WritePrivateProfileString(Plugin.Description,key,pio[i].str,Plugin.szINIFileName);
			return;
		}
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
	if (!FreeStorm())
		MessageBox(GetFocus(),"Failed to free STORM library.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);

	SFileOpenFileEx=NULL;
	SFileCloseFile=NULL;
	SFileGetFileSize=NULL;
	SFileSetFilePointer=NULL;
	SFileReadFile=NULL;
	SFileCloseArchive=NULL;
	SFileOpenArchive=NULL;
	SFileDestroy=NULL;
	stormUsed=0;
	stormChanged=FALSE;
	
    if (Plugin.szINIFileName==NULL)
		return;

    WritePrivateProfileString(Plugin.Description,"STORM.DLL",stormPath,Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"ListPath",listPath,Plugin.szINIFileName);
    WriteIntOption("MPQFileList",mpqFileListOptions,sizeof(mpqFileListOptions)/sizeof(IntOption),mpqFileList);
    WritePrivateProfileString(Plugin.Description,"AskSelect",SaveOptionBool(opAskSelect),Plugin.szINIFileName);
    WritePrivateProfileString(Plugin.Description,"CheckExt",SaveOptionBool(opCheckExt),Plugin.szINIFileName);
    WritePrivateProfileString(Plugin.Description,"CheckHeader",SaveOptionBool(opCheckHeader),Plugin.szINIFileName);
}

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

LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;

    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			hres=FindResource(Plugin.hDllInst,(LPCTSTR)IDR_CREDITS,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(Plugin.hDllInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_CREDITS,(res!=NULL)?res:"No info available.");
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,TRUE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case IDOK:
					EndDialog(hwnd,TRUE);
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

void __stdcall About(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"About",hwnd,AboutDlgProc);
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

BOOL GetFileName(HWND hwnd, char* name, DWORD size, const char* title, const char* ext)
{
	BOOL retval;
    OPENFILENAME ofn={0};
    char szDirName[MAX_PATH];

	if (GetFileTitleEx(name)==name)
		ReplaceModuleFileTitle(szDirName,sizeof(szDirName),"");
	else
	{
		lstrcpy(szDirName,name);
		lstrcpy(name,GetFileTitleEx(szDirName));
		*(GetFileTitleEx(szDirName))=0;
	}
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = ext;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = name;
    ofn.lpstrTitle= title;
    ofn.lpstrDefExt= NULL;
    ofn.nMaxFile = size;
    ofn.lpstrFileTitle = NULL;
    ofn.lpstrInitialDir = szDirName;
    ofn.Flags = OFN_NODEREFERENCELINKS |
				OFN_SHAREAWARE |
				OFN_PATHMUSTEXIST |
				OFN_FILEMUSTEXIST |
				OFN_EXPLORER |
				OFN_READONLY |
				OFN_NOCHANGEDIR;

	retval=GetOpenFileName(&ofn);

	return retval;
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

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BROWSEINFO   bi;
	LPITEMIDLIST lpidl;
    char         path[MAX_PATH];

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetDlgItemText(hwnd,ID_STORMPATH,stormPath);
			SetDlgItemText(hwnd,ID_LISTPATH,listPath);
			CheckRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST,mpqFileList);
			SetCheckBox(hwnd,ID_ASKSELECT,opAskSelect);
			SetCheckBox(hwnd,ID_CHECKEXT,opCheckExt);
			SetCheckBox(hwnd,ID_CHECKHEADER,opCheckHeader);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetDlgItemText(hwnd,ID_STORMPATH,"Storm.dll");
					GetDefListPath(path,sizeof(path));
					SetDlgItemText(hwnd,ID_LISTPATH,path);
					CheckRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST,ID_ASKLIST);
					SetCheckBox(hwnd,ID_ASKSELECT,TRUE);
					SetCheckBox(hwnd,ID_CHECKEXT,TRUE);
					SetCheckBox(hwnd,ID_CHECKHEADER,FALSE);
					break;
				case ID_BROWSE_STORM:
					SendDlgItemMessage(hwnd,ID_STORMPATH,WM_GETTEXT,(WPARAM)sizeof(path),(LPARAM)path);
					if (GetFileName(hwnd,path,sizeof(path),"Specify STORM library to use","Dynamic Link Libraries (*.DLL)\0*.dll\0All Files (*.*)\0*.*\0"))
						SetDlgItemText(hwnd,ID_STORMPATH,path);
					break;
				case ID_BROWSE_LIST:
					SendDlgItemMessage(hwnd,ID_LISTPATH,WM_GETTEXT,(WPARAM)sizeof(path),(LPARAM)path);
					bi.hwndOwner=hwnd;
					bi.pidlRoot=NULL;
					bi.pszDisplayName=path;
					bi.lpszTitle="Select MPQ file lists directory:";
					bi.ulFlags=BIF_RETURNONLYFSDIRS;
					bi.lpfn=BrowseProc;
					if (path[lstrlen(path)-1]=='\\')
						path[lstrlen(path)-1]=0;
					bi.lParam=(LPARAM)path;
					if ((lpidl=SHBrowseForFolder(&bi))!=NULL)
					{
						SHGetPathFromIDList(lpidl,path);
						CorrectDirString(path);
						SetDlgItemText(hwnd,ID_LISTPATH,path);
					}
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					SendDlgItemMessage(hwnd,ID_STORMPATH,WM_GETTEXT,(WPARAM)sizeof(stormPath),(LPARAM)stormPath);
					SendDlgItemMessage(hwnd,ID_LISTPATH,WM_GETTEXT,(WPARAM)sizeof(listPath),(LPARAM)listPath);
					CorrectDirString(listPath);
					mpqFileList=GetRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST);
					opAskSelect=GetCheckBox(hwnd,ID_ASKSELECT);
					opCheckExt=GetCheckBox(hwnd,ID_CHECKEXT);
					opCheckHeader=GetCheckBox(hwnd,ID_CHECKHEADER);
					stormChanged=TRUE;
					InitStorm();
					EndDialog(hwnd,TRUE);
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

void __stdcall Config(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"Config",hwnd,ConfigDlgProc);
}

void GetDefFileListName(char* name, const char* mpq)
{
	char title[MAX_PATH];

    lstrcpy(title,GetFileTitleEx(mpq));
    CutFileExtension(title);
    lstrcat(title,".txt");
	lstrcpy(name,listPath);
	lstrcat(name,title);
}

void AddNode(RFile** first, RFile** last, RFile* node)
{
	node->next=NULL;
    if ((*first)==NULL)
		*first=node;
    else
		(*last)->next=node;
    (*last)=node;
}

void AddFile(RFile** first, RFile** last, const char* str)
{
	RFile* node;

	node=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
	lstrcpy(node->rfDataString,str);
	AddNode(first,last,node);
}

BOOL __stdcall PluginFreeFileList(RFile* list)
{
    RFile *rnode,*tnode;

    rnode=list;
    while (rnode!=NULL)
    {
		tnode=rnode->next;
		GlobalFree(rnode);
		rnode=tnode;
    }

    return TRUE;
}

BOOL CheckExt(const char* str, const char* ext)
{
    char* point;

    if (!lstrcmpi(ext,"*"))
		return TRUE;
    point=strrchr(str,'.');
    if (point==NULL)
		return (!lstrcmpi(ext,""));
    return (!lstrcmpi(point+1,ext));
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

LRESULT CALLBACK SelectDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int    index,num,pt[1],*selitems,selnum;
    BOOL   selected;
    char   str[MAX_PATH+100],*newline,ext[6];
    RFile *last,*cnode,*snode,*tnode;
    RECT   rect;
    LONG   pwidth;
	DWORD  hFile;
	
	CheckedArchive *pca;
	static RFile   *list;
	static int      fullnum=0;

    switch (uMsg)
    {
		case WM_INITDIALOG:
			pca=(CheckedArchive*)lParam;
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			wsprintf(str,"Select files to process from archive: %s",GetFileTitleEx(pca->resname));
			SetWindowText(hwnd,str);
			GetClientRect(GetDlgItem(hwnd,ID_PROGRESS),&rect);
			pwidth=rect.right-rect.left;
			GetClientRect(GetDlgItem(hwnd,ID_STATUS),&rect);
			pt[0]=rect.right-pwidth-6;
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETPARTS,sizeof(pt)/sizeof(pt[0]),(LPARAM)pt);
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETRANGE,0,MAKELPARAM(0,100));
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Trying to open archive...");
			list=NULL;
			last=NULL;
			SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)"*.*");
			SendDlgItemMessage(hwnd,ID_FILTER,CB_SETCURSEL,(WPARAM)0,0L);
			lstrcpy(ext,"*");
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Verifying archive files...");
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			ShowWindow(hwnd,SW_SHOWNORMAL);
			fullnum=0;
			fseek(pca->listfile,0,SEEK_SET);
			while (!feof(pca->listfile))
			{
				lstrcpy(str,"\0");
				fgets(str,sizeof(str),pca->listfile);
				if (lstrlen(str)==0)
					break;
				newline=strchr(str,'\n');
				if (newline!=NULL)
					newline[0]='\0';
				if (SFileOpenFileEx(pca->hMPQ,str,0,&hFile))
				{
					AddFile(&list,&last,str);
					index=SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
					SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)(DWORD)last);
					if (GetFileExtension(str)!=NULL)
					{
						ext[1]=0;
						lstrcat(ext,GetFileExtension(str));
						if (SendDlgItemMessage(hwnd,ID_FILTER,CB_FINDSTRINGEXACT,(WPARAM)(-1),(LPARAM)ext)==CB_ERR)
							SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)ext);
					}
					SFileCloseFile(hFile);
					fullnum++;
				}
				SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,ftell(pca->listfile),fsize(pca->listfile)),0L);
				UpdateWindow(hwnd);
			}
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
			return TRUE;
		case WM_CLOSE:
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Freeing list...");
			PluginFreeFileList(list);
			EndDialog(hwnd,(int)NULL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_LIST:
					switch (HIWORD(wParam))
					{
						case LBN_DBLCLK:
						case LBN_KILLFOCUS:
						case LBN_SETFOCUS:
						case LBN_SELCANCEL:
						case LBN_SELCHANGE:
							// TODO: ???
							break;
						default:
							break;
					}
					break;
				case ID_FILTER:
					if (HIWORD(wParam)==CBN_SELCHANGE)
					{
						index=(int)SendDlgItemMessage(hwnd,ID_FILTER,CB_GETCURSEL,(WPARAM)0,0L);
						if (index==CB_ERR)
							break;
						SendDlgItemMessage(hwnd,ID_LIST,LB_RESETCONTENT,(WPARAM)0,(LPARAM)0L);
						SendDlgItemMessage(hwnd,ID_FILTER,CB_GETLBTEXT,(WPARAM)index,(LPARAM)ext);
						cnode=list;
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filtering list...");
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
						num=0;
						while (cnode!=NULL)
						{
							if (CheckExt(cnode->rfDataString,ext+2))
							{
								index=SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,(WPARAM)0,(LPARAM)(cnode->rfDataString));
								SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)(DWORD)cnode);
							}
							SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,++num,fullnum),0L);
							cnode=cnode->next;
							UpdateWindow(hwnd);
						}
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					}
					break;
				case ID_SELALL:
					num=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETCOUNT,(WPARAM)0,(LPARAM)0L);
					SendDlgItemMessage(hwnd,ID_LIST,LB_SELITEMRANGE,(WPARAM)TRUE,MAKELPARAM(0,num-1));
					break;
				case ID_SELNONE:
					num=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETCOUNT,(WPARAM)0,(LPARAM)0L);
					SendDlgItemMessage(hwnd,ID_LIST,LB_SELITEMRANGE,(WPARAM)FALSE,MAKELPARAM(0,num-1));
					break;
				case ID_INVERT:
					selnum=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETSELCOUNT,0,0L);
					selitems=(int*)LocalAlloc(LPTR,selnum*sizeof(int));
					SendDlgItemMessage(hwnd,ID_LIST,LB_GETSELITEMS,(WPARAM)selnum,(LPARAM)selitems);
					num=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETCOUNT,(WPARAM)0,(LPARAM)0L);
					SendDlgItemMessage(hwnd,ID_LIST,LB_SELITEMRANGE,(WPARAM)TRUE,MAKELPARAM(0,num-1));
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Inverting selection...");
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					for (index=0;index<selnum;index++)
					{
						SendDlgItemMessage(hwnd,ID_LIST,LB_SETSEL,(WPARAM)FALSE,(LPARAM)selitems[index]);
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,index+1,selnum),0L);
					}
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					LocalFree(selitems);
					break;
				case IDCANCEL:
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Freeing list...");
					PluginFreeFileList(list);
					EndDialog(hwnd,(int)NULL);
					break;
				case IDOK:
					num=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETCOUNT,(WPARAM)0,(LPARAM)0L);
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Checking selection...");
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					cnode=list;
					list=NULL;
					last=NULL;
					for (index=0;index<num;index++)
					{
						selected=(BOOL)SendDlgItemMessage(hwnd,ID_LIST,LB_GETSEL,(WPARAM)index,0L);
						if (selected)
						{
							snode=(RFile*)SendDlgItemMessage(hwnd,ID_LIST,LB_GETITEMDATA,(WPARAM)index,0L);
							while ((cnode!=snode) && (cnode!=NULL))
							{
								tnode=cnode;
								cnode=cnode->next;
								GlobalFree(tnode);
							}
							if (cnode!=NULL)
								cnode=cnode->next;
							AddNode(&list,&last,snode);
						}
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,index+1,num),0L);
					}
					PluginFreeFileList(cnode);
					EndDialog(hwnd,(int)list);
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

RFile* __stdcall PluginGetFileList(const char* resname)
{
    FILE  *listfile;
    char   filelist[MAX_PATH],str[MAX_PATH+100],*newline;
    RFile *list,*last;
	DWORD  hMPQ,hFile;

    if (opCheckExt)
    {
		BOOL  isOurExt;
		char *ext,*defext;

		ext=GetFileExtension(resname);
		defext=Plugin.rfExtensions;
		if (defext==NULL)
			return NULL;
		isOurExt=FALSE;
		while (defext[0]!='\0')
		{
			defext+=lstrlen(defext)+2;
			if (!lstrcmpi(ext,defext))
			{
				isOurExt=TRUE;
				break;
			}
			defext+=lstrlen(defext)+1;
		}
		if (!isOurExt)
			return NULL;
	}
    if (opCheckHeader)
    {
		HANDLE file;
		DWORD  read;
		char   header[4];

		file=CreateFile(resname,
						GENERIC_READ,
						FILE_SHARE_READ,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
						NULL
					   );
		if (file==INVALID_HANDLE_VALUE)
			return NULL;
		SetFilePointer(file,0,NULL,FILE_BEGIN);
		ReadFile(file,header,sizeof(header),&read,NULL);
		CloseHandle(file);
		if (
			(read<sizeof(header)) ||
			(memcmp(header,IDSTR_MPQHEADER,sizeof(header)))
		   )
			return NULL;
    }
	InitStorm();
	if (hDllStorm==NULL)
		return NULL;
	if (
		(SFileOpenArchive==NULL) ||
		(SFileCloseArchive==NULL) ||
		(SFileOpenFileEx==NULL) ||
		(SFileCloseFile==NULL)
	   )
		return NULL;
	InterlockedIncrement(&stormUsed);
	if (!SFileOpenArchive(resname,0,0,&hMPQ))
	{
		InterlockedDecrement(&stormUsed);
		return NULL;
	}
	if (mpqFileList==ID_USEDEFLIST)
		GetDefFileListName(filelist,resname);
	else
	{
		wsprintf(str,"Specify filelist file for %s",GetFileTitleEx(resname));
		GetDefFileListName(filelist,resname);
		if (!GetFileName(GetActiveWindow(),filelist,sizeof(filelist),str,"Text Files (*.TXT)\0*.txt\0All Files (*.*)\0*.*\0"))
		{
			SFileCloseArchive(hMPQ);
			InterlockedDecrement(&stormUsed);
			return NULL;
		}
	}
	listfile=fopen(filelist,"rt");
	if (listfile==NULL)
	{
		SFileCloseArchive(hMPQ);
		InterlockedDecrement(&stormUsed);
		return NULL;
	}
    if (opAskSelect)
    {
		CheckedArchive ca;

		ca.resname=(char*)resname;
		ca.hMPQ=hMPQ;
		ca.listfile=listfile;

		list=(RFile*)DialogBoxParam(Plugin.hDllInst,"Select",GetActiveWindow(),SelectDlgProc,(LPARAM)&ca);
    }
    else
    {
		list=NULL;
		last=NULL;
		fseek(listfile,0,SEEK_SET);
		while (!feof(listfile))
		{
			lstrcpy(str,"\0");
			fgets(str,sizeof(str),listfile);
			if (lstrlen(str)==0)
				break;
			newline=strchr(str,'\n');
			if (newline!=NULL)
				newline[0]='\0';
			if (SFileOpenFileEx(hMPQ,str,0,&hFile))
			{
				AddFile(&list,&last,str);
				SFileCloseFile(hFile);
			}
		}
    }
	fclose(listfile);
	SFileCloseArchive(hMPQ);
	InterlockedDecrement(&stormUsed);

    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, const char* rfDataString)
{
	DWORD      hMPQ,hFile;
    MPQHandle *mpq;
    RFHandle  *rf;

	InitStorm();
	if (hDllStorm==NULL)
	{
		SetLastError(PRIVEC(IDS_NOSTORM));
		return NULL;
	}
    if (
		(SFileOpenArchive==NULL) ||
		(SFileCloseArchive==NULL) ||
		(SFileOpenFileEx==NULL) ||
		(SFileCloseFile==NULL)
	   )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return NULL;
	}
	InterlockedIncrement(&stormUsed);
    if (!SFileOpenArchive(resname,0,0,&hMPQ))
	{
		InterlockedDecrement(&stormUsed);
		SetLastError(PRIVEC(IDS_OPENARCFAILED));
		return NULL;
	}
    if (!SFileOpenFileEx(hMPQ,rfDataString,0,&hFile))
    {
		SFileCloseArchive(hMPQ);
		InterlockedDecrement(&stormUsed);
		SetLastError(PRIVEC(IDS_OPENFILEFAILED));
		return NULL;
    }
	mpq=(MPQHandle*)GlobalAlloc(GPTR,sizeof(MPQHandle));
    if (mpq==NULL)
    {
		SFileCloseFile(hFile);
		SFileCloseArchive(hMPQ);
		InterlockedDecrement(&stormUsed);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	mpq->hMPQ=hMPQ;
	mpq->hFile=hFile;
    mpq->eof=FALSE;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		SFileCloseFile(hFile);
		SFileCloseArchive(hMPQ);
		InterlockedDecrement(&stormUsed);
		GlobalFree(mpq);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
    rf->rfPlugin=&Plugin;
    rf->rfHandleData=mpq;

    return rf;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
	BOOL retval;

    if (rf==NULL)
		return TRUE;
    if (MPQHANDLE(rf)==NULL)
		return FALSE;
    if (
		(SFileCloseArchive==NULL) ||
		(SFileCloseFile==NULL)
       )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return FALSE;
	}
	
    retval=(
			(SFileCloseFile(MPQHANDLE(rf)->hFile)) &&
			(SFileCloseArchive(MPQHANDLE(rf)->hMPQ)) &&
			(GlobalFree(rf->rfHandleData)==NULL) &&
			(GlobalFree(rf)==NULL)
		   );
	InterlockedDecrement(&stormUsed);

	return retval;
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
    if (rf==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)==NULL)
		return 0xFFFFFFFF;
    if (SFileSetFilePointer==NULL)
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return 0xFFFFFFFF;
	}

    return (SFileSetFilePointer(MPQHANDLE(rf)->hFile,0,0,FILE_CURRENT));
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
    if (rf==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)==NULL)
		return 0xFFFFFFFF;
    if (SFileGetFileSize==NULL)
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return 0xFFFFFFFF;
	}

    return (SFileGetFileSize(MPQHANDLE(rf)->hFile,0));
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
    if (rf==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)==NULL)
		return 0xFFFFFFFF;
    if (
		(SFileSetFilePointer==NULL) ||
		(SFileGetFileSize==NULL)
       )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return 0xFFFFFFFF;
	}

    switch (method)
    {
		case FILE_BEGIN:
			MPQHANDLE(rf)->eof=(BOOL)(pos>=(LONG)PluginGetFileSize(rf));
			break;
		case FILE_CURRENT:
			MPQHANDLE(rf)->eof=(BOOL)(PluginGetFilePointer(rf)+pos>=PluginGetFileSize(rf));
			break;
		case FILE_END:
			MPQHANDLE(rf)->eof=(BOOL)(pos>=0);
			break;
		default: // ???
			break;
    }

    return (SFileSetFilePointer(MPQHANDLE(rf)->hFile,pos,0L,method));
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
    if (rf==NULL)
		return TRUE;
    if (MPQHANDLE(rf)==NULL)
		return TRUE;

    return (MPQHANDLE(rf)->eof);
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
    DWORD p;
    BOOL  retval;

	if (Read!=NULL)
		*Read=0L;
    if (rf==NULL)
		return FALSE;
    if (MPQHANDLE(rf)==NULL)
		return FALSE;
    if (
		(SFileSetFilePointer==NULL) ||
		(SFileReadFile==NULL) ||
		(SFileGetFileSize==NULL)
       )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return FALSE;
	}

    p=PluginGetFilePointer(rf);
    retval=SFileReadFile(MPQHANDLE(rf)->hFile,lpBuffer,ToRead,(DWORD)Read,0);
    if (p+(*Read)>=PluginGetFileSize(rf))
		MPQHANDLE(rf)->eof=TRUE;

    return retval;
}

RFPlugin Plugin=
{
	RFP_VER,
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX MPQ Resource File Plug-In",
    "v0.97",
    "Blizzard MPQ Archives (*.MPQ)\0*.mpq\0"
	"EXE Files (may contain MPQ) (*.EXE)\0*.exe\0"
	"Blizzard SNP Files (*.SNP)\0*.snp\0"
	"StarCraft Map Files (*.SCM)\0*.scm\0",
    "MPQ",
    Config,
    About,
    Init,
    Quit,
    PluginGetFileList,
    PluginFreeFileList,
    PluginOpenFile,
    PluginCloseFile,
    PluginGetFileSize,
    PluginGetFilePointer,
    PluginEndOfFile,
    PluginSetFilePointer,
    PluginReadFile
};

__declspec(dllexport) RFPlugin* __stdcall GetResourceFilePlugin(void)
{
    return &Plugin;
}
