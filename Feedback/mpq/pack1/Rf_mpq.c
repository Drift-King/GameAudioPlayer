/*
 * MPQ Plug-in source code
 *
 * Copyright (C) 1999 ANX Software
 * E-mail: anxsoftware@avn.mccme.ru
 *
 * Author: Asatur V. Nazarian (samael@avn.mccme.ru)
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
#include <commdlg.h>
#include <commctrl.h>

#include "..\plugins.h"

#include "resource.h"

#define IDSTR_MPQHEADER "MPQ\x1A"

typedef struct tagMPQHandle
{
    DWORD     hMPQ,hFile;
    BOOL      eof;
	HINSTANCE hDllStorm;
	BOOL  (__stdcall *SFileOpenFileEx)(DWORD hMPQ, const char *fname, DWORD flags, DWORD *phFILE);
	BOOL  (__stdcall *SFileCloseFile)(DWORD hFILE);
	DWORD (__stdcall *SFileGetFileSize)(DWORD hFILE, DWORD flags);
	DWORD (__stdcall *SFileSetFilePointer)(DWORD hFILE, LONG pos, DWORD, DWORD method);
	BOOL  (__stdcall *SFileReadFile)(DWORD hFILE, void *buffer, DWORD size, DWORD p4, DWORD p5);
	BOOL  (__stdcall *SFileCloseArchive)(DWORD hMPQ);
	BOOL  (__stdcall *SFileOpenArchive)(const char *fname, DWORD p2, DWORD p3, DWORD* phMPQ);
	BOOL  (__stdcall *SFileDestroy)(void);
} MPQHandle;

#define MPQHANDLE(rf) ((MPQHandle*)((rf)->rfHandleData))

RFPlugin Plugin;

const char stormName[]="storm.dll";
HINSTANCE  hDllStorm;

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

typedef struct tagIntOption {
    char* str;
    int   value;
} IntOption;

char stormPath[MAX_PATH];
int  mpqFileList;
BOOL opAskSelect,
	 opCheckExt,
	 opCheckHeader;

IntOption mpqFileListOptions[]={
    {"default", ID_USEDEFLIST},
    {"ask",	ID_ASKLIST}
};
/*
static BOOL checkMPQ(HANDLE hFile)
{
    char   buffer[16];
    DWORD  temp;
    BOOL   isMPQ = FALSE;
    WORD   exeSignature = 0x5A4D;

    // Read block
    ReadFile(hFile, buffer, sizeof(buffer), &temp, NULL);

    // Check MPQ signature
    if(!memcmp(buffer, "MPQ", 3))
        return TRUE;

    // Check EXE signature
    if(*(WORD *)buffer == exeSignature)
    {
        IMAGE_DOS_HEADER      idh;
        IMAGE_FILE_HEADER     ifh;
        IMAGE_OPTIONAL_HEADER ioh;
        IMAGE_SECTION_HEADER  ish;
        DWORD  filePos;
        DWORD  endOfExe;
        int    i;

        // Read DOS header
        filePos = SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        ReadFile(hFile, &idh, sizeof(IMAGE_DOS_HEADER), &temp, NULL);

        // Read new EXE header
        filePos = SetFilePointer(hFile, idh.e_lfanew + sizeof(DWORD), 0, FILE_BEGIN);
        ReadFile(hFile, &ifh, sizeof(IMAGE_FILE_HEADER), &temp, NULL);
        if(ifh.SizeOfOptionalHeader == 0)
            return FALSE;

        // Read image file header
        filePos = SetFilePointer(hFile, filePos + sizeof(IMAGE_FILE_HEADER), NULL, FILE_BEGIN);
        ReadFile(hFile, &ioh, sizeof(IMAGE_OPTIONAL_HEADER), &temp, NULL);
        if(ioh.Magic != 0x010B)
            return FALSE;

        // Search through all EXE sections
        SetFilePointer(hFile, filePos + sizeof(IMAGE_OPTIONAL_HEADER), NULL, FILE_BEGIN);
        for(i = 0; i < ifh.NumberOfSections; i++)
            ReadFile(hFile, &ish, sizeof(IMAGE_SECTION_HEADER), &temp, NULL);

        // Calculate end of EXE file
        endOfExe = ish.SizeOfRawData + ish.PointerToRawData;

        // Try to read
        memset(buffer, 0, sizeof(buffer));
        SetFilePointer(hFile, endOfExe, NULL, FILE_BEGIN);
        ReadFile(hFile, buffer, sizeof(buffer), &temp, NULL);

        // If nothing has readed, FALSE.
        if(temp == 0)
            return FALSE;

        // Check MPQ signature
        if(!memcmp(buffer, "MPQ", 3))
            return TRUE;
    }
    // Not found any MPQ
    return FALSE;
}
*/

