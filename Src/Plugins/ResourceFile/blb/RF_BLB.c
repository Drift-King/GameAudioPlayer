/*
 * BLB plug-in source code
 * (DreamWorks resources: The Neverhood)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * Info on PKWARE-compressed files dealing is provided by 
 * Dmitry Kirnocenskij: ejt@mail.ru
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

#include "..\RFPlugin.h"

#include "RF_BLB.h"
#include "resource.h"

typedef struct tagBLBHandle
{
	HANDLE blb;
	BYTE   bAction;
	DWORD  start,size,outsize;
	char  *buffer;
	DWORD  pointer;
} BLBHandle;

#define BLBHandle(rf) ((BLBHandle*)((rf)->rfHandleData))

typedef struct tagCheckedArchive
{
	char  *resname;
	HANDLE rf;
	DWORD  number;
	WORD   datasize;
} CheckedArchive;

BOOL opAskSelect,
	 opAddIndex,
	 opSupportCompressed,
	 opCheckDecompression;

#define PKLIB_NAME "PKWARE.DLL"

HINSTANCE hDllPKLib;

DWORD (__cdecl *Uncompress)(char *outbuff, DWORD *outsize, char *inbuff, DWORD insize);

LONG pklibUsed;

RFPlugin Plugin;

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void InitPKLib(void)
{
	if (Uncompress!=NULL)
		return;

	hDllPKLib=LoadLibrary(PKLIB_NAME);
	if (hDllPKLib==NULL)
	{
		MessageBox(GetFocus(),"Failed to load "PKLIB_NAME"\nPKWARE-compressed files will be unaccessible.",Plugin.Description,MB_OK | MB_ICONERROR);
		Uncompress=NULL;
	}
	else
	{
		Uncompress=(DWORD (__cdecl *)(char*, DWORD*, char*, DWORD))GetProcAddress(hDllPKLib,"Uncompress");
		if (Uncompress==NULL)
		{
			MessageBox(GetFocus(),"Failed to get decompression function address.\nPKWARE-compressed files will be unaccessible.",Plugin.Description,MB_OK | MB_ICONERROR);
			FreeLibrary(hDllPKLib);
			hDllPKLib=NULL;
		}
	}
}

void __stdcall Init(void)
{
    char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opAskSelect=TRUE;
		opAddIndex=TRUE;
		opSupportCompressed=TRUE;
		opCheckDecompression=FALSE;
		return;
    }
    GetPrivateProfileString(Plugin.Description,"AskSelect","on",str,40,Plugin.szINIFileName);
    opAskSelect=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"AddIndex","on",str,40,Plugin.szINIFileName);
    opAddIndex=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"SupportCompressed","on",str,40,Plugin.szINIFileName);
    opSupportCompressed=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckDecompression","off",str,40,Plugin.szINIFileName);
    opCheckDecompression=LoadOptionBool(str);

	hDllPKLib=NULL;
	Uncompress=NULL;
	pklibUsed=0L;
	if (opSupportCompressed)
		InitPKLib();		
}

#define SaveOptionBool(b) ((b)?"on":"off")

void ShutdownPKLib(void)
{
	FreeLibrary(hDllPKLib);
	hDllPKLib=NULL;
	Uncompress=NULL;
	pklibUsed=0L;
}

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	ShutdownPKLib();

    WritePrivateProfileString(Plugin.Description,"AskSelect",SaveOptionBool(opAskSelect),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"AddIndex",SaveOptionBool(opAddIndex),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"SupportCompressed",SaveOptionBool(opSupportCompressed),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckDecompression",SaveOptionBool(opCheckDecompression),Plugin.szINIFileName);
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
			SetCheckBox(hwnd,ID_SUPPORTCOMPRESSED,opSupportCompressed);
			SetCheckBox(hwnd,ID_CHECKDECOMPRESSION,opCheckDecompression);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_ASKSELECT,TRUE);
					SetCheckBox(hwnd,ID_ADDINDEX,TRUE);
					SetCheckBox(hwnd,ID_SUPPORTCOMPRESSED,TRUE);
					SetCheckBox(hwnd,ID_CHECKDECOMPRESSION,FALSE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opAskSelect=GetCheckBox(hwnd,ID_ASKSELECT);
					opAddIndex=GetCheckBox(hwnd,ID_ADDINDEX);
					opSupportCompressed=GetCheckBox(hwnd,ID_SUPPORTCOMPRESSED);
					if (opSupportCompressed)
						InitPKLib();
					else
					{
						while (pklibUsed>0) {}; // !!!
						ShutdownPKLib();
					}
					opCheckDecompression=GetCheckBox(hwnd,ID_CHECKDECOMPRESSION);
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

void AddNode(RFile** first, RFile** last, RFile* node)
{
	node->next=NULL;
    if ((*first)==NULL)
		*first=node;
    else
		(*last)->next=node;
    (*last)=node;
}

void AddFile(RFile** first, RFile** last, DWORD i, DWORD id, BLBDirEntry* pentry, BYTE shift)
{
	RFile* node;

	node=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
	if (opAddIndex)
		wsprintf(node->rfDataString,"%lX/",i);
	else
		lstrcpy(node->rfDataString,"");
	wsprintf(node->rfDataString+lstrlen(node->rfDataString),"ID_%08lX-%02X.%02X",id,shift,pentry->bType);
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

DWORD CheckArchive(HANDLE file, WORD* datasize)
{
	DWORD     read;
	BLBHeader blbhdr;

	SetFilePointer(file,0,NULL,FILE_BEGIN);
	ReadFile(file,&blbhdr,sizeof(BLBHeader),&read,NULL);
	if (
		(read<sizeof(BLBHeader)) ||
	    (memcmp(blbhdr.szID,IDSTR_BLB,4)) ||
		(blbhdr.bID!=ID_BLB) ||
		(blbhdr.dwFileSize!=GetFileSize(file,NULL))
	   )
		return 0L;
	else
	{
		*datasize=blbhdr.wDataSize;
		return blbhdr.dwNumber;
	}
}

#define CheckDataIndex(di,ds) ( ((di)>=1) && ((di)<=(ds)) )

BOOL ReadDirEntry
(
	HANDLE f, 
	DWORD  i, 
	DWORD  number, 
	WORD   datasize,
	DWORD* pid,
	BLBDirEntry* pentry, 
	BYTE*  pshift
)
{
	DWORD read;

	if (i>=number)
		return FALSE;

	SetFilePointer(f,sizeof(BLBHeader)+i*4,NULL,FILE_BEGIN);
	ReadFile(f,pid,4,&read,NULL);
	if (read<4)
		return FALSE;

	SetFilePointer(f,sizeof(BLBHeader)+number*4+i*sizeof(BLBDirEntry),NULL,FILE_BEGIN);
	ReadFile(f,pentry,sizeof(BLBDirEntry),&read,NULL);
	if (read<sizeof(BLBDirEntry))
		return FALSE;

	if (CheckDataIndex(pentry->wDataIndex,datasize))
	{
		SetFilePointer(f,sizeof(BLBHeader)+number*(4+sizeof(BLBDirEntry))+(pentry->wDataIndex)-1,NULL,FILE_BEGIN);
		ReadFile(f,pshift,1,&read,NULL);
		if (read<1)
			*pshift=0xFF;
	}
	else
		*pshift=0xFF;

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

void FillListBox
(
	HWND   hwnd, 
	DWORD *ids, 
	BLBDirEntry *table, 
	BYTE  *shifts, 
	DWORD  fullnum, 
	WORD   datasize,
	char  *filter
)
{
	DWORD i;
	int   index;
	char  str[MAX_PATH+200],name[20],ext[6],action[20];

	if (filter==NULL)
		lstrcpy(ext,"*");
	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
	SendDlgItemMessage(hwnd,ID_LIST,LB_RESETCONTENT,0,0L);
	for (i=0;i<fullnum;i++)
	{
		wsprintf(name,"ID_%08lX-%02X.%02X",ids[i],(CheckDataIndex(table[i].wDataIndex,datasize))?shifts[table[i].wDataIndex-1]:0xFF,table[i].bType);
		switch (table[i].bAction)
		{
			case ID_BLB_ACTION_COMPRESSED:
				lstrcpy(action,"compressed");
				break;
			case ID_BLB_ACTION_FAKE:
				lstrcpy(action,"fake");
				break;
			case ID_BLB_ACTION_NONE:
			default:
				lstrcpy(action,"");
				break;
		}
		if (filter==NULL)
		{
			wsprintf(str,"%-17s\t%s\t%ld KB",name,action,table[i].dwOutSize/1024);
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
			wsprintf(str,"%-17s\t%s\t%ld KB",name,action,table[i].dwOutSize/1024);
			index=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
			if (index!=LB_ERR)
				SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)i);
		}
		SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,i+1,fullnum),0L);
		UpdateWindow(hwnd);
	}
	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
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

LRESULT CALLBACK SelectDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int    index,pt[2],*selitems,num,selnum;
    char   str[MAX_PATH+200],ext[6];
    RECT   rect;
    LONG   pwidth;
	DWORD  i,read;
	RFile *list,*last;

	static CheckedArchive *pca;
	static DWORD          *ids;
	static BLBDirEntry    *table;
	static BYTE           *shifts;
	
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
			pwidth=rect.right-rect.left;
			pt[0]=rect.left+pwidth/4;
			pt[1]=rect.left+5*pwidth/12;
			SendDlgItemMessage(hwnd,ID_LIST,LB_SETTABSTOPS,2,(LPARAM)pt);
			SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)"*.*");
			SendDlgItemMessage(hwnd,ID_FILTER,CB_SETCURSEL,(WPARAM)0,0L);
			ShowWindow(hwnd,SW_SHOWNORMAL);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Allocating memory for list...");
			SendDlgItemMessage(hwnd,ID_LIST,LB_INITSTORAGE,(WPARAM)(pca->number),(LPARAM)(pca->number*60));
			ids=(DWORD*)GlobalAlloc(GPTR,(pca->number)*sizeof(DWORD));
			table=(BLBDirEntry*)GlobalAlloc(GPTR,(pca->number)*sizeof(BLBDirEntry));
			shifts=(BYTE*)GlobalAlloc(GPTR,(pca->datasize)*sizeof(BYTE));
			if (
				(ids==NULL) ||
				(table==NULL) ||
				(shifts==NULL)
			   )
			{
				GlobalFree(shifts);
				GlobalFree(table);
				GlobalFree(ids);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Reading BLB file directory...");
			ReadFile(pca->rf,ids,(pca->number)*sizeof(DWORD),&read,NULL);
			if (read<(pca->number)*sizeof(DWORD))
			{
				GlobalFree(shifts);
				GlobalFree(table);
				GlobalFree(ids);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			ReadFile(pca->rf,table,(pca->number)*sizeof(BLBDirEntry),&read,NULL);
			if (read<(pca->number)*sizeof(BLBDirEntry))
			{
				GlobalFree(shifts);
				GlobalFree(table);
				GlobalFree(ids);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			ReadFile(pca->rf,shifts,(pca->datasize)*sizeof(BYTE),&read,NULL);
			if (read<(pca->datasize)*sizeof(BYTE))
			{
				GlobalFree(shifts);
				GlobalFree(table);
				GlobalFree(ids);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filling file list...");
			FillListBox(hwnd,ids,table,shifts,pca->number,pca->datasize,NULL);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
			return TRUE;
		case WM_CLOSE:
			GlobalFree(shifts);
			GlobalFree(table);
			GlobalFree(ids);
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
						FillListBox(hwnd,ids,table,shifts,pca->number,pca->datasize,ext);
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
					SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Ready");
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					LocalFree(selitems);
					break;
				case IDCANCEL:
					GlobalFree(shifts);
					GlobalFree(table);
					GlobalFree(ids);
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
							AddFile(&list,&last,i,ids[i],table+i,(CheckDataIndex(table[i].wDataIndex,pca->datasize))?(shifts[table[i].wDataIndex-1]):0xFF);
							SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,MulDiv(100,index+1,selnum),0L);
						}
					}
					SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
					LocalFree(selitems);
					GlobalFree(shifts);
					GlobalFree(table);
					GlobalFree(ids);
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
	DWORD  i,number,id;
	BYTE   shift;
	WORD   datasize;
    RFile *list,*last;
	BLBDirEntry entry;

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
	number=CheckArchive(rf,&datasize);
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
		ca.datasize=datasize;

		list=(RFile*)DialogBoxParam(Plugin.hDllInst,"Select",GetActiveWindow(),SelectDlgProc,(LPARAM)&ca);
	}
    else
    {
		list=NULL;
		last=NULL;
		for (i=0;i<number;i++)
		{
			if (!ReadDirEntry(rf,i,number,datasize,&id,&entry,&shift))
				break; // ???
			AddFile(&list,&last,i,id,&entry,shift);
		}
    }
	CloseHandle(rf);

    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, const char* rfDataString)
{
	RFHandle* rf;
	BLBHandle* blbh;
    HANDLE blb;
	DWORD  read,i,number,start,size,outsize,id,cid;
	BYTE   shift,action;
	WORD   datasize;
    char  *slash,*packed,*endptr;
	BLBDirEntry entry;

    blb=CreateFile(resname,
				   GENERIC_READ,
				   FILE_SHARE_READ,
				   NULL,
				   OPEN_EXISTING,
				   FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
				   NULL
				  );
    if (blb==INVALID_HANDLE_VALUE)
		return NULL;
	number=CheckArchive(blb,&datasize);
	if (number==0L)
	{
		CloseHandle(blb);
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return NULL;
	}
	slash=strchr(rfDataString,'/');
	if (slash==NULL)
	{
		cid=id=0xFFFFFFFF; // ???
		slash=strchr(rfDataString,'_');
		if (slash!=NULL)
		{
			id=strtoul(slash+1,&endptr,0x10);
			SetFilePointer(blb,sizeof(BLBHeader),NULL,FILE_BEGIN);
			for (i=0;i<number;i++)
			{
				ReadFile(blb,&cid,4,&read,NULL);
				if ((read<4) || (cid==id))
					break;
			}
		}
	}
	else
	{
		i=strtoul(rfDataString,&endptr,0x10);
		if (i>=number)
		{
			CloseHandle(blb);
			SetLastError(PRIVEC(IDS_NOFILE));
			return NULL;
		}
		SetFilePointer(blb,sizeof(BLBHeader)+i*4,NULL,FILE_BEGIN);
		cid=id=0xFFFFFFFF; // ???
		slash=strchr(rfDataString,'_');
		if (slash!=NULL)
		{
			id=strtoul(slash+1,&endptr,0x10);
			ReadFile(blb,&cid,4,&read,NULL);
		}
	}
	if (
		(cid!=id) ||
		(id==0xFFFFFFFF) ||
		(cid==0xFFFFFFFF)
	   )
	{
		CloseHandle(blb);
		SetLastError(PRIVEC(IDS_NOFILE));
		return NULL;
	}
	ReadDirEntry(blb,i,number,datasize,&id,&entry,&shift);
	switch (entry.bAction)
	{
		case ID_BLB_ACTION_FAKE:
			CloseHandle(blb);
			SetLastError(PRIVEC(IDS_FAKEFILE));
			return NULL;
		case ID_BLB_ACTION_COMPRESSED:
			if (Uncompress==NULL)
			{
				CloseHandle(blb);
				SetLastError(PRIVEC(IDS_NOFUNCTION));
				return NULL;
			}
			break;
		default:
			break;
	}
	start=entry.dwStart;
	outsize=entry.dwOutSize;
	action=entry.bAction;
	size=entry.dwStart;
	if (i+1<number)
	{
		ReadDirEntry(blb,i+1,number,datasize,&id,&entry,&shift);
		size=entry.dwStart-size;
	}
	else
		size=GetFileSize(blb,NULL)-size;
	blbh=(BLBHandle*)GlobalAlloc(GPTR,sizeof(BLBHandle));
    if (blbh==NULL)
    {
		CloseHandle(blb);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	blbh->blb=blb;
	blbh->start=start;
	blbh->size=size;
	blbh->outsize=outsize;
	blbh->bAction=action;
	blbh->buffer=NULL;
	blbh->pointer=0L;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		GlobalFree(blbh);
		CloseHandle(blb);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
    rf->rfPlugin=&Plugin;
    rf->rfHandleData=blbh;
	if (blbh->bAction==ID_BLB_ACTION_COMPRESSED)
	{
		InterlockedIncrement(&pklibUsed);
		blbh->buffer=(char*)GlobalAlloc(GPTR,outsize);
		if (blbh->buffer==NULL)
		{
			InterlockedDecrement(&pklibUsed);
			GlobalFree(blbh);
			CloseHandle(blb);
			SetLastError(PRIVEC(IDS_NOMEM));
			return NULL;
		}
		packed=(char*)GlobalAlloc(GPTR,size);
		if (packed==NULL)
		{
			InterlockedDecrement(&pklibUsed);
			GlobalFree(blbh->buffer);
			GlobalFree(blbh);
			CloseHandle(blb);
			SetLastError(PRIVEC(IDS_NOMEM));
			return NULL;
		}
		SetFilePointer(blb,start,NULL,FILE_BEGIN);
		ReadFile(blb,packed,size,&read,NULL);
		if (read<size)
		{
			InterlockedDecrement(&pklibUsed);
			GlobalFree(packed);
			GlobalFree(blbh->buffer);
			GlobalFree(blbh);
			CloseHandle(blb);
			SetLastError(PRIVEC(IDS_READINGFAILED));
			return NULL;
		}
		Uncompress(blbh->buffer,&outsize,packed,size);
		if (opCheckDecompression)
		{
			if (outsize<blbh->outsize)
			{
				InterlockedDecrement(&pklibUsed);
				GlobalFree(packed);
				GlobalFree(blbh->buffer);
				GlobalFree(blbh);
				CloseHandle(blb);
				SetLastError(PRIVEC(IDS_DECOMPRESSIONFAILED));
				return NULL;
			}
		}
		GlobalFree(packed);
		CloseHandle(blb);
		blbh->blb=INVALID_HANDLE_VALUE;
		InterlockedDecrement(&pklibUsed);
	}

    return rf;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
	if (rf==NULL)
		return TRUE;
	if (BLBHandle(rf)==NULL)
		return FALSE;

    return (
			(GlobalFree(BLBHandle(rf)->buffer)==NULL) &&
			(CloseHandle(BLBHandle(rf)->blb)) &&
			(GlobalFree(BLBHandle(rf))==NULL) &&
			(GlobalFree(rf)==NULL)
		   );
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
	if ((rf==NULL) || (BLBHandle(rf)==NULL))
		return ((DWORD)(-1));

	switch (BLBHandle(rf)->bAction)
	{
		case ID_BLB_ACTION_NONE:
			return (SetFilePointer(BLBHandle(rf)->blb,0,NULL,FILE_CURRENT)-(BLBHandle(rf)->start));
		case ID_BLB_ACTION_COMPRESSED:
			if (BLBHandle(rf)->buffer!=NULL)
				return (BLBHandle(rf)->pointer);
			break;
		default:
			break;
	}

	return ((DWORD)(-1));
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
	DWORD curpos=-1;

	if ((rf==NULL) || (BLBHandle(rf)==NULL))
		return curpos;

	switch (BLBHandle(rf)->bAction)
	{
		case ID_BLB_ACTION_NONE:
			switch (method)
			{
				case FILE_BEGIN:
					curpos=SetFilePointer(BLBHandle(rf)->blb,BLBHandle(rf)->start+pos,NULL,FILE_BEGIN)-(BLBHandle(rf)->start);
					break;
				case FILE_CURRENT:
					curpos=SetFilePointer(BLBHandle(rf)->blb,pos,NULL,FILE_CURRENT)-(BLBHandle(rf)->start);
					break;
				case FILE_END:
					curpos=SetFilePointer(BLBHandle(rf)->blb,BLBHandle(rf)->start+BLBHandle(rf)->size+pos,NULL,FILE_BEGIN)-(BLBHandle(rf)->start);
					break;
				default: // ???
					break;
			}
			break;
		case ID_BLB_ACTION_COMPRESSED:
			if (BLBHandle(rf)->buffer!=NULL)
			{
				switch (method)
				{
					case FILE_BEGIN:
						curpos=BLBHandle(rf)->pointer=pos;
						break;
					case FILE_CURRENT:
						curpos=BLBHandle(rf)->pointer+=pos;
						break;
					case FILE_END:
						curpos=BLBHandle(rf)->pointer=BLBHandle(rf)->outsize+pos;
						break;
					default: // ???
						break;
				}
			}
			break;
		default:
			break;
	}

    return curpos;
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
	if ((rf==NULL) || (BLBHandle(rf)==NULL))
		return ((DWORD)(-1));

    return BLBHandle(rf)->outsize;
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
	if ((rf==NULL) || (BLBHandle(rf)==NULL))
		return TRUE;

    return (PluginGetFilePointer(rf)>=PluginGetFileSize(rf));
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
	if (Read!=NULL)
		*Read=0L;
	if ((rf==NULL) || (BLBHandle(rf)==NULL))
		return FALSE;

	switch (BLBHandle(rf)->bAction)
	{
		case ID_BLB_ACTION_NONE:
			return ReadFile(BLBHandle(rf)->blb,lpBuffer,min(ToRead,PluginGetFileSize(rf)-PluginGetFilePointer(rf)),Read,NULL);
		case ID_BLB_ACTION_COMPRESSED:
			if (BLBHandle(rf)->buffer!=NULL)
			{
				ToRead=min(ToRead,(BLBHandle(rf)->outsize)-(BLBHandle(rf)->pointer));
				memcpy(lpBuffer,(BLBHandle(rf)->buffer)+(BLBHandle(rf)->pointer),ToRead);
				if (Read!=NULL)
					*Read=ToRead;
				BLBHandle(rf)->pointer+=ToRead;
				return TRUE;
			}
			break;
		default:
			break;
	}

	return FALSE;
}

RFPlugin Plugin=
{
	RFP_VER,
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX BLB Resource File Plug-In",
    "v0.90",
    "DreamWorks BLB Resource Files (*.BLB)\0*.blb\0",
    "BLB",
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
