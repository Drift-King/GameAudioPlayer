/*
 * WADSFX plug-in source code
 * (id Software WAD sounds: Doom/Doom2/Heretic/Hexen)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code is based on the Unofficial DOOM specs
 * written by Matthew S Fell: msfell@aol.com
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

#include <string.h>

#include <windows.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_WADSFX.h"
#include "resource.h"

BOOL opDisableSearch,
	 opCheckRate;

typedef struct tagData
{
    WORD  channels,bits,align;
    DWORD rate;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opDisableSearch=TRUE;
		opCheckRate=TRUE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"DisableSearch","on",str,40,Plugin.szINIFileName);
    opDisableSearch=LoadOptionBool(str);
	Plugin.numPatterns=(opDisableSearch)?(0L):1;
	GetPrivateProfileString(Plugin.Description,"CheckRate","on",str,40,Plugin.szINIFileName);
    opCheckRate=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"DisableSearch",SaveOptionBool(opDisableSearch),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckRate",SaveOptionBool(opCheckRate),Plugin.szINIFileName);
}

BOOL ReadHeader(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits, WORD* nsamples)
{
    DWORD read;
	SBSoundHeader sbsndhdr;

    Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(f,&sbsndhdr,sizeof(SBSoundHeader),&read);
    if (read<sizeof(SBSoundHeader))
		return FALSE;
    if (
		(memcmp(sbsndhdr.id,IDSTR_3,2)) || // ???
		(memcmp(sbsndhdr.dummy,IDSTR_0,2)) // ???
       )
		return FALSE;
	if ((sbsndhdr.rate<7000) || (sbsndhdr.rate>48000)) // check rate ???
		return FALSE;
	if ((sbsndhdr.nSamples+sizeof(SBSoundHeader))!=Plugin.fsh->GetFileSize(f)) // check size ???
		return FALSE;
    *rate=sbsndhdr.rate;
    *channels=1; // ???
    *bits=8; // ???
	*nsamples=sbsndhdr.nSamples;

    return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	WORD  nsamples;

    if (f==NULL)
		return FALSE;

    if (!ReadHeader(f,rate,channels,bits,&nsamples))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
    Plugin.fsh->SetFilePointer(f,sizeof(SBSoundHeader),FILE_BEGIN);
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
    LPData(f)->align=(*channels)*((*bits)/8);
    LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
    LPData(f)->rate=*rate;

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

    return (GlobalFree(f->afData)==NULL);
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate;
	WORD   channels,bits,nsamples;

    if (f==NULL)
		return NULL;

    if (!ReadHeader(f,&rate,&channels,&bits,&nsamples))
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
}

DWORD __stdcall GetTime(AFile* node)
{
    DWORD rate;
    FSHandle* f;
    WORD  channels,bits,nsamples;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

    if (!ReadHeader(f,&rate,&channels,&bits,&nsamples))
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
    Plugin.fsh->CloseFile(f);

	return MulDiv(8000,nsamples,rate*channels*bits);
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	Plugin.fsh->ReadFile(file,buffer,CORRALIGN(buffsize,LPData(file)->align),&read);
	*buffpos=-1;

    return read;
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

void __stdcall About(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"About",hwnd,AboutDlgProc);
}

AFile* ibNode;
DWORD  ibSize,ibRate,ibTime;
WORD   ibChannels,ibBits;

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

LRESULT CALLBACK InfoBoxDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    char str[512];

    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetDlgItemText(hwnd,ID_RESNAME,ibNode->rfName);
			SetDlgItemText(hwnd,ID_DATASTR,ibNode->rfDataString);
			wsprintf(str,"%lu",ibSize);
			SetDlgItemText(hwnd,ID_FILESIZE,str);
			if (ibTime!=-1)
				GetShortTimeString(ibTime/1000,str);
			else
				lstrcpy(str,"N/A");
			SetDlgItemText(hwnd,ID_LENGTH,str);
			wsprintf(str,"Format: PCM\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 ibChannels,
						 (ibChannels==4)?"(quadro)":((ibChannels==2)?"(stereo)":((ibChannels==1)?"(mono)":"")),
						 ibRate,
						 ibBits
					);
			SetDlgItemText(hwnd,ID_DATAFMT,str);
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

void __stdcall InfoBox(AFile* node, HWND hwnd)
{
	WORD nsamples;
    char str[256];
    FSHandle* f;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadHeader(f,&ibRate,&ibChannels,&ibBits,&nsamples))
    {
		wsprintf(str,"This is not %s file.",Plugin.afID);
		MessageBox(hwnd,str,Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		Plugin.fsh->CloseFile(f);
		return;
    }
    ibSize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);
    ibNode=node;
    if (node->afTime!=-1)
		ibTime=node->afTime;
    else
		ibTime=GetTime(node);
    DialogBox(Plugin.hDllInst,"InfoBox",hwnd,InfoBoxDlgProc);
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    SBSoundHeader sbsndhdr;
    DWORD      read;
    AFile     *node;

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying WADSFX file...");
    RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
    RFPLUGIN(f)->ReadFile(f,&sbsndhdr,sizeof(SBSoundHeader),&read);
	if (read<sizeof(SBSoundHeader))
		return NULL;
    if (
		(memcmp(sbsndhdr.id,IDSTR_3,2)) || // ???
		(memcmp(sbsndhdr.dummy,IDSTR_0,2)) // ???
       )
		return NULL;
	if ((sbsndhdr.rate<7000) || (sbsndhdr.rate>48000))
		return NULL;
	if (opCheckRate)
	{
		if ((sbsndhdr.rate!=11025) && (sbsndhdr.rate!=22050))
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=sbsndhdr.nSamples+sizeof(SBSoundHeader); // ???
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"WADSFX file correct.");

    return node;
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
			SetCheckBox(hwnd,ID_NOSEARCH,opDisableSearch);
			SetCheckBox(hwnd,ID_CHECKRATE,opCheckRate);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_NOSEARCH,TRUE);
					SetCheckBox(hwnd,ID_CHECKRATE,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opDisableSearch=GetCheckBox(hwnd,ID_NOSEARCH);
					Plugin.numPatterns=(opDisableSearch)?(0L):1;
					opCheckRate=GetCheckBox(hwnd,ID_CHECKRATE);
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

void __stdcall Seek(FSHandle* f, DWORD pos)
{
    DWORD filepos;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

    filepos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
    Plugin.fsh->SetFilePointer(f,sizeof(SBSoundHeader)+CORRALIGN(filepos,LPData(f)->align),FILE_BEGIN);
}

SearchPattern patterns[]=
{
	{2,IDSTR_3}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX WADSFX Audio File Plug-In",
    "v0.91",
    "ID Software WAD SFX Audio Files (*.WADSFX)\0*.wadsfx\0",
    "WADSFX",
    1,
    patterns,
    NULL,
    Config,
    About,
    Init,
    Quit,
    InfoBox,
    InitPlayback,
    ShutdownPlayback,
    FillPCMBuffer,
    CreateNodeForID,
    CreateNodeForFile,
    GetTime,
    Seek
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