char* GetFileTitleEx(const char* path)
{
    char* name;

    name=strrchr(path,'\\');
    if (name==NULL)
		name=strrchr(path,'/');
	if (name==NULL)
		return (char *)path;
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

void SetDefDir(void)
{
    char path[MAX_PATH];

    ReplaceModuleFileTitle(path,sizeof(path),".\\");
    SetCurrentDirectory(path);
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

BOOL FreeStorm(MPQHandle* mpqh)
{
	if (mpqh==NULL)
		return TRUE;
	if (mpqh->hDllStorm==NULL)
		return TRUE;

	return (
			(mpqh->SFileDestroy()) &&
			(FreeLibrary(mpqh->hDllStorm))
		   );
}

BOOL GetStormFunctions(MPQHandle* mpqh)
{
	if (mpqh==NULL)
		return FALSE;
	if (mpqh->hDllStorm==NULL)
		return FALSE;

    mpqh->SFileOpenFileEx=(BOOL (__stdcall *)(DWORD, const char*, DWORD, DWORD*))GetProcAddress(mpqh->hDllStorm,szSFileOpenFileEx);
    mpqh->SFileCloseFile=(BOOL (__stdcall *)(DWORD))GetProcAddress(mpqh->hDllStorm,szSFileCloseFile);
    mpqh->SFileGetFileSize=(DWORD (__stdcall *)(DWORD, DWORD))GetProcAddress(mpqh->hDllStorm,szSFileGetFileSize);
    mpqh->SFileSetFilePointer=(DWORD (__stdcall *)(DWORD, LONG, DWORD, DWORD))GetProcAddress(mpqh->hDllStorm,szSFileSetFilePointer);
    mpqh->SFileReadFile=(BOOL (__stdcall *)(DWORD, void*, DWORD, DWORD, DWORD))GetProcAddress(mpqh->hDllStorm,szSFileReadFile);
    mpqh->SFileCloseArchive=(BOOL (__stdcall *)(DWORD))GetProcAddress(mpqh->hDllStorm,szSFileCloseArchive);
    mpqh->SFileOpenArchive=(BOOL (__stdcall *)(const char*, DWORD, DWORD, DWORD*))GetProcAddress(mpqh->hDllStorm,szSFileOpenArchive);
    mpqh->SFileDestroy=(BOOL (__stdcall *)(void))GetProcAddress(mpqh->hDllStorm,szSFileDestroy);

	if (
		(mpqh->SFileOpenFileEx==NULL) ||
		(mpqh->SFileCloseFile==NULL) ||
		(mpqh->SFileGetFileSize==NULL) ||
		(mpqh->SFileSetFilePointer==NULL) ||
		(mpqh->SFileReadFile==NULL) ||
		(mpqh->SFileCloseArchive==NULL) ||
		(mpqh->SFileOpenArchive==NULL) ||
		(mpqh->SFileDestroy==NULL)
       )
	{
		mpqh->SFileOpenFileEx=(BOOL (__stdcall *)(DWORD, const char*, DWORD, DWORD*))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileOpenFileEx);
		mpqh->SFileCloseFile=(BOOL (__stdcall *)(DWORD))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileCloseFile);
		mpqh->SFileGetFileSize=(DWORD (__stdcall *)(DWORD, DWORD))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileGetFileSize);
		mpqh->SFileSetFilePointer=(DWORD (__stdcall *)(DWORD, LONG, DWORD, DWORD))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileSetFilePointer);
		mpqh->SFileReadFile=(BOOL (__stdcall *)(DWORD, void*, DWORD, DWORD, DWORD))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileReadFile);
		mpqh->SFileCloseArchive=(BOOL (__stdcall *)(DWORD))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileCloseArchive);
		mpqh->SFileOpenArchive=(BOOL (__stdcall *)(const char*, DWORD, DWORD, DWORD*))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileOpenArchive);
		mpqh->SFileDestroy=(BOOL (__stdcall *)(void))GetProcAddress(mpqh->hDllStorm,(LPCSTR)ordSFileDestroy);

		if (
			(mpqh->SFileOpenFileEx==NULL) ||
			(mpqh->SFileCloseFile==NULL) ||
			(mpqh->SFileGetFileSize==NULL) ||
			(mpqh->SFileSetFilePointer==NULL) ||
			(mpqh->SFileReadFile==NULL) ||
			(mpqh->SFileCloseArchive==NULL) ||
			(mpqh->SFileOpenArchive==NULL) ||
			(mpqh->SFileDestroy==NULL)
		   )
			return FALSE;
	}
}

