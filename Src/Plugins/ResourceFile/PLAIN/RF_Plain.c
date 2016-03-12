/*
 * Plain plug-in source code
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

#include "..\RFPlugin.h"

#include "resource.h"

RFPlugin Plugin;

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

RFile* __stdcall PluginGetFileList(const char* resname)
{
    RFile* list;

    list=(RFile*)GlobalAlloc(GPTR,sizeof(RFile));
    lstrcpy(list->rfDataString,"");
    list->next=NULL;

    return list;
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

RFHandle* __stdcall PluginOpenFile(const char* resname, const char* rfDataString)
{
    HANDLE rfd;
    RFHandle* rf;

    rfd=CreateFile(resname,
				   GENERIC_READ,
				   FILE_SHARE_READ,
				   NULL,
				   OPEN_EXISTING,
				   FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, // ???
				   NULL
				  );
    if (rfd==INVALID_HANDLE_VALUE)
		return NULL;
    rf=(RFHandle*)GlobalAlloc(GPTR,sizeof(RFHandle));
    if (rf==NULL)
    {
		CloseHandle(rfd);
		SetLastError(PRIVEC(IDS_NOMEM));
		return NULL;
    }
    rf->rfPlugin=&Plugin;
    rf->rfHandleData=rfd;

    return rf;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
	if (rf==NULL)
		return TRUE;

    return (CloseHandle((HANDLE)(rf->rfHandleData)) && (GlobalFree(rf)==NULL));
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
	if (rf==NULL)
		return ((DWORD)(-1));

    return SetFilePointer((HANDLE)(rf->rfHandleData),0,NULL,FILE_CURRENT);
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
	if (rf==NULL)
		return ((DWORD)(-1));

    return SetFilePointer((HANDLE)(rf->rfHandleData),pos,NULL,method);
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
	if (rf==NULL)
		return ((DWORD)(-1));

    return GetFileSize((HANDLE)(rf->rfHandleData),NULL);
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
	if (rf==NULL)
		return TRUE;

    return (PluginGetFilePointer(rf)>=PluginGetFileSize(rf));
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
	if (Read!=NULL)
		*Read=0L;
	if (rf==NULL)
		return FALSE;

    return ReadFile((HANDLE)(rf->rfHandleData),lpBuffer,ToRead,Read,NULL);
}

void __stdcall PluginInit(void)
{
	DisableThreadLibraryCalls(Plugin.hDllInst);
}

RFPlugin Plugin=
{
	RFP_VER,
    0,
    0,
	0,
    NULL,
    "ANX Plain Resource File Plug-In",
    "v1.01",
    NULL,
    "PLAIN",
    NULL,
    PluginAbout,
    PluginInit,
    NULL,
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
