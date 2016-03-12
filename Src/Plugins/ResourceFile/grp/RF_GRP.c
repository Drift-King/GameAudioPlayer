/*
 * GRP plug-in source code
 * (3D Realms resources: Duke Nukem 3D, Blood, Redneck Rampage)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code is based on the DN3D-DC written by 
 * Daniel Mecklenburg Jr (codewar@ns.cencom.net)
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

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "..\RFPlugin.h"

#include "RF_GRP.h"
#include "resource.h"

typedef struct tagGRPHandle
{
	HANDLE grp;
	DWORD  start,size;
} GRPHandle;

#define GRPHANDLE(rf) ((GRPHandle*)((rf)->rfHandleData))

typedef struct tagCheckedArchive
{
	char  *resname;
	HANDLE rf;
	DWORD  number;
} CheckedArchive;

BOOL opAskSelect,
	 opAddIndex;

RFPlugin Plugin;

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
    char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opAskSelect=TRUE;
		opAddIndex=TRUE;
		return;
    }
    GetPrivateProfileString(Plugin.Description,"AskSelect","on",str,40,Plugin.szINIFileName);
    opAskSelect=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"AddIndex","on",str,40,Plugin.szINIFileName);
    opAddIndex=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

    WritePrivateProfileString(Plugin.Description,"AskSelect",SaveOptionBool(opAskSelect),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"AddIndex",SaveOptionBool(opAddIndex),Plugin.szINIFileName);
}

BOOL CenterWindow(HWND hwndChild, HWND hwndParent)
{
    RECT rcWorkArea,rcChild,rcParent;
    int  cxChild,cyChild,cxParent,cyParent;
    int  xNew,yNew;

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
						xNew,yNew,
						0,0,
						SWP_NOSIZE | SWP_NOZORDER);
}

LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
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

void __stdcall PluginAbout(HWND hwnd)
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

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetCheckBox(hwnd,ID_ASKSELECT,opAskSelect);
			SetCheckBox(hwnd,ID_ADDINDEX,opAddIndex);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_ASKSELECT,TRUE);
					SetCheckBox(hwnd,ID_ADDINDEX,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opAskSelect=GetCheckBox(hwnd,ID_ASKSELECT);
					opAddIndex=GetCheckBox(hwnd,ID_ADDINDEX);
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

void __stdcall PluginConfig(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"Config",hwnd,ConfigDlgProc);
}

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

void AddNode(RFile** first, RFile** last, RFile* node)
{
	node->next=NULL;
    if ((*first)==NULL)
		*first=node;
    else
		(*last)->next=node;
    (*last)=node;
}

void AddFile(RFile** first, RFile** last, DWORD i, GRPDirEntry *pentry)
{
	RFile* node;

	node=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
	if (opAddIndex)
		wsprintf(node->rfDataString,"%lX/",i);
	else
		lstrcpy(node->rfDataString,"");
	lstrcpyn(node->rfDataString+lstrlen(node->rfDataString),pentry->fileName,13);
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

char* GetFileExtension(const char* filename)
{
	char *ch;

	ch=strrchr(filename,'.');
	if ((ch>strrchr(filename,'\\')) && (ch>strrchr(filename,'/')))
		return ch;
	else
		return NULL;
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

DWORD CheckArchive(HANDLE file)
{
	DWORD     read;
	GRPHeader grphdr;

	SetFilePointer(file,0,NULL,FILE_BEGIN);
	ReadFile(file,&grphdr,sizeof(GRPHeader),&read,NULL);
	if (
		(read<sizeof(GRPHeader)) ||
	    (memcmp(grphdr.id,IDSTR_KenSilverman,12))
	   )
		return 0L;
	else
		return grphdr.numFiles;
}

void FillListBox(HWND hwnd, GRPDirEntry *table, DWORD fullnum, char* filter)
{
	DWORD i;
	int   index;
	char  str[MAX_PATH+200],name[13],ext[6];

	if (filter==NULL)
		lstrcpy(ext,"*");
	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
	SendDlgItemMessage(hwnd,ID_LIST,LB_RESETCONTENT,0,0L);
	for (i=0;i<fullnum;i++)
	{
		lstrcpyn(name,table[i].fileName,13);
		if (filter==NULL)
		{
			wsprintf(str,"%-12s\t%ld KB",name,table[i].fileSize/1024);
			index=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
			if (index!=LB_ERR)
				SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)i);
			if (GetFileExtension(name)!=NULL)
			{
				ext[1]=0;
				lstrcat(ext,GetFileExtension(name));
				if (SendDlgItemMessage(hwnd,ID_FILTER,CB_FINDSTRINGEXACT,(WPARAM)(-1),(LPARAM)ext)==CB_ERR)
					SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)ext);
			}
		}
		else if (CheckExt(name,filter+2))
		{
			wsprintf(str,"%-12s\t%ld KB",name,table[i].fileSize/1024);
			index=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
			if (index!=LB_ERR)
				SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)i);
		}
		SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,i+1,fullnum),0L);
		UpdateWindow(hwnd);
	}
	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
}

LRESULT CALLBACK SelectDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int    index,pt[1],*selitems,num,selnum;
    char   str[MAX_PATH+200],ext[6];
    RECT   rect;
    LONG   pwidth;
	DWORD  i,read;
	RFile *list,*last;

	static CheckedArchive *pca;
	static GRPDirEntry    *table;

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
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETPARTS,1,(LPARAM)pt);
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETRANGE,0,MAKELPARAM(0,100));
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			GetClientRect(GetDlgItem(hwnd,ID_LIST),&rect);
			pt[0]=(rect.right-rect.left)/4;
			SendDlgItemMessage(hwnd,ID_LIST,LB_SETTABSTOPS,1,(LPARAM)pt);
			SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)"*.*");
			SendDlgItemMessage(hwnd,ID_FILTER,CB_SETCURSEL,(WPARAM)0,0L);
			ShowWindow(hwnd,SW_SHOWNORMAL);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Allocating memory for list...");
			SendDlgItemMessage(hwnd,ID_LIST,LB_INITSTORAGE,(WPARAM)(pca->number),(LPARAM)(pca->number*25));
			table=(GRPDirEntry*)GlobalAlloc(GPTR,(pca->number)*sizeof(GRPDirEntry));
			if (table==NULL)
			{
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Reading GRP file directory...");
			ReadFile(pca->rf,table,(pca->number)*sizeof(GRPDirEntry),&read,NULL);
			if (read<(pca->number)*sizeof(GRPDirEntry))
			{
				GlobalFree(table);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filling file list...");
			FillListBox(hwnd,table,pca->number,NULL);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
			return TRUE;
		case WM_CLOSE:
			GlobalFree(table);
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
						SendDlgItemMessage(hwnd,ID_FILTER,CB_GETLBTEXT,(WPARAM)index,(LPARAM)ext);
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filtering list...");
						FillListBox(hwnd,table,(pca->number),ext);
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
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
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
					LocalFree(selitems);
					break;
				case IDCANCEL:
					GlobalFree(table);
					EndDialog(hwnd,(int)NULL);
					break;
				case IDOK:
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Checking selection...");
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					selnum=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_GETSELCOUNT,0,0L);
					selitems=(int*)LocalAlloc(LPTR,selnum*sizeof(int));
					SendDlgItemMessage(hwnd,ID_LIST,LB_GETSELITEMS,(WPARAM)selnum,(LPARAM)selitems);
					list=NULL;
					last=NULL;
					for (index=0;index<selnum;index++)
					{
						i=(DWORD)SendDlgItemMessage(hwnd,ID_LIST,LB_GETITEMDATA,(WPARAM)selitems[index],0L);
						if (i!=LB_ERR)
						{
							AddFile(&list,&last,i,table+i);
							SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,index+1,selnum),0L);
						}
					}
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					LocalFree(selitems);
					GlobalFree(table);
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
	HANDLE rf;
	DWORD  read,i,number;
    RFile *list,*last;
	GRPDirEntry entry;

	rf=CreateFile(resname,
				  GENERIC_READ,
				  FILE_SHARE_READ,
				  NULL,
				  OPEN_EXISTING,
				  FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
				  NULL
				 );
	if (rf==INVALID_HANDLE_VALUE)
		return NULL;
	number=CheckArchive(rf);
	if (number==0L)
	{
		CloseHandle(rf);
		return NULL;
	}
    if (opAskSelect)
	{
		CheckedArchive ca;

		ca.resname=(char*)resname;
		ca.rf=rf;
		ca.number=number;

		list=(RFile*)DialogBoxParam(Plugin.hDllInst,"Select",GetActiveWindow(),SelectDlgProc,(LPARAM)&ca);
	}
    else
    {
		list=NULL;
		last=NULL;
		for (i=0;i<number;i++)
		{
			ReadFile(rf,&entry,sizeof(GRPDirEntry),&read,NULL);
			if (read<sizeof(GRPDirEntry))
				break; // ???
			AddFile(&list,&last,i,&entry);
		}
    }
	CloseHandle(rf);

    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, const char* rfDataString)
{
    HANDLE grp;
	DWORD  read,i,k,number,start,size;
    RFHandle* rf;
	GRPDirEntry entry;
	GRPHandle* grph;
	char *slash,name[13],*endptr;

    grp=CreateFile(resname,
				   GENERIC_READ,
				   FILE_SHARE_READ,
				   NULL,
				   OPEN_EXISTING,
				   FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
				   NULL
				  );
    if (grp==INVALID_HANDLE_VALUE)
		return NULL;
	number=CheckArchive(grp);
	if (number==0L)
	{
		CloseHandle(grp);
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return NULL;
	}
	slash=strchr(rfDataString,'/');
	if (slash==NULL)
	{
		lstrcpy(name,rfDataString);
		if (lstrlen(name)<=13)
			memset(name+lstrlen(name),0,13-lstrlen(name));
		for (i=0;i<number;i++)
		{
			ReadFile(grp,&entry,sizeof(GRPDirEntry),&read,NULL);
			if (read<sizeof(GRPDirEntry))
				break; // ???
			if (!memcmp(entry.fileName,name,12))
				break;
		}
	}
	else
	{
		lstrcpy(name,slash+1);
		if (lstrlen(name)<=13)
			memset(name+lstrlen(name),0,13-lstrlen(name));
		i=strtoul(rfDataString,&endptr,0x10);
		if (i>=number)
		{
			CloseHandle(grp);
			SetLastError(PRIVEC(IDS_NOFILE));
			return NULL;
		}
		SetFilePointer(grp,sizeof(GRPHeader)+i*sizeof(GRPDirEntry),NULL,FILE_BEGIN);
		ReadFile(grp,&entry,sizeof(GRPDirEntry),&read,NULL);
		if (read<sizeof(GRPDirEntry))
			lstrcpyn(entry.fileName,"\0\0\0\0\0\0\0\0\0\0\0",12); // ???
	}
	if (memcmp(entry.fileName,name,12))
	{
		CloseHandle(grp);
		SetLastError(PRIVEC(IDS_NOFILE));
		return NULL;
	}
	size=entry.fileSize;
	grph=(GRPHandle*)GlobalAlloc(GPTR,sizeof(GRPHandle));
    if (grph==NULL)
    {
		CloseHandle(grp);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	grph->grp=grp;
	start=sizeof(GRPHeader)+number*sizeof(GRPDirEntry);
	SetFilePointer(grp,sizeof(GRPHeader),NULL,FILE_BEGIN);
	for (k=0;k<i;k++)
	{
		ReadFile(grp,&entry,sizeof(GRPDirEntry),&read,NULL);
		if (read<sizeof(GRPDirEntry))
		{
			GlobalFree(grph);
			CloseHandle(grp);
			SetLastError(PRIVEC(IDS_WALKERROR));
			return NULL;
		}
		start+=entry.fileSize;
	}
	grph->start=start;
	grph->size=size;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		GlobalFree(grph);
		CloseHandle(grp);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
    rf->rfPlugin=&Plugin;
    rf->rfHandleData=grph;

    return rf;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
	if (rf==NULL)
		return TRUE;
	if (GRPHANDLE(rf)==NULL)
		return FALSE;

    return (
			(CloseHandle(GRPHANDLE(rf)->grp)) &&
			(GlobalFree(GRPHANDLE(rf))==NULL) &&
			(GlobalFree(rf)==NULL)
		   );
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
	if ((rf==NULL) || (GRPHANDLE(rf)==NULL))
		return ((DWORD)(-1));

    return (SetFilePointer(GRPHANDLE(rf)->grp,0,NULL,FILE_CURRENT)-(GRPHANDLE(rf)->start));
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
	DWORD curpos=-1;

	if ((rf==NULL) || (GRPHANDLE(rf)==NULL))
		return ((DWORD)(-1));

	switch (method)
	{
		case FILE_BEGIN:
			curpos=SetFilePointer(GRPHANDLE(rf)->grp,GRPHANDLE(rf)->start+pos,NULL,FILE_BEGIN)-(GRPHANDLE(rf)->start);
			break;
		case FILE_CURRENT:
			curpos=SetFilePointer(GRPHANDLE(rf)->grp,pos,NULL,FILE_CURRENT)-(GRPHANDLE(rf)->start);
			break;
		case FILE_END:
			curpos=SetFilePointer(GRPHANDLE(rf)->grp,GRPHANDLE(rf)->start+GRPHANDLE(rf)->size+pos,NULL,FILE_BEGIN)-(GRPHANDLE(rf)->start);
			break;
		default: // ???
			break;
	}

    return curpos;
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
	if ((rf==NULL) || (GRPHANDLE(rf)==NULL))
		return ((DWORD)(-1));

    return GRPHANDLE(rf)->size;
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
	if ((rf==NULL) || (GRPHANDLE(rf)==NULL))
		return TRUE;

    return (PluginGetFilePointer(rf)>=PluginGetFileSize(rf));
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
	if (Read!=NULL)
		*Read=0L;
	if ((rf==NULL) || (GRPHANDLE(rf)==NULL))
		return FALSE;

    return ReadFile(GRPHANDLE(rf)->grp,lpBuffer,min(ToRead,PluginGetFileSize(rf)-PluginGetFilePointer(rf)),Read,NULL);
}

RFPlugin Plugin=
{
	RFP_VER,
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX GRP Resource File Plug-In",
    "v0.90",
    "3D Realms GRP Resource Files (*.GRP)\0*.grp\0",
    "GRP",
    PluginConfig,
    PluginAbout,
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