void __stdcall Init(void)
{
    char str[40];

    if (Plugin.szINIFileName==NULL)
    {
		lstrcpy(stormPath,"Storm.dll");
		mpqFileList=ID_ASKLIST;
		opAskSelect=TRUE;
		opCheckExt=TRUE;
		opCheckHeader=FALSE;
		return;
    }
    GetPrivateProfileString(Plugin.Description,"STORM.DLL","Storm.dll",stormPath,sizeof(stormPath),Plugin.szINIFileName);
    mpqFileList=GetIntOption("MPQFileList",mpqFileListOptions,2,0);
    GetPrivateProfileString(Plugin.Description,"AskSelect","on",str,40,Plugin.szINIFileName);
    opAskSelect=(lstrcmpi(str,"on"))?FALSE:TRUE;
    GetPrivateProfileString(Plugin.Description,"CheckExt","on",str,40,Plugin.szINIFileName);
    opCheckExt=(lstrcmpi(str,"on"))?FALSE:TRUE;
    GetPrivateProfileString(Plugin.Description,"CheckHeader","off",str,40,Plugin.szINIFileName);
    opCheckHeader=(lstrcmpi(str,"on"))?FALSE:TRUE;
	hDllStorm=LoadStorm(stormPath);
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

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;
    WritePrivateProfileString(Plugin.Description,"STORM.DLL",stormPath,Plugin.szINIFileName);
    WriteIntOption("MPQFileList",mpqFileListOptions,2,mpqFileList);
    WritePrivateProfileString(Plugin.Description,"AskSelect",(opAskSelect)?"on":"off",Plugin.szINIFileName);
    WritePrivateProfileString(Plugin.Description,"CheckExt",(opCheckExt)?"on":"off",Plugin.szINIFileName);
    WritePrivateProfileString(Plugin.Description,"CheckHeader",(opCheckHeader)?"on":"off",Plugin.szINIFileName);
	FreeLibrary(hDllStorm);
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

const char infotext[]=
	"This plug-in wouldn't be possible without the help of the following people whom I'd like to thank:\n"
	"(brainspin@hanmail.net)\n"
	"For the STORMING source code,\n"
	"Ted Lyngmo (ted@lyncon.se)\n"
	"For the StarCrack mailing list,\n"
	"King Arthur (KingArthur@warzone.com),\n"
	"Unknown Mnemonic (zorohack@hotmail.com)\n"
	"For sending me the filelists for many MPQ-games,\n"
	"Sean Mims (smims@hotmail.com),\n"
	"David \"Splice\" James (beta@rogers.wave.ca)\n"
	"For the invaluable help with the STORM.DLL API.";

LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetDlgItemText(hwnd,ID_INFOTEXT,infotext);
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
					//return DefWindowProc(hwnd,umsg,wparm,lparm);
					break;
			}
			break;
		default:
			//return DefWindowProc(hwnd,umsg,wparm,lparm);
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

