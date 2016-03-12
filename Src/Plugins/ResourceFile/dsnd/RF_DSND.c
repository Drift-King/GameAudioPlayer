/*
 * DSND plug-in source code
 * (Descent 1,2 sound resources)
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

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "..\RFPlugin.h"

#include "RF_DSND.h"
#include "resource.h"

typedef struct tagDSNDHandle
{
	HANDLE dsnd;
	DWORD  start,size;
} DSNDHandle;

#define DSNDHANDLE(rf) ((DSNDHandle*)((rf)->rfHandleData))

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

void AddFile(RFile** first, RFile** last, DWORD i, DSNDDirEntry* pentry)
{
	RFile* node;

	node=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
	if (opAddIndex)
		wsprintf(node->rfDataString,"%lX/",i);
	else
		lstrcpy(node->rfDataString,"");
	lstrcpyn(node->rfDataString+lstrlen(node->rfDataString),pentry->fileName,9);
	lstrcat(node->rfDataString,".raw");
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

DWORD CheckArchive(HANDLE f)
{
	DWORD	     read;
	PIGHeader    pighdr;
	DSNDHeader   dsndhdr;
	DSNDDirEntry entry;

	SetFilePointer(f,0,NULL,FILE_BEGIN);
	ReadFile(f,&dsndhdr,sizeof(DSNDHeader),&read,NULL);
	if (read<sizeof(DSNDHeader))
		return 0;
	if (!memcmp(dsndhdr.id,IDSTR_DSND,4))
		return dsndhdr.nFiles;
	else
	{
		SetFilePointer(f,0,NULL,FILE_BEGIN);
		ReadFile(f,&pighdr,sizeof(PIGHeader),&read,NULL);
		if (read<sizeof(PIGHeader))
			return 0;
		SetFilePointer(f,pighdr.nFiles*PIG_REC_SIZE+(pighdr.nSoundFiles-1)*sizeof(DSNDDirEntry),NULL,FILE_CURRENT);
		ReadFile(f,&entry,sizeof(DSNDDirEntry),&read,NULL);
		if (read<sizeof(DSNDDirEntry))
			return 0;
		if (entry.fileSize+entry.fileStart+sizeof(PIGHeader)+pighdr.nFiles*PIG_REC_SIZE+pighdr.nSoundFiles*sizeof(DSNDDirEntry)!=GetFileSize(f,NULL))
			return 0;
		else
		{
			SetFilePointer(f,sizeof(PIGHeader)+pighdr.nFiles*PIG_REC_SIZE,NULL,FILE_BEGIN);
			return pighdr.nSoundFiles;
		}
	}
}

void FillListBox(HWND hwnd, DSNDDirEntry *table, DWORD fullnum)
{
	DWORD i;
	int   index;
	char  str[MAX_PATH+200],name[13];

	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
	SendDlgItemMessage(hwnd,ID_LIST,LB_RESETCONTENT,0,0L);
	for (i=0;i<fullnum;i++)
	{
		lstrcpyn(name,table[i].fileName,9);
		lstrcat(name,".raw");
		wsprintf(str,"%-12s\t%ld KB",name,table[i].fileSize/1024);
		index=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
		if (index!=LB_ERR)
			SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)i);
		SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,i+1,fullnum),0L);
		UpdateWindow(hwnd);
	}
	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
}

LRESULT CALLBACK SelectDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int    index,pt[1],*selitems,num,selnum;
    char   str[MAX_PATH+200];
    RECT   rect;
    LONG   pwidth;
	DWORD  i,read;
	RFile *list,*last;

	static CheckedArchive *pca;
	static DSNDDirEntry   *table;

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
			pt[0]=rect.right-pwidth-4;
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETPARTS,1,(LPARAM)pt);
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETRANGE,0,MAKELPARAM(0,100));
			SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,0,0L);
			GetClientRect(GetDlgItem(hwnd,ID_LIST),&rect);
			pt[0]=(rect.right-rect.left)/3;
			SendDlgItemMessage(hwnd,ID_LIST,LB_SETTABSTOPS,1,(LPARAM)pt);
			ShowWindow(hwnd,SW_SHOWNORMAL);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Allocating memory for list...");
			SendDlgItemMessage(hwnd,ID_LIST,LB_INITSTORAGE,(WPARAM)(pca->number),(LPARAM)(pca->number*25));
			table=(DSNDDirEntry*)GlobalAlloc(GPTR,(pca->number)*sizeof(DSNDDirEntry));
			if (table==NULL)
			{
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Reading DSND file directory...");
			ReadFile(pca->rf,table,(pca->number)*sizeof(DSNDDirEntry),&read,NULL);
			if (read<(pca->number)*sizeof(DSNDDirEntry))
			{
				GlobalFree(table);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filling file list...");
			FillListBox(hwnd,table,pca->number);
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
	DSNDDirEntry entry;

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
	if (number==0)
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
			ReadFile(rf,&entry,sizeof(DSNDDirEntry),&read,NULL);
			if (read<sizeof(DSNDDirEntry))
				break; // ???
			AddFile(&list,&last,i,&entry);
		}
    }
	CloseHandle(rf);

    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, const char* rfDataString)
{
    HANDLE dsnd;
	DWORD  read,i,num,shift;
    RFHandle* rf;
	DSNDDirEntry entry;
	DSNDHandle*  dsndh;
	char *slash,*point,name[13],filename[9],*endptr;

    dsnd=CreateFile(resname,
					GENERIC_READ,
					FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
					NULL
				   );
    if (dsnd==INVALID_HANDLE_VALUE)
		return NULL;
	num=CheckArchive(dsnd);
	if (num==0)
	{
		CloseHandle(dsnd);
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return NULL;
	}
	shift=SetFilePointer(dsnd,0,NULL,FILE_CURRENT)+num*sizeof(DSNDDirEntry);
	slash=strchr(rfDataString,'/');
	if (slash==NULL)
	{
		lstrcpy(name,rfDataString);
		point=strchr(name,'.');
		if (point!=NULL)
			*point=0;
		if (lstrlen(name)<=9)
			memset(name+lstrlen(name),0,9-lstrlen(name)); // ???
		for (i=0;i<num;i++)
		{
			ReadFile(dsnd,&entry,sizeof(DSNDDirEntry),&read,NULL);
			if (read<sizeof(DSNDDirEntry))
				break; // ???
			lstrcpyn(filename,entry.fileName,9);
			if (!lstrcmp(filename,name))
				break;
		}
	}
	else
	{
		lstrcpy(name,slash+1);
		point=strchr(name,'.');
		if (point!=NULL)
			*point=0;
		if (lstrlen(name)<=9)
			memset(name+lstrlen(name),0,9-lstrlen(name)); // ???
		i=strtoul(rfDataString,&endptr,0x10);
		if (i>=num)
		{
			CloseHandle(dsnd);
			SetLastError(PRIVEC(IDS_NOFILE));
			return NULL;
		}
		SetFilePointer(dsnd,i*sizeof(DSNDDirEntry),NULL,FILE_CURRENT);
		ReadFile(dsnd,&entry,sizeof(DSNDDirEntry),&read,NULL);
		if (read<sizeof(DSNDDirEntry))
			lstrcpyn(entry.fileName,"\0\0\0\0\0\0\0",8);
	}
	lstrcpyn(filename,entry.fileName,9);
	if (lstrcmp(filename,name))
	{
		CloseHandle(dsnd);
		SetLastError(PRIVEC(IDS_NOFILE));
		return NULL;
	}
	dsndh=(DSNDHandle*)GlobalAlloc(GPTR,sizeof(DSNDHandle));
    if (dsndh==NULL)
    {
		CloseHandle(dsnd);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	dsndh->dsnd=dsnd;
	dsndh->start=entry.fileStart+shift;
	dsndh->size=entry.fileSize;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		GlobalFree(dsndh);
		CloseHandle(dsnd);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
    rf->rfPlugin=&Plugin;
    rf->rfHandleData=dsndh;

    return rf;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
	if (rf==NULL)
		return TRUE;
	if (DSNDHANDLE(rf)==NULL)
		return FALSE;

    return (
			(CloseHandle(DSNDHANDLE(rf)->dsnd)) &&
			(GlobalFree(DSNDHANDLE(rf))==NULL) &&
			(GlobalFree(rf)==NULL)
		   );
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
	if ((rf==NULL) || (DSNDHANDLE(rf)==NULL))
		return ((DWORD)(-1));

    return (SetFilePointer(DSNDHANDLE(rf)->dsnd,0,NULL,FILE_CURRENT)-(DSNDHANDLE(rf)->start));
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
	DWORD curpos=-1;

	if ((rf==NULL) || (DSNDHANDLE(rf)==NULL))
		return ((DWORD)(-1));

	switch (method)
	{
		case FILE_BEGIN:
			curpos=SetFilePointer(DSNDHANDLE(rf)->dsnd,DSNDHANDLE(rf)->start+pos,NULL,FILE_BEGIN)-(DSNDHANDLE(rf)->start);
			break;
		case FILE_CURRENT:
			curpos=SetFilePointer(DSNDHANDLE(rf)->dsnd,pos,NULL,FILE_CURRENT)-(DSNDHANDLE(rf)->start);
			break;
		case FILE_END:
			curpos=SetFilePointer(DSNDHANDLE(rf)->dsnd,DSNDHANDLE(rf)->start+DSNDHANDLE(rf)->size+pos,NULL,FILE_BEGIN)-(DSNDHANDLE(rf)->start);
			break;
		default: // ???
			break;
	}

	return curpos;
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
	if ((rf==NULL) || (DSNDHANDLE(rf)==NULL))
		return ((DWORD)(-1));

    return DSNDHANDLE(rf)->size;
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
	if ((rf==NULL) || (DSNDHANDLE(rf)==NULL))
		return TRUE;

    return (PluginGetFilePointer(rf)>=PluginGetFileSize(rf));
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
	if (Read!=NULL)
		*Read=0L;
	if ((rf==NULL) || (DSNDHANDLE(rf)==NULL))
		return FALSE;

    return ReadFile(DSNDHANDLE(rf)->dsnd,lpBuffer,min(ToRead,PluginGetFileSize(rf)-PluginGetFilePointer(rf)),Read,NULL);
}

RFPlugin Plugin=
{
	RFP_VER,
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX DSND Resource File Plug-In",
    "v0.91",
    "Parallax DSND Sound Resource Files (*.S11;*.S22)\0*.s11;*.s22\0"
	"Parallax PIG Resource Files (*.PIG)\0*.pig\0",
    "DSND",
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
