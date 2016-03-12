/*
 * MPQ Plug-in source code
 *
 * Copyright (C) 1999 ANX Software
 * E-mail: anxsoftware@avn.mccme.ru
 *
 * Author: Asatur V. Nazarian (samael@avn.mccme.ru)
 * StormLib Author Ladik (zezula@volny.cz)
 * Optimization    Ladik (zezula@volny.cz)
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
#include "storm.h"

#define FILEHANDLE(rf) (HANDLE)((rf)->rfHandleData)

extern RFPlugin Plugin;

typedef struct tagIntOption {
    char* str;
    int   value;
} IntOption;

//char stormPath[MAX_PATH];
int  mpqFileList;
BOOL opAskSelect,
	 opCheckExt,
	 opCheckHeader;

IntOption mpqFileListOptions[]={
    {"default", ID_USEDEFLIST},
    {"ask",	ID_ASKLIST}
};

//-----------------------------------------------------------------------------
// Optimization data. The old version opened and closed archive file each time
// when a file was open. But this takes much time on MPQ archiver which are not
// at the begin of the file StormLib must srerch the file for MPQ header each
// time if any file is open.

static TMPQArchive * ha = NULL;

//-----------------------------------------------------------------------------


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

void __stdcall Init(void)
{
    char str[40];

    if (Plugin.szINIFileName==NULL)
    {
		mpqFileList=ID_ASKLIST;
		opAskSelect=TRUE;
		opCheckExt=TRUE;
		opCheckHeader=FALSE;
		return;
    }
    mpqFileList=GetIntOption("MPQFileList",mpqFileListOptions,2,0);
    GetPrivateProfileString(Plugin.Description,"AskSelect","on",str,40,Plugin.szINIFileName);
    opAskSelect=(lstrcmpi(str,"on"))?FALSE:TRUE;
    GetPrivateProfileString(Plugin.Description,"CheckExt","on",str,40,Plugin.szINIFileName);
    opCheckExt=(lstrcmpi(str,"on"))?FALSE:TRUE;
    GetPrivateProfileString(Plugin.Description,"CheckHeader","off",str,40,Plugin.szINIFileName);
    opCheckHeader=(lstrcmpi(str,"on"))?FALSE:TRUE;
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
    WriteIntOption("MPQFileList",mpqFileListOptions,2,mpqFileList);
    WritePrivateProfileString(Plugin.Description,"AskSelect",(opAskSelect)?"on":"off",Plugin.szINIFileName);
    WritePrivateProfileString(Plugin.Description,"CheckExt",(opCheckExt)?"on":"off",Plugin.szINIFileName);
    WritePrivateProfileString(Plugin.Description,"CheckHeader",(opCheckHeader)?"on":"off",Plugin.szINIFileName);

    // If archive is still open, close it.
    SFileCloseArchive(ha);
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
	"For the invaluable help with the STORM.DLL API,\n"
    "Ladik (zezula@volny.cz) for StormLib source files.";

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
    DialogBox(Plugin.hDllInst, "About", hwnd, (DLGPROC)AboutDlgProc);
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
					mpqFileList=GetRadioButton(hwnd,ID_USEDEFLIST,ID_ASKLIST);
					opAskSelect=GetCheckBox(hwnd,ID_ASKSELECT);
					opCheckExt=GetCheckBox(hwnd,ID_CHECKEXT);
					opCheckHeader=GetCheckBox(hwnd,ID_CHECKHEADER);
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
    DialogBox(Plugin.hDllInst, "Config", hwnd, (DLGPROC)ConfigDlgProc);
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

    rnode = (RFile*)GlobalAlloc(GPTR,sizeof(RFile));
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
    HANDLE hArchive;
    HANDLE hFile;
    RECT  rect;
    FILE *listfile;
    RFile *rnode;
    int index,num,pt[1],*selitems,selnum;
    static int fullnum=0;
    BOOL selected;
    char str[MAX_PATH+100],*resname,*newline,ext[6];
    static RFile *tmplist=NULL,*last;
    LONG  pwidth;

    switch (uMsg)
    {
		case WM_INITDIALOG:
			resname=(char*)lParam;
			if(!SFileOpenArchive(resname, 0, 0, &hArchive))
			{
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
					SFileCloseArchive(hArchive);
					EndDialog(hwnd,FALSE);
					return FALSE;
				}
			}
			listfile=fopen(fileListPath,"rt");
			if (listfile==NULL)
			{
				SFileCloseArchive(hArchive);
				EndDialog(hwnd,FALSE);
				return FALSE;
			}
//      	fseek(listfile,0,SEEK_SET);
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
				if(SFileOpenFileEx(hArchive, str, 0, &hFile))
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
					SFileCloseFile(hFile);
					fullnum++;
				}
				SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,ftell(listfile),fsize(listfile)),0L);
				UpdateWindow(hwnd);
			}
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
			fclose(listfile);
			SFileCloseArchive(hArchive);
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
    FILE * listfile;
    HANDLE hArchive;
    HANDLE hFile;
    BOOL   isOurExt;
    char   str[MAX_PATH+100],*newline,*ext,*defext;
    RFile *last;

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
    if(opCheckHeader)
    {
        // No code here. Checking header is not necessary. StormLib
        // does it.
    }
    list=NULL;
    last=NULL;
    if (opAskSelect)
    {
		if (!DialogBoxParam(Plugin.hDllInst,"MPQSelect",GetFocus(), (DLGPROC)MPQSelectDlgProc,(LPARAM)resname))
			return NULL;
    }
    else
    {
        // Try to open archive
        if(SFileOpenArchive((char *)resname, 0, 0, &hArchive) == FALSE)
			return NULL;

		if(mpqFileList == ID_USEDEFLIST)
			GetDefFileListName(fileListPath,sizeof(fileListPath),resname);
		else
		{
			if(!DialogBoxParam(Plugin.hDllInst,"FileList",GetFocus(),(DLGPROC)FileListDlgProc,(LPARAM)resname))
			{
				SFileCloseArchive(hArchive);
				return NULL;
			}
		}
		listfile=fopen(fileListPath,"rt");
		if (listfile==NULL)
		{
			SFileCloseArchive(hArchive);
			return NULL;
		}
//      fseek(listfile,0,SEEK_SET);
		while(!feof(listfile))
		{
			lstrcpy(str,"\0");
			fgets(str,sizeof(str),listfile);
			if (lstrlen(str)==0)
				break;
			newline=strchr(str,'\n');
			if (newline!=NULL)
				newline[0]='\0';
			if(SFileOpenFileEx(hArchive, str, 0, &hFile))
			{
				AddRNode(&list, &last, str);
				SFileCloseFile(hFile);
			}
		}
		fclose(listfile);
		SFileCloseArchive(hArchive);
    }
    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, LPCSTR rfDataString)
{
    RFHandle * rf;
    HANDLE hArchive = (HANDLE)ha;
    HANDLE hFile;

    // Optimization : Check if archive is already open and the same like required
    if(ha == NULL || stricmp(resname, ha->fileName))
    {
        // Not the same archive required or archive not open
        SFileCloseArchive(ha);
        
        // Open the new archive. If failed, return NULL.
        if(SFileOpenArchive((char *)resname, 0, 0, &hArchive) == FALSE)
        {
            SetLastError(PRIVEC(IDS_OPENARCFAILED));
            return NULL;
        }
        ha = (TMPQArchive *)hArchive;
    }
    
    // Try to open file
    if(SFileOpenFileEx(hArchive, (char *)rfDataString, 0, &hFile) == TRUE)
    {
        // Try to allocate memory for resource file handle    
        if((rf = new RFHandle) != NULL)
        {
            rf->rfPlugin=&Plugin;
            rf->rfHandleData = hFile;

            return rf;
        }
        else
            SetLastError(PRIVEC(IDS_NOMEM));
        SFileCloseFile(hFile);
    }
    else
        SetLastError(PRIVEC(IDS_OPENFILEFAILED));

    return NULL;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
    HANDLE     hArchive;
    TMPQFile * hFile = (TMPQFile *)(rf->rfHandleData);

    // Check the right parameters
    if(hFile == NULL)
		return FALSE;

    // Archive handle is stored in file handle, but will not be accessible after
    // Close file.
    hArchive = hFile->hArchive;

    delete rf;

    // Don't close archive here !!
    return (SFileCloseFile(hFile) == TRUE);
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return (DWORD)-1;

    return SFileSetFilePointer(FILEHANDLE(rf), 0, NULL, FILE_CURRENT);
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return (DWORD)-1;

    return SFileGetFileSize(FILEHANDLE(rf), NULL);
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
    DWORD filePtr;                      // New file pointer

    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return (DWORD)-1;

    // Move to the new position and check if at the end of file
    filePtr = SFileSetFilePointer(FILEHANDLE(rf), pos, NULL, method);
    
    return filePtr;
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
    DWORD fileSize;
    DWORD filePtr;

    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return FALSE;

    fileSize = SFileGetFileSize(FILEHANDLE(rf));
    filePtr  = SFileSetFilePointer(FILEHANDLE(rf), 0, NULL, FILE_CURRENT);

    return (BOOL)(filePtr >= fileSize);
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return FALSE;

    // Read file
    return SFileReadFile(FILEHANDLE(rf), (BYTE *)lpBuffer, ToRead, Read);
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


// Trick for right function export name 
extern "C"
{
    __declspec(dllexport) RFPlugin* __stdcall GetResourceFilePlugin(void)
    {
        return &Plugin;
    }
}