BOOL GetFileName(char* name, HWND hwnd, const char* title, const char* ext)
{
    OPENFILENAME ofn={0};
    char szDirName[MAX_PATH];
    char szFile[MAX_PATH];
    char szFileTitle[MAX_PATH];

    GetCurrentDirectory(sizeof(szDirName), szDirName);
    szFile[0]='\0';
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = ext;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.lpstrTitle=title;
    ofn.lpstrDefExt=NULL;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFileTitle = szFileTitle;
    ofn.nMaxFileTitle = sizeof(szFileTitle);
    ofn.lpstrInitialDir = szDirName;
    ofn.Flags = OFN_NODEREFERENCELINKS |
				OFN_SHAREAWARE |
				OFN_PATHMUSTEXIST |
				OFN_FILEMUSTEXIST |
				OFN_EXPLORER |
				OFN_READONLY;

    if (GetOpenFileName(&ofn))
    {
		lstrcpy(name,ofn.lpstrFile);
		return TRUE;
    }
    else
		return FALSE;
}

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char path[MAX_PATH];

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetDlgItemText(hwnd,ID_PATH,stormPath);
			CheckRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST,mpqFileList);
			SetCheckBox(hwnd,ID_ASKSELECT,opAskSelect);
			SetCheckBox(hwnd,ID_CHECKEXT,opCheckExt);
			SetCheckBox(hwnd,ID_CHECKHEADER,opCheckHeader);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetDlgItemText(hwnd,ID_PATH,"Storm.dll");
					CheckRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST,ID_ASKLIST);
					SetCheckBox(hwnd,ID_ASKSELECT,TRUE);
					SetCheckBox(hwnd,ID_CHECKEXT,TRUE);
					SetCheckBox(hwnd,ID_CHECKHEADER,FALSE);
					break;
				case ID_BROWSE:
					SetDefDir();
					if (GetFileName(path,hwnd,"Path to STORM.DLL","Dynamic Link Libraries (*.DLL)\0*.dll\0All Files (*.*)\0*.*\0"))
						SetDlgItemText(hwnd,ID_PATH,path);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					SendDlgItemMessage(hwnd,ID_PATH,WM_GETTEXT,(WPARAM)sizeof(stormPath),(LPARAM)stormPath);
					mpqFileList=GetRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST);
					opAskSelect=GetCheckBox(hwnd,ID_ASKSELECT);
					opCheckExt=GetCheckBox(hwnd,ID_CHECKEXT);
					opCheckHeader=GetCheckBox(hwnd,ID_CHECKHEADER);
					FreeLibrary(hDllStorm);
					hDllStorm=LoadStorm(stormPath);
					EndDialog(hwnd,TRUE);
					break;
				default:
					//return DefWindowProc(hwnd,uMsg,wParam,lParam);
					break;
			}
			break;
		default:
			//return DefWindowProc(hwnd,uMsg,wParam,lParam);
			break;
    }
    return FALSE;
}

void __stdcall Config(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"Config",hwnd,ConfigDlgProc);
}

char fileListPath[MAX_PATH];

void GetDefFileListName(char* name, DWORD size, const char* mpq)
{
    ReplaceModuleFileTitle(name,size,GetFileTitleEx(mpq));
    CutFileExtension(name);
    lstrcat(name,".txt");
}

LRESULT CALLBACK FileListDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char str[MAX_PATH+100];

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			wsprintf(str,"Specify filelist file for %s",GetFileTitleEx((const char*)lParam));
			SetWindowText(hwnd,str);
			GetDefFileListName(fileListPath,sizeof(fileListPath),(const char*)lParam);
			SetDlgItemText(hwnd,ID_PATH,fileListPath);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_BROWSE:
					SetDefDir();
					if (GetFileName(str,hwnd,"Select MPQ filelist file","Text Files (*.TXT)\0*.txt\0All Files (*.*)\0*.*\0"))
						SetDlgItemText(hwnd,ID_PATH,str);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					SendDlgItemMessage(hwnd,ID_PATH,WM_GETTEXT,(WPARAM)sizeof(fileListPath),(LPARAM)fileListPath);
					EndDialog(hwnd,TRUE);
					break;
				default:
					//return DefWindowProc(hwnd,uMsg,wParam,lParam);
					break;
			}
			break;
		default:
			//return DefWindowProc(hwnd,uMsg,wParam,lParam);
			break;
    }
    return FALSE;
}

