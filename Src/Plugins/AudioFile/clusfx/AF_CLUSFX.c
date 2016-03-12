/*
 * CLUSFX plug-in source code
 * (Revolution CLU sounds: Broken Sword 2)
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

#include <string.h>

#include <windows.h>

#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "resource.h"

#define BUFFERSIZE (64000ul)

typedef struct tagData
{
    WORD  channels,bits,align;
    DWORD rate;
	SHORT CurSample;
	BYTE *buffer;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

void __stdcall Init(void)
{
	DisableThreadLibraryCalls(Plugin.hDllInst);
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

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	SHORT sample;
	DWORD read;

    if (f==NULL)
		return FALSE;

    if (
		(!CheckExt(f->node->rfDataString,"CLUSFX")) &&
		(!CheckExt(f->node->rfName,"CLUSFX"))
	   )
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	Plugin.fsh->SetFilePointer(f,0L,FILE_BEGIN);
	Plugin.fsh->ReadFile(f,&sample,sizeof(sample),&read);
	if (read<sizeof(sample))
	{
		SetLastError(PRIVEC(IDS_INITFAILED));
		return FALSE;
	}
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
	LPData(f)->buffer=(BYTE*)GlobalAlloc(GPTR,BUFFERSIZE);
	if (LPData(f)->buffer==NULL)
	{
		GlobalFree(f->afData);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
	}
	LPData(f)->channels=*channels=1;
	LPData(f)->bits=*bits=16;
    LPData(f)->rate=*rate=22050;
    LPData(f)->align=(LPData(f)->channels)*((LPData(f)->bits)/8);
	LPData(f)->CurSample=sample;

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

    return (
			(GlobalFree(LPData(f)->buffer)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;

    if (f==NULL)
		return NULL;

    if (
		(!CheckExt(rfDataString,"CLUSFX")) &&
		(!CheckExt(rfName,"CLUSFX"))
	   )
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
    FSHandle *f;
	DWORD     size;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

    size=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);

	return MulDiv(1000,(size-2),22050*1);
}

DWORD Decode_CLU_ADPCM(FSHandle* file, BYTE* inbuf, DWORD size, short* outbuf)
{
	DWORD  i;

	if (file==NULL)
		return 0L;

	for (i=0;i<size;i++)
	{
		if (inbuf[i] & 0x8)
			LPData(file)->CurSample-=((SHORT)(inbuf[i] & 7)) << (inbuf[i]>>4);
		else
			LPData(file)->CurSample+=((SHORT)(inbuf[i] & 7)) << (inbuf[i]>>4);
		*outbuf++=LPData(file)->CurSample;
	}

	return 2*size;
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read,pcmSize,toRead,decoded;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;
	
	pcmSize=0;
	toRead=min(buffsize/2,BUFFERSIZE);
	if (LPData(file)->align/2>0)
		toRead=CORRALIGN(toRead,LPData(file)->align/2);
	while ((!(Plugin.fsh->EndOfFile(file))) && (toRead>0))
	{
		Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
		if (read>0)
			decoded=Decode_CLU_ADPCM(file,LPData(file)->buffer,read,(short*)(buffer+pcmSize));
		else
			break; // ???
		pcmSize+=decoded;
		buffsize-=decoded;
		toRead=min(buffsize/2,BUFFERSIZE);
		if (LPData(file)->align/2>0)
			toRead=CORRALIGN(toRead,LPData(file)->align/2);
	}
	*buffpos=-1;

    return pcmSize;
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
			wsprintf(str,"Format: CLU ADPCM\r\n"
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
    char str[256];
    FSHandle* f;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (
		(!CheckExt(node->rfDataString,"CLUSFX")) &&
		(!CheckExt(node->rfName,"CLUSFX"))
	   )
    {
		wsprintf(str,"This is not %s file.",Plugin.afID);
		MessageBox(hwnd,str,Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		Plugin.fsh->CloseFile(f);
		return;
    }
	ibRate=22050;
	ibChannels=1;
	ibBits=16;
    ibSize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);
    ibNode=node;
    if (node->afTime!=-1)
		ibTime=node->afTime;
    else
		ibTime=GetTime(node);
    DialogBox(Plugin.hDllInst,"InfoBox",hwnd,InfoBoxDlgProc);
}

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX CLUSFX Audio File Decoder",
    "v0.80",
    "Revolution Cluster SFX Audio Files (*.CLUSFX)\0*.clusfx\0",
    "CLUSFX",
    0,
    NULL,
    NULL,
    NULL,
    About,
    Init,
    NULL,
    InfoBox,
    InitPlayback,
    ShutdownPlayback,
    FillPCMBuffer,
    NULL,
    CreateNodeForFile,
    GetTime,
    NULL
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
