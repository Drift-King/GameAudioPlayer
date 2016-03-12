/*
 * BLBSFX plug-in source code
 * (DreamWorks music/sfx: Neverhood)
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

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_BLBSFX.h"
#include "resource.h"

#define BUFFERSIZE (64000ul)

typedef struct tagData
{
	BYTE  bShift;
	SHORT curValue;
    WORD  channels,bits,align;
    DWORD rate;
	char *buffer;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

void __stdcall Init(void)
{
	DisableThreadLibraryCalls(Plugin.hDllInst);
}

BOOL GetInfo
(
	FSHandle* f,
	char*  rfDataString, 
	DWORD* rate, 
	WORD*  channels, 
	WORD*  bits, 
	BYTE*  shift
)
{
	char *mark,*endptr;

	if (lstrcmp(RFPLUGIN(f->rf)->rfID,"BLB"))
		return FALSE;
	mark=strrchr(rfDataString,'.');
	if (mark==NULL)
		return FALSE;
	switch ((BYTE)strtoul(mark+1,&endptr,0x10))
	{
		case ID_BLB_TYPE_SOUND:
		case ID_BLB_TYPE_MUSIC:
			break;
		default:
			return FALSE;
	}
	*rate=22050; // ???
	*channels=1; // ???
	*bits=16;    // ???
	mark=strchr(rfDataString,'-');
	*shift=(mark==NULL)?0xFF:((BYTE)strtoul(mark+1,&endptr,0x10));

    return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	BYTE shift;

    if ((f==NULL) || (f->node==NULL))
		return FALSE;

    if (!GetInfo(f,f->node->rfDataString,rate,channels,bits,&shift))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
    Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
	if (shift!=0xFF)
	{
		LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
		if (LPData(f)->buffer==NULL)
		{
			GlobalFree(f->afData);
			SetLastError(PRIVEC(IDS_NOBUFFER));
			return FALSE;
		}
	}
	else
		LPData(f)->buffer=NULL;
    LPData(f)->align=(*channels)*((*bits)/8);
    LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
    LPData(f)->rate=*rate;
	LPData(f)->bShift=shift;
	LPData(f)->curValue=0x0000;

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
	BYTE   shift;
    DWORD  rate;
	WORD   channels,bits;

    if (f==NULL)
		return NULL;

    if (!GetInfo(f,(char*)rfDataString,&rate,&channels,&bits,&shift))
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
	FSHandle* f;
	BYTE  shift;
    DWORD rate,size;
    WORD  channels,bits;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

    if (!GetInfo(f,node->rfDataString,&rate,&channels,&bits,&shift))
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	size=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);

	return MulDiv(8000,((shift==0xFF)?1:2)*size,rate*channels*bits);
}

DWORD Decode_DW_ADPCM(FSHandle* file, char* inbuf, DWORD size, char* outbuf)
{
	DWORD  i;
	short *pcm16;

	if (file==NULL)
		return 0L;

	pcm16=(short*)outbuf;
	for (i=0;i<size;i++)
	{
		LPData(file)->curValue+=(signed short)inbuf[i];
		*pcm16++=(LPData(file)->curValue)<<(LPData(file)->bShift);
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

	switch (LPData(file)->bShift)
	{
		case 0xFF:
			Plugin.fsh->ReadFile(file,buffer,CORRALIGN(buffsize,LPData(file)->align),&pcmSize);
			break;
		default:
			pcmSize=0;
			toRead=min(buffsize/2,BUFFERSIZE);
			if (LPData(file)->align/2>0)
				toRead=CORRALIGN(toRead,LPData(file)->align/2);
			while ((!(Plugin.fsh->EndOfFile(file))) && (toRead>0))
			{
				Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
				if (read>0)
					decoded=Decode_DW_ADPCM(file,LPData(file)->buffer,read,buffer+pcmSize);
				else
					break; // ???
				pcmSize+=decoded;
				buffsize-=decoded;
				toRead=min(buffsize/2,BUFFERSIZE);
				if (LPData(file)->align/2>0)
					toRead=CORRALIGN(toRead,LPData(file)->align/2);
			}
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
BYTE   ibShift;
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
			wsprintf(str,"Compression: %s\r\n"
						 "Shift: 0x%02X\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (ibShift==0xFF)?"None":"DW ADPCM",
						 ibShift,
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
	FSHandle* f;
    char str[256];

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!GetInfo(f,node->rfDataString,&ibRate,&ibChannels,&ibBits,&ibShift))
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

void __stdcall Seek(FSHandle* f, DWORD pos)
{
    DWORD filepos;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

    filepos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
	filepos=CORRALIGN(filepos,LPData(f)->align);
    Plugin.fsh->SetFilePointer(f,(LPData(f)->bShift==0xFF)?filepos:(filepos/2),FILE_BEGIN);
	LPData(f)->curValue=0x0000; // ???
}

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX BLBSFX Audio File Decoder",
    "v1.00",
    "DreamWorks BLB SFX Audio Files (*.07;*.08)\0*.07;*.08\0",
    "BLBSFX",
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
    Seek
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