void AddRNode(RFile** first, RFile** last, const char* str)
{
    RFile* rnode;

    rnode=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
    lstrcpy(rnode->rfDataString,str);
    rnode->next=NULL;
    if ((*first)==NULL)
		*first=rnode;
    else
		(*last)->next=rnode;
    (*last)=rnode;
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

RFile *list;

LRESULT CALLBACK MPQSelectDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int index,num,pt[1],*selitems,selnum;
    static int fullnum=0;
    BOOL selected;
    char str[MAX_PATH+100],*resname,*newline,ext[6];
    static RFile *tmplist=NULL,*last;
    RFile *rnode;
    FILE *listfile;
    RECT  rect;
    LONG  pwidth;
	MPQHandle mpq;

    switch (uMsg)
    {
		case WM_INITDIALOG:
			resname=(char*)lParam;
			mpq.hDllStorm=LoadStorm(stormPath);
			if (mpq.hDllStorm==NULL)
			{
				EndDialog(hwnd,FALSE);
				return FALSE;
			}
			if (!GetStormFunctions(&mpq))
			{
				FreeLibrary(mpq.hDllStorm);
				EndDialog(hwnd,FALSE);
				return FALSE;
			}
			if (!(mpq.SFileOpenArchive(resname,0,0,&(mpq.hMPQ))))
			{
				FreeStorm(&mpq);
				EndDialog(hwnd,FALSE);
				return FALSE;
			}
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			wsprintf(str,"Select files to process from archive: %s",GetFileTitleEx(resname));
			SetWindowText(hwnd,str);
			GetClientRect(GetDlgItem(hwnd,ID_PROGRESS),&rect);
			pwidth=rect.right-rect.left;
			GetClientRect(GetDlgItem(hwnd,ID_STATUS),&rect);
			pt[0]=rect.right-pwidth-6;
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETPARTS,sizeof(pt)/sizeof(pt[0]),(LPARAM)pt);
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETRANGE,0,MAKELPARAM(0,100));
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Trying to open archive...");
			tmplist=NULL;
			last=NULL;
			ShowWindow(hwnd,SW_SHOWNORMAL);
			if (mpqFileList==ID_USEDEFLIST)
				GetDefFileListName(fileListPath,sizeof(fileListPath),resname);
			else
			{
				if (!DialogBoxParam(Plugin.hDllInst,"FileList",GetFocus(),(DLGPROC)FileListDlgProc,(LPARAM)resname))
				{
					mpq.SFileCloseArchive(mpq.hMPQ);
					FreeStorm(&mpq);
					EndDialog(hwnd,FALSE);
					return FALSE;
				}
			}
			listfile=fopen(fileListPath,"rt");
			if (listfile==NULL)
			{
				mpq.SFileCloseArchive(mpq.hMPQ);
				FreeStorm(&mpq);
				EndDialog(hwnd,FALSE);
				return FALSE;
			}
			fseek(listfile,0,SEEK_SET);
			SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)"*.*");
			SendDlgItemMessage(hwnd,ID_FILTER,CB_SETCURSEL,(WPARAM)0,0L);
			lstrcpy(ext,"*");
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Verifying archive files...");
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			fullnum=0;
			while (!feof(listfile))
			{
				lstrcpy(str,"\0");
				fgets(str,sizeof(str),listfile);
				if (lstrlen(str)==0)
					break;
				newline=strchr(str,'\n');
				if (newline!=NULL)
					newline[0]='\0';
				if (mpq.SFileOpenFileEx(mpq.hMPQ,str,0,&(mpq.hFile)))
				{
					AddRNode(&tmplist,&last,str);
					index=SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
					if (GetFileExtension(str)!=NULL)
					{
						ext[1]=0;
						lstrcat(ext,GetFileExtension(str));
						if (SendDlgItemMessage(hwnd,ID_FILTER,CB_FINDSTRINGEXACT,(WPARAM)(-1),(LPARAM)ext)==CB_ERR)
							SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)ext);
					}
					mpq.SFileCloseFile(mpq.hFile);
					fullnum++;
				}
				SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,ftell(listfile),fsize(listfile)),0L);
				UpdateWindow(hwnd);
			}
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
			fclose(listfile);
			mpq.SFileCloseArchive(mpq.hMPQ);
			FreeStorm(&mpq);
			return TRUE;
		case WM_CLOSE:
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Freeing list...");
			PluginFreeFileList(tmplist);
			list=NULL;
			EndDialog(hwnd,FALSE);
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
							//return DefWindowProc(hwnd,uMsg,wParam,lParam);
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
						rnode=tmplist;
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filtering list...");
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
						index=0;
						while (rnode!=NULL)
						{
							if (CheckExt(rnode->rfDataString,ext+2))
								SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,(WPARAM)0,(LPARAM)(rnode->rfDataString));
							SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,index+1,fullnum),0L);
							rnode=rnode->next;
							index++;
							UpdateWindow(hwnd);
						}
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					}
					/*else
						return DefWindowProc(hwnd,uMsg,wParam,lParam);*/
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
					PluginFreeFileList(tmplist);
					list=NULL;
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Freeing list...");
					PluginFreeFileList(tmplist);
					num=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETCOUNT,(WPARAM)0,(LPARAM)0L);
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Checking selection...");
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					list=NULL;
					last=NULL;
					for (index=0;index<num;index++)
					{
						selected=(BOOL)SendDlgItemMessage(hwnd,ID_LIST,LB_GETSEL,(WPARAM)index,0L);
						if (selected)
						{
							SendDlgItemMessage(hwnd,ID_LIST,LB_GETTEXT,(WPARAM)index,(LPARAM)str);
							AddRNode(&list,&last,str);
						}
						SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,index+1,num),0L);
					}
					EndDialog(hwnd,TRUE);
					break;
				default:
					//return DefWindowProc(hwnd,uMsg,wParam,lParam);
					break;
			}
			break;
		default:
			//return DefWindowProc(hwnd,uMsg,wParam,lParam);
			break;
    }
    return FALSE;
}

