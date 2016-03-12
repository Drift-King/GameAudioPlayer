/*
 * Game Audio Player source code: file sections management
 *
 * File section is just a part of file (which may be a file in resource)
 * starting at dwSectionStart with the size dwSectionLength.
 * If the file contains several audio files "as is" within, then each such
 * audio file is a file section.
 * 
 * Examples: C&C/RedAlert AUDs in MIXes, Sierra games SOLs in RESOURCE.SFX/.AUD, etc.
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

#include "..\Plugins\ResourceFile\RFPlugin.h"
#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Progress.h"
#include "PlayDialog.h"
#include "Errors.h"
#include "Misc.h"
#include "FSHandler.h"

FSHandle* __stdcall FSOpenFileAsSection(RFPlugin* plugin, const char* rfName, const char* rfDataString, DWORD start, DWORD length)
{
    FSHandle* f;
    RFHandle* rf;

    if (plugin==NULL)
		return NULL;

    rf=plugin->OpenFile(rfName,rfDataString);
    if (rf==NULL)
		return NULL;
    f=(FSHandle*)GlobalAlloc(GPTR,sizeof(FSHandle));
    f->rf=rf;
    f->start=start;
    if (length<=plugin->GetFileSize(rf))
		f->length=length;
    else
		f->length=plugin->GetFileSize(rf);
    f->node=NULL;
    f->afData=NULL;

    return f;
}

FSHandle* __stdcall FSOpenFile(AFile* node)
{
    FSHandle* f;

    if (node!=NULL)
    {
		f=FSOpenFileAsSection(RFPLUGIN(node),node->rfName,node->rfDataString,node->fsStart,node->fsLength);
		if (f!=NULL)
			f->node=node;
		return f;
    }
    else
		return NULL;
}

BOOL __stdcall FSCloseFile(FSHandle* f)
{
    if (f!=NULL)
    {
		if ((f->rf==NULL) || (RFPLUGIN(f->rf)==NULL))
			return FALSE;
		return ((RFPLUGIN(f->rf)->CloseFile(f->rf)) && (GlobalFree(f)==NULL));
    }
    else
		return TRUE;
}

DWORD __stdcall FSSetFilePointer(FSHandle* f, LONG pos, DWORD method)
{
    if (f!=NULL)
    {
		if ((f->rf==NULL) || (RFPLUGIN(f->rf)==NULL))
			return 0xFFFFFFFF;
		switch (method)
		{
			case FILE_BEGIN:
				return RFPLUGIN(f->rf)->SetFilePointer(f->rf,(f->start)+pos,FILE_BEGIN);
			case FILE_CURRENT:
				return RFPLUGIN(f->rf)->SetFilePointer(f->rf,pos,FILE_CURRENT);
			case FILE_END:
				return RFPLUGIN(f->rf)->SetFilePointer(f->rf,(f->start)+(f->length)+pos,FILE_BEGIN);
			default: // ???
				return 0xFFFFFFFF;
		}
    }
    else
		return 0xFFFFFFFF;
}

DWORD __stdcall FSGetFilePointer(FSHandle* f)
{
    if (f!=NULL)
    {
		if ((f->rf==NULL) || (RFPLUGIN(f->rf)==NULL))
			return 0xFFFFFFFF;
		return RFPLUGIN(f->rf)->GetFilePointer(f->rf)-(f->start);
    }
    else
		return 0xFFFFFFFF;
}

DWORD __stdcall FSGetFileSize(FSHandle* f)
{
    if (f!=NULL)
    {
		if ((f->rf==NULL) || (RFPLUGIN(f->rf)==NULL))
			return 0xFFFFFFFF;
		return min((f->length),RFPLUGIN(f->rf)->GetFileSize(f->rf)-(f->start));
    }
    else
		return 0xFFFFFFFF;
}

BOOL __stdcall FSEndOfFile(FSHandle* f)
{
    if (f!=NULL)
    {
		if ((f->rf==NULL) || (RFPLUGIN(f->rf)==NULL))
			return TRUE;
		return (
				(FSGetFilePointer(f)>=FSGetFileSize(f)) ||
				(RFPLUGIN(f->rf)->EndOfFile(f->rf))
			   );
    }
    else
		return TRUE;
}

BOOL __stdcall FSReadFile(FSHandle* f, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
	if (Read!=NULL)
		*Read=0L;
    if (f!=NULL)
    {
		if ((f->rf==NULL) || (RFPLUGIN(f->rf)==NULL))
			return FALSE;
		return RFPLUGIN(f->rf)->ReadFile(f->rf,lpBuffer,min(ToRead,FSGetFileSize(f)-FSGetFilePointer(f)),Read);
    }
    else
		return FALSE;
}

FSHandler appFSHandler=
{
    FSOpenFile,
    FSCloseFile,
    FSGetFileSize,
    FSGetFilePointer,
    FSEndOfFile,
    FSSetFilePointer,
    FSReadFile,
};

void SaveNode(HWND hwnd, AFile* node, const char* fname)
{
    FSHandle *file;
	char	 *saveBuffer,str[MAX_PATH+100];
    HANDLE    savefile;
    DWORD     read,written;

    if (node==NULL)
		return;
	if (!EnsureDirPresence(fname))
		return;
    file=FSOpenFile(node);
    if (file==NULL)
    {
		wsprintf(str,"Cannot open resource file: %s",node->rfName);
		ReportError(hwnd,str,RFPLUGIN(node)->hDllInst);
		return;
    }
    FSSetFilePointer(file,0,FILE_BEGIN);
    savefile=CreateFile(fname,
						GENERIC_WRITE,
						FILE_SHARE_READ,
						NULL,
						CREATE_ALWAYS,
						FILE_ATTRIBUTE_NORMAL,
						NULL
					   );
    if (savefile==INVALID_HANDLE_VALUE)
    {
		wsprintf(str,"Cannot create/open file: %s",fname);
		ReportError(hwnd,str,NULL);
		FSCloseFile(file);
		return;
    }
	saveBuffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
    SetFilePointer(savefile,0,NULL,FILE_BEGIN);
    ShowProgressHeaderMsg(fname);
    while (!FSEndOfFile(file))
    {
		if (IsCancelled())
			break;
		ShowProgressStateMsg("Reading file...");
		FSReadFile(file,saveBuffer,BUFFERSIZE,&read);
		if (read==0L)
			break;
		ShowProgressStateMsg("Writing file...");
		WriteFile(savefile,saveBuffer,read,&written,NULL);
		if (written!=read)
		{
			ReportError(hwnd,"Failure writing file.",NULL);
			SetCancelFlag();
			break;
		}
		ShowProgress(FSGetFilePointer(file),FSGetFileSize(file));
    }
    FSCloseFile(file);
    CloseHandle(savefile);
    if (IsCancelled())
		DeleteFile(fname);
	GlobalFree(saveBuffer);
}

FSHandle* FSOpenForPlayback(HWND hwnd, AFile* node, DWORD* rate, WORD* channels, WORD* bits)
{
    FSHandle *file;
    char      str[MAX_PATH+400],lbr[2],rbr[2];

    if (node==NULL)
		return NULL;

    if (node->afPlugin==NULL)
    {
		wsprintf(str,"No audio file plug-in for this %s",node->afID);
		MessageBox(GetFocus(),str,szAppName,MB_OK | MB_ICONEXCLAMATION);
		return NULL;
    }
	if (node->rfPlugin==NULL)
    {
		wsprintf(str,"No resource file plug-in for this %s",node->rfID);
		MessageBox(GetFocus(),str,szAppName,MB_OK | MB_ICONEXCLAMATION);
		return NULL;
    }
    if ((file=FSOpenFile(node))==NULL)
	{
		lstrcpy(lbr,(lstrcmpi(node->rfDataString,""))?"(":"");
		lstrcpy(rbr,(lstrcmpi(node->rfDataString,""))?")":"");
		wsprintf(str,"Cannot open file: %s %s%s%s",node->rfName,lbr,node->rfDataString,rbr);
		ReportError(hwnd,str,RFPLUGIN(node)->hDllInst);
		UpdateWindow(hwnd);
		UpdateWindow(playDialog);
	    return NULL;
	}
    if (!(AFPLUGIN(node)->InitPlayback(file,rate,channels,bits)))
    {
		wsprintf(str,"Cannot initialize playback for this %s",node->afID);
		ReportError(hwnd,str,AFPLUGIN(node)->hDllInst);
		FSCloseFile(file);
		UpdateWindow(hwnd);
		UpdateWindow(playDialog);
		return NULL;
    }
	UpdateWindow(hwnd);
	UpdateWindow(playDialog);

    return file;
}
