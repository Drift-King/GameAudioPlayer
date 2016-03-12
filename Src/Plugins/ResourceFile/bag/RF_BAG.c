/*
 * BAG plug-in source code
 * (Westwood resources: Emperor: Battle For Dune)
 *
 * Copyright (C) 2001 ANX Software
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
#include <mmsystem.h>

#include "..\RFPlugin.h"

#include "RF_BAG.h"
#include "resource.h"

typedef struct tagBAGHandle
{
	HANDLE bag;
	DWORD  start,size;
	DWORD  wavhdrsize;
	char  *wavhdr;
} BAGHandle;

#define BAGHANDLE(rf) ((BAGHandle*)((rf)->rfHandleData))

#define NO_FILTER ((DWORD)(-1))

typedef struct tagCheckedArchive
{
	char  *resname;
	HANDLE rf;
	DWORD  number;
} CheckedArchive;

BOOL opAskSelect,
	 opAddIndex,
	 opEmulateWAV,
	 opSupplyExt;

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
		opEmulateWAV=TRUE;
		opSupplyExt=TRUE;
		return;
    }
    GetPrivateProfileString(Plugin.Description,"AskSelect","on",str,40,Plugin.szINIFileName);
    opAskSelect=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"AddIndex","on",str,40,Plugin.szINIFileName);
    opAddIndex=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"EmulateWAV","on",str,40,Plugin.szINIFileName);
    opEmulateWAV=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"SupplyExt","on",str,40,Plugin.szINIFileName);
    opSupplyExt=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

    WritePrivateProfileString(Plugin.Description,"AskSelect",SaveOptionBool(opAskSelect),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"AddIndex",SaveOptionBool(opAddIndex),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"EmulateWAV",SaveOptionBool(opEmulateWAV),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"SupplyExt",SaveOptionBool(opSupplyExt),Plugin.szINIFileName);
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
			SetCheckBox(hwnd,ID_EMULATEWAV,opEmulateWAV);
			SetCheckBox(hwnd,ID_SUPPLYEXT,opSupplyExt);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_ASKSELECT,TRUE);
					SetCheckBox(hwnd,ID_ADDINDEX,TRUE);
					SetCheckBox(hwnd,ID_EMULATEWAV,TRUE);
					SetCheckBox(hwnd,ID_SUPPLYEXT,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opAskSelect=GetCheckBox(hwnd,ID_ASKSELECT);
					opAddIndex=GetCheckBox(hwnd,ID_ADDINDEX);
					opEmulateWAV=GetCheckBox(hwnd,ID_EMULATEWAV);
					opSupplyExt=GetCheckBox(hwnd,ID_SUPPLYEXT);
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

void AddFile(RFile** first, RFile** last, DWORD i, BAGDirEntry *pentry)
{
	RFile* node;

	node=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
	if (opAddIndex)
		wsprintf(node->rfDataString,"%lX/",i);
	else
		lstrcpy(node->rfDataString,"");
	lstrcpyn(node->rfDataString+lstrlen(node->rfDataString),pentry->szFileName,33);
	if (opSupplyExt)
		lstrcat(node->rfDataString,(pentry->dwFlags & ID_BAG_MP3)?".mp3":((opEmulateWAV)?".wav":".BAGSFX"));
	
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
	BAGHeader baghdr;

	SetFilePointer(file,0,NULL,FILE_BEGIN);
	ReadFile(file,&baghdr,sizeof(BAGHeader),&read,NULL);
	if (
		(read<sizeof(BAGHeader)) ||
	    (memcmp(baghdr.id,IDSTR_GABA,4))
	   )
		return 0L;
	else
		return baghdr.dwFileAmount;
}

void FillListBox(HWND hwnd, BAGDirEntry *table, DWORD fullnum, DWORD filter)
{
	DWORD i;
	int   index;
	char  str[256],name[40],ext[7];

	SendDlgItemMessage(hwnd,ID_PROGRESS,PBM_SETPOS,(UINT)0,0L);
	SendDlgItemMessage(hwnd,ID_LIST,LB_RESETCONTENT,0,0L);
	for (i=0;i<fullnum;i++)
	{
		lstrcpyn(name,table[i].szFileName,33);
		lstrcpy(ext,(table[i].dwFlags & ID_BAG_MP3)?"mp3":((opEmulateWAV)?"wav":"BAGSFX"));
		if (opSupplyExt)
		{
			lstrcat(name,".");
			lstrcat(name,ext);
		}
		_strupr(ext);
		if (filter==NO_FILTER)
		{
			wsprintf(str,"%-39s\t%s\t%s\t%d-bit\t%s\t%ld KB",
					 name,
					 ext,
					 (table[i].dwFlags & ID_BAG_MP3)?"MPEG-1 Layer3":((table[i].dwFlags & ID_BAG_COMPRESSED)?"IMA ADPCM":"PCM"),
					 (table[i].dwFlags & ID_BAG_16BIT)?16:8,
					 (table[i].dwFlags & ID_BAG_STEREO)?"stereo":"mono",
					 table[i].dwFileSize/1024
					);
			index=(int)SendDlgItemMessage(hwnd,ID_LIST,LB_ADDSTRING,0,(LPARAM)str);
			if (index!=LB_ERR)
				SendDlgItemMessage(hwnd,ID_LIST,LB_SETITEMDATA,(WPARAM)index,(LPARAM)i);
			wsprintf(str,"%s (%s) %d-bit %s",
					 ext,
					 (table[i].dwFlags & ID_BAG_MP3)?"MPEG-1 Layer3":((table[i].dwFlags & ID_BAG_COMPRESSED)?"IMA ADPCM":"PCM"),
					 (table[i].dwFlags & ID_BAG_16BIT)?16:8,
					 (table[i].dwFlags & ID_BAG_STEREO)?"stereo":"mono"
					);
			if (SendDlgItemMessage(hwnd,ID_FILTER,CB_FINDSTRINGEXACT,(WPARAM)(-1),(LPARAM)str)==CB_ERR)
			{
				index=(int)SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)str);
				SendDlgItemMessage(hwnd,ID_FILTER,CB_SETITEMDATA,(WPARAM)index,(LPARAM)(table[i].dwFlags));
			}
		}
		else if (table[i].dwFlags==filter)
		{
			wsprintf(str,"%-39s\t%s\t%s\t%d-bit\t%s\t%ld KB",
					 name,
					 ext,
					 (table[i].dwFlags & ID_BAG_MP3)?"MPEG-1 Layer3":((table[i].dwFlags & ID_BAG_COMPRESSED)?"IMA ADPCM":"PCM"),
					 (table[i].dwFlags & ID_BAG_16BIT)?16:8,
					 (table[i].dwFlags & ID_BAG_STEREO)?"stereo":"mono",
					 table[i].dwFileSize/1024
					);
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
    int    index,pt[5],*selitems,num,selnum;
    char   str[MAX_PATH+200];
    RECT   rect;
    LONG   pwidth;
	DWORD  i,read,filter;
	RFile *list,*last;

	static CheckedArchive *pca;
	static BAGDirEntry    *table;

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
			pt[0]=(rect.right-rect.left)*1/5;
			pt[1]=(rect.right-rect.left)*2/5
			pt[2]=(rect.right-rect.left)*3/5;
			pt[3]=(rect.right-rect.left)*4/5;
			pt[4]=(rect.right-rect.left)*5/5;
			SendDlgItemMessage(hwnd,ID_LIST,LB_SETTABSTOPS,5,(LPARAM)pt);

			index=(int)SendDlgItemMessage(hwnd,ID_FILTER,CB_ADDSTRING,0,(LPARAM)"All types");
			SendDlgItemMessage(hwnd,ID_FILTER,CB_SETITEMDATA,(WPARAM)index,(LPARAM)NO_FILTER);
			SendDlgItemMessage(hwnd,ID_FILTER,CB_SETCURSEL,(WPARAM)0,0L);

			ShowWindow(hwnd,SW_SHOWNORMAL);
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Allocating memory for list...");
			SendDlgItemMessage(hwnd,ID_LIST,LB_INITSTORAGE,(WPARAM)(pca->number),(LPARAM)(pca->number*128));
			table=(BAGDirEntry*)GlobalAlloc(GPTR,(pca->number)*sizeof(BAGDirEntry));
			if (table==NULL)
			{
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Reading BAG file directory...");
			ReadFile(pca->rf,table,(pca->number)*sizeof(BAGDirEntry),&read,NULL);
			if (read<(pca->number)*sizeof(BAGDirEntry))
			{
				GlobalFree(table);
				EndDialog(hwnd,(int)NULL);
				return TRUE;
			}
			SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filling file list...");
			FillListBox(hwnd,table,pca->number,NO_FILTER);
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
						filter=(DWORD)SendDlgItemMessage(hwnd,ID_FILTER,CB_GETITEMDATA,(WPARAM)index,(LPARAM)0);
						SendDlgItemMessage(hwnd,ID_STATUS,SB_SETTEXT,0,(LPARAM)"Filtering list...");
						FillListBox(hwnd,table,(pca->number),filter);
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
			ReadFile(rf,&entry,sizeof(BAGDirEntry),&read,NULL);
			if (read<sizeof(BAGDirEntry))
				break; // ???
			AddFile(&list,&last,i,&entry);
		}
    }
	CloseHandle(rf);

    return list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, const char* rfDataString)
{
    HANDLE bag;
	DWORD  read,i,k,number,start,size,*ptr,outsize;
	BOOL   compressed,stereo;
	WORD   spb,bps;
	char  *ch,name[40],*endptr;
	WAVEFORMTEX *pwfex;
    RFHandle* rf;
	BAGDirEntry entry;
	BAGHandle* bagh;

    bag=CreateFile(resname,
				   GENERIC_READ,
				   FILE_SHARE_READ,
				   NULL,
				   OPEN_EXISTING,
				   FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
				   NULL
				  );
    if (bag==INVALID_HANDLE_VALUE)
		return NULL;
	number=CheckArchive(bag);
	if (number==0L)
	{
		CloseHandle(bag);
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return NULL;
	}
	ch=strchr(rfDataString,'/');
	if (ch==NULL)
	{
		lstrcpy(name,rfDataString);
		if ((ch=strrchr(name,'.'))!=NULL)
			*ch=0;
		if (lstrlen(name)<=33)
			memset(name+lstrlen(name),0,33-lstrlen(name));
		for (i=0;i<number;i++)
		{
			ReadFile(bag,&entry,sizeof(BAGDirEntry),&read,NULL);
			if (read<sizeof(BAGDirEntry))
				break; // ???
			if (!memcmp(entry.szFileName,name,32))
				break;
		}
	}
	else
	{
		lstrcpy(name,ch+1);
		if ((ch=strrchr(name,'.'))!=NULL)
			*ch=0;
		if (lstrlen(name)<=33)
			memset(name+lstrlen(name),0,33-lstrlen(name));
		i=strtoul(rfDataString,&endptr,0x10);
		if (i>=number)
		{
			CloseHandle(bag);
			SetLastError(PRIVEC(IDS_NOFILE));
			return NULL;
		}
		SetFilePointer(bag,sizeof(BAGHeader)+i*sizeof(BAGDirEntry),NULL,FILE_BEGIN);
		ReadFile(bag,&entry,sizeof(BAGDirEntry),&read,NULL);
		if (read<sizeof(BAGDirEntry))
			memset(&(entry.szFileName),0,32); // ???
	}
	if (memcmp(entry.szFileName,name,32))
	{
		CloseHandle(bag);
		SetLastError(PRIVEC(IDS_NOFILE));
		return NULL;
	}
	bagh=(BAGHandle*)GlobalAlloc(GPTR,sizeof(BAGHandle));
    if (bagh==NULL)
    {
		CloseHandle(bag);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	bagh->bag=bag;
	bagh->start=entry.dwStart;
	bagh->size=entry.dwSize;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		GlobalFree(bagh);
		CloseHandle(bag);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
	if (opEmulateWAV)
	{
		bagh->wavhdrsize=28+sizeof(WAVEFORMATEX)+((entry.dwFlags & ID_BAG_COMPRESSED)?14:0);
		bagh->wavhdr=(char*)GlobalAlloc(GPTR,bagh->wavhdrsize);
		compressed=(entry.dwFlags & ID_BAG_COMPRESSED);
		stereo=(entry.dwFlags & ID_BAG_STEREO);
		bps=((entry.dwFlags & ID_BAG_16BIT)?16:8);
		spb=(WORD)((entry.dwBlockAlign-((stereo)?8:4))*((stereo)?1:2)+1);
		ptr=(DWORD*)(bagh->wavhdr);
		*ptr++=(DWORD)mmioFOURCC('R','I','F','F');
		*ptr++=20+sizeof(WAVEFORMATEX)+((compressed)?14:0)+entry.dwSize;
		*ptr++=(DWORD)mmioFOURCC('W','A','V','E');
		*ptr++=(DWORD)mmioFOURCC('f','m','t',' ');
		*ptr++=sizeof(WAVEFORMATEX)+((compressed)?2:0);
		pwfex=(WAVEFORMATEX*)ptr;
		memset(pwfex,0,sizeof(WAVEFORMATEX));
		pwfex->wFormatTag=(compressed)?WAVE_FORMAT_IMA_ADPCM:WAVE_FORMAT_PCM;
		pwfex->nChannels=(stereo)?2:1;
		pwfex->nSamplesPerSec=entry.dwSampleRate;
		pwfex->nAvgBytesPerSec=(compressed)?((entry.dwSampleRate/spb)*entry.dwBlockAlign):(entry.dwSampleRate*((stereo)?2:1)*(bps/8));
		pwfex->nBlockAlign=(compressed)?((WORD)entry.dwBlockAlign):(((stereo)?2:1)*(bps/8));
		pwfex->wBitsPerSample=(compressed)?4:bps;
		pwfex->cbSize=(compressed)?2:0;
		ptr+=sizeof(WAVEFORMATEX);
		if (compressed)
		{
			*((WORD*)ptr)++=spb;
			*ptr++=(DWORD)mmioFOURCC('f','a','c','t');
			*ptr++=4;
			outsize=(entry.dwSize/entry.dwBlockAlign)*spb;
			if (entry.dwSize%entry.dwBlockAlign)
				outsize+=(entry.dwSize%entry.dwBlockAlign-((stereo)?8:4))*((stereo)?1:2)+1;
			*ptr++=outsize;
		}
		*ptr++=(DWORD)mmioFOURCC('d','a','t','a');
		*ptr++=entry.dwSize;
	}
	else
	{
		bagh->wavhdrsize=0L;
		bagh->wavhdr=NULL;
	}
    rf->rfPlugin=&Plugin;
    rf->rfHandleData=bagh;

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