RFile* __stdcall PluginGetFileList(const char* resname)
{
    FILE *listfile;
    BOOL isOurExt;
    char str[MAX_PATH+100],*newline,*ext,*defext;
    RFile *last;
	MPQHandle mpq;

    if (opCheckExt)
    {
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
						FILE_ATTRIBUTE_READONLY,
						NULL
					   );
		if (file==INVALID_HANDLE_VALUE)
			return NULL;
		SetFilePointer(file,0,NULL,FILE_BEGIN);
		ReadFile(file,header,sizeof(header),&read,NULL);
		CloseHandle(file);
		if (memcmp(header,IDSTR_MPQHEADER,sizeof(header)))
			return NULL;
    }
    list=NULL;
    last=NULL;
    if (opAskSelect)
    {
		if (!DialogBoxParam(Plugin.hDllInst,"MPQSelect",GetFocus(),MPQSelectDlgProc,(LPARAM)resname))
			return NULL;
    }
    else
    {
		mpq.hDllStorm=LoadStorm(stormPath);
		if (mpq.hDllStorm==NULL)
			return NULL;
		if (!GetStormFunctions(&mpq))
		{
			FreeLibrary(mpq.hDllStorm);
			return NULL;
		}
		if (!(mpq.SFileOpenArchive(resname,0,0,&(mpq.hMPQ))))
		{
			FreeStorm(&mpq);
			return NULL;
		}
		if (mpqFileList==ID_USEDEFLIST)
			GetDefFileListName(fileListPath,sizeof(fileListPath),resname);
		else
		{
			if (!DialogBoxParam(Plugin.hDllInst,"FileList",GetFocus(),(DLGPROC)FileListDlgProc,(LPARAM)resname))
			{
				mpq.SFileCloseArchive(mpq.hMPQ);
				FreeStorm(&mpq);
				return NULL;
			}
		}
		listfile=fopen(fileListPath,"rt");
		if (listfile==NULL)
		{
			mpq.SFileCloseArchive(mpq.hMPQ);
			FreeStorm(&mpq);
			return NULL;
		}
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
			if (mpq.SFileOpenFileEx(mpq.hMPQ,str,0,&(mpq.hFile)))
			{
				AddRNode(&list,&last,str);
				mpq.SFileCloseFile(mpq.hFile);
			}
		}
		fclose(listfile);
		mpq.SFileCloseArchive(mpq.hMPQ);
		FreeStorm(&mpq);
    }

    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, LPCSTR rfDataString)
{
    MPQHandle* mpq;
    RFHandle* rf;

	mpq=(MPQHandle*)GlobalAlloc(GPTR,sizeof(MPQHandle));
    if (mpq==NULL)
    {
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	mpq->hDllStorm=LoadStorm(stormPath);
	if (mpq->hDllStorm==NULL)
	{
		GlobalFree(mpq);
		SetLastError(PRIVEC(IDS_NOSTORM));
		return NULL;
	}
    if (!GetStormFunctions(mpq))
	{
		FreeLibrary(mpq->hDllStorm);
		GlobalFree(mpq);
		SetLastError(PRIVEC(IDS_NOPROCS));
		return NULL;
	}
    if (!(mpq->SFileOpenArchive(resname,0,0,&(mpq->hMPQ))))
	{
		FreeStorm(mpq);
		GlobalFree(mpq);
		SetLastError(PRIVEC(IDS_OPENARCFAILED));
		return NULL;
	}
    if (!(mpq->SFileOpenFileEx(mpq->hMPQ,rfDataString,0,&(mpq->hFile))))
    {
		mpq->SFileCloseArchive(mpq->hMPQ);
		FreeStorm(mpq);
		GlobalFree(mpq);
		SetLastError(PRIVEC(IDS_OPENFILEFAILED));
		return NULL;
    }
    mpq->eof=FALSE;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		mpq->SFileCloseFile(mpq->hFile);
		mpq->SFileCloseArchive(mpq->hMPQ);
		FreeStorm(mpq);
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
    if (rf==NULL)
		return FALSE;
    if (MPQHANDLE(rf)==NULL)
		return FALSE;
    if (
		(MPQHANDLE(rf)->SFileCloseArchive==NULL) ||
		(MPQHANDLE(rf)->SFileCloseFile==NULL)
       )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return FALSE;
	}
    return (
			(MPQHANDLE(rf)->SFileCloseFile(MPQHANDLE(rf)->hFile)) &&
			(MPQHANDLE(rf)->SFileCloseArchive(MPQHANDLE(rf)->hMPQ)) &&
			(FreeStorm(MPQHANDLE(rf))) &&
			(GlobalFree(rf->rfHandleData)==NULL) &&
			(GlobalFree(rf)==NULL)
		   );
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
    if (rf==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)->SFileSetFilePointer==NULL)
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return 0xFFFFFFFF;
	}

    return (MPQHANDLE(rf)->SFileSetFilePointer(MPQHANDLE(rf)->hFile,0,0,FILE_CURRENT));
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
    if (rf==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)->SFileGetFileSize==NULL)
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return 0xFFFFFFFF;
	}

    return (MPQHANDLE(rf)->SFileGetFileSize(MPQHANDLE(rf)->hFile,0));
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
    if (rf==NULL)
		return 0xFFFFFFFF;
    if (MPQHANDLE(rf)==NULL)
		return 0xFFFFFFFF;
    if (
		(MPQHANDLE(rf)->SFileSetFilePointer==NULL) ||
		(MPQHANDLE(rf)->SFileGetFileSize==NULL)
       )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return 0xFFFFFFFF;
	}

    switch (method)
    {
		case FILE_BEGIN:
			MPQHANDLE(rf)->eof=(BOOL)(pos >= (LONG)PluginGetFileSize(rf));
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

    return (MPQHANDLE(rf)->SFileSetFilePointer(MPQHANDLE(rf)->hFile,pos,0,method));
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
    if (rf==NULL)
		return FALSE;
    if (MPQHANDLE(rf)==NULL)
		return FALSE;

    return (MPQHANDLE(rf)->eof);
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
    DWORD p;
    BOOL  retval;

    if (rf==NULL)
		return FALSE;
    if (MPQHANDLE(rf)==NULL)
		return FALSE;
    if (
		(MPQHANDLE(rf)->SFileSetFilePointer==NULL) ||
		(MPQHANDLE(rf)->SFileReadFile==NULL) ||
		(MPQHANDLE(rf)->SFileGetFileSize==NULL)
       )
	{
		SetLastError(PRIVEC(IDS_NOPROCS));
		return FALSE;
	}
    p=PluginGetFilePointer(rf);
    retval=MPQHANDLE(rf)->SFileReadFile(MPQHANDLE(rf)->hFile,lpBuffer,ToRead,(DWORD)Read,0);
    if (p+(*Read)>=PluginGetFileSize(rf))
		MPQHANDLE(rf)->eof=TRUE;

    return retval;
}

RFPlugin Plugin=
{
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX MPQ Resource File Plug-In",
    "v0.95",
    "Blizzard MPQ Archives (*.MPQ)\0*.mpq\0"
	"EXE Files (may contain MPQ) (*.EXE)\0*.exe\0"
	"SNP Files (*.SNP)\0*.snp\0"
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
