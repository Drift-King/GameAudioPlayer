/*
 * BNK plug-in source code
 * (Electronic Arts sfx/speech)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code is based on the WVE2PCM converter written by
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

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_BNK.h"
#include "resource.h"

#define BUFFERSIZE (64000ul)

typedef struct tagData
{
	WORD   channels,bits,align;
	BYTE   compression;
	DWORD  rate;
    DWORD  samplesLeft;
	DWORD  dataStart,numSamples;
    long   PrevSample1;
    long   CurSample1;
	long   PrevSample2;
    long   CurSample2;
    char  *buffer;
} Data;

BOOL opUseClipping;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

long EATable[]=
{
	0x00000000,
	0x000000F0,
	0x000001CC,
	0x00000188,
	0x00000000,
	0x00000000,
	0xFFFFFF30,
	0xFFFFFF24,
	0x00000000,
	0x00000001,
	0x00000003,
	0x00000004,
	0x00000007,
	0x00000008,
	0x0000000A,
	0x0000000B,
	0x00000000,
	0xFFFFFFFF,
	0xFFFFFFFD,
	0xFFFFFFFC
};

#define LONIBBLE(b) ((b)&0x0F)
#define HINIBBLE(b) (((b)&0xF0)>>4)

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opUseClipping=TRUE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
}

DWORD ReadBytes(FSHandle* file, BYTE count)
{
	BYTE  i,byte;
	DWORD result,read;

	result=0;
	for (i=0;i<count;i++)
	{
		Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
		if (read<sizeof(BYTE))
			break;
		result<<=8;
		result+=byte;
	}

	return result;
}

BOOL ParsePTHeader
(
   FSHandle* file,
   DWORD* rate,
   WORD* channels,
   WORD* bits,
   BYTE* compression,
   DWORD* numsamples,
   DWORD* datastart
)
{
	char  id[4];
	DWORD read;
	BYTE  byte;
	BOOL  flag,subflag;

	*rate=22050; // ???
	*channels=1; // ???
	*bits=16; // ???
	*numsamples=0;
	*datastart=-1;
	*compression=-1;
	lstrcpyn(id,"\0\0\0",4);
	Plugin.fsh->ReadFile(file,id,4,&read);
	if (memcmp(id,IDSTR_PT,4))
		return FALSE;
	flag=TRUE;
	while (flag)
	{
		Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
		if (read<sizeof(BYTE))
			break;
		switch (byte)
		{
			case 0xFF:
				flag=FALSE;
			case 0xFE:
			case 0xFC:
				break;
			case 0xFD:
				subflag=TRUE;
				while (subflag)
				{
					Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
					if (read<sizeof(BYTE))
						break;
					switch (byte)
					{
						case 0x85:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*numsamples=ReadBytes(file,byte);
							break;
						case 0x82:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*channels=(WORD)ReadBytes(file,byte);
							break;
						case 0x83:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*compression=(BYTE)ReadBytes(file,byte); // ???
							break;
						case 0x84:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*rate=ReadBytes(file,byte);
							break;
						case 0x88:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*datastart=ReadBytes(file,byte);
							break;
						case 0x8A:
							subflag=FALSE;
						case 0x86: // ???
						case 0x87: // ???
						case 0x91: // ???
						case 0x92: // ???
						default: // ???
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							Plugin.fsh->SetFilePointer(file,byte,FILE_CURRENT);
					}
				}
				break;
			default:
				Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
				if (read<sizeof(BYTE))
					break;
				if (byte==0xFF)
					Plugin.fsh->SetFilePointer(file,4,FILE_CURRENT);
				Plugin.fsh->SetFilePointer(file,byte,FILE_CURRENT);
		}
	}

	return TRUE;
}

BOOL ReadHeader
(
	FSHandle* file,
	DWORD* rate,
	WORD* channels,
	WORD* bits,
	BYTE* compression,
	DWORD* numsamples,
	DWORD* datastart
)
{
    DWORD read,pos;
	char  id[4],*endptr;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
	Plugin.fsh->ReadFile(file,id,4,&read);
	if (!memcmp(id,IDSTR_BNKl,4))
	{
		if (file->node==NULL)
			return FALSE;
		pos=strtoul(file->node->afDataString,&endptr,0x10);
		if (pos==-1)
			return FALSE;
		Plugin.fsh->SetFilePointer(file,pos,FILE_BEGIN);
	}
	else if (!memcmp(id,IDSTR_PT,4))
		Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
	else
		return FALSE;
	if (!ParsePTHeader(file,rate,channels,bits,compression,numsamples,datastart))
		return FALSE;
	if ((*datastart==-1) || (*numsamples==0))
		return FALSE;

    return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	DWORD nsamp,datastart;
	BYTE  compression;

    if (f==NULL)
		return FALSE;

    if (!ReadHeader(f,rate,channels,bits,&compression,&nsamp,&datastart))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	switch (compression)
	{
		case SC_EA_ADPCM:
			break;
		default:
			SetLastError(PRIVEC(IDS_NODECODER));
			return FALSE;
			break;
	}
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
    LPData(f)->samplesLeft=nsamp;
    LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
    if (LPData(f)->buffer==NULL)
    {
		GlobalFree(f->afData);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
    }
	LPData(f)->align=(*channels)*(*bits/8);
    LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->rate=*rate;
	LPData(f)->compression=compression;
	Plugin.fsh->SetFilePointer(f,datastart,FILE_BEGIN);
	LPData(f)->dataStart=datastart;
	LPData(f)->numSamples=nsamp;
	LPData(f)->PrevSample1=0;
	LPData(f)->PrevSample2=0;
	LPData(f)->CurSample1=0;
	LPData(f)->CurSample2=0;

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

void Clip16bitSample(long *sample)
{
	if ((*sample)>32767)
		(*sample)=32767;
    else if ((*sample)<-32768)
		(*sample)=-32768;
}

DWORD DecodeBlock(FSHandle* f, char* chunk, DWORD size, char* buff)
{
    DWORD  i,outsize;
    short *dstpcm16;

	long c1left,c2left,c1right,c2right,left,right;
	BYTE dleft,dright;

    if (f==NULL)
		return 0;

	switch (LPData(f)->compression)
	{
		case SC_EA_ADPCM:
			dstpcm16=(short*)buff;
			outsize=0;
			i=0;
			switch (LPData(f)->channels)
			{
				case 2:
					c1left=EATable[HINIBBLE(chunk[i])];
					c2left=EATable[HINIBBLE(chunk[i])+4];
					c1right=EATable[LONIBBLE(chunk[i])];
					c2right=EATable[LONIBBLE(chunk[i])+4];
					i++;
					if (i>=size)
						break;
					dleft=HINIBBLE(chunk[i])+8;
					dright=LONIBBLE(chunk[i])+8;
					i++;
					break;
				case 1:
					c1left=EATable[HINIBBLE(chunk[i])];
					c2left=EATable[HINIBBLE(chunk[i])+4];
					dleft=LONIBBLE(chunk[i])+8;
					i++;
					break;
				default: // ???
					break;
			}
			for (;i<size;i++)
			{
				switch (LPData(f)->channels)
				{
					case 2:
						left=HINIBBLE(chunk[i]);
						right=LONIBBLE(chunk[i]);
						left=(left<<0x1c)>>dleft;
						right=(right<<0x1c)>>dright;
						left=(left+(LPData(f)->CurSample1)*c1left+(LPData(f)->PrevSample1)*c2left+0x80)>>8;
						right=(right+(LPData(f)->CurSample2)*c1right+(LPData(f)->PrevSample2)*c2right+0x80)>>8;
						if (opUseClipping)
						{
							Clip16bitSample(&left);
							Clip16bitSample(&right);
						}
						LPData(f)->PrevSample1=LPData(f)->CurSample1;
						LPData(f)->CurSample1=left;
						LPData(f)->PrevSample2=LPData(f)->CurSample2;
						LPData(f)->CurSample2=right;
						*dstpcm16++=(short)left;
						*dstpcm16++=(short)right;
						outsize+=1;
						break;
					case 1:
						left=HINIBBLE(chunk[i]);
						left=(left<<0x1c)>>dleft;
						left=(left+(LPData(f)->CurSample1)*c1left+(LPData(f)->PrevSample1)*c2left+0x80)>>8;
						if (opUseClipping)
							Clip16bitSample(&left);
						LPData(f)->PrevSample1=LPData(f)->CurSample1;
						LPData(f)->CurSample1=left;
						*dstpcm16++=(short)left;

						left=LONIBBLE(chunk[i]);
						left=(left<<0x1c)>>dleft;
						left=(left+(LPData(f)->CurSample1)*c1left+(LPData(f)->PrevSample1)*c2left+0x80)>>8;
						if (opUseClipping)
							Clip16bitSample(&left);
						LPData(f)->PrevSample1=LPData(f)->CurSample1;
						LPData(f)->CurSample1=(short)left;
						*dstpcm16++=(short)left;

						outsize+=2;
						break;
					default: // ???
						break;
				}
			}
			break;
		default: // ???
			outsize=0;
			break;
	}

    return outsize;
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
	static WORD  num=0;
	static DWORD startpos,curpos;
    DWORD  read,start,length;
    AFile* node;
	char   id[4];

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying BNK file section...");
	switch (ipattern)
	{
		case 0: // BNKl
			startpos=pos;
			RFPLUGIN(f)->SetFilePointer(f,pos+6,FILE_BEGIN);
			RFPLUGIN(f)->ReadFile(f,&num,sizeof(WORD),&read);
			if (read<sizeof(WORD))
			{
				num=0;
				return NULL;
			}
			RFPLUGIN(f)->SetFilePointer(f,4,FILE_CURRENT);
			curpos=RFPLUGIN(f)->GetFilePointer(f);
			RFPLUGIN(f)->ReadFile(f,&start,sizeof(DWORD),&read);
			if (read<sizeof(DWORD))
			{
				num=0;
				return NULL;
			}
			RFPLUGIN(f)->SetFilePointer(f,start-4,FILE_CURRENT);
			RFPLUGIN(f)->ReadFile(f,id,4,&read);
			if (read<4)
			{
				num=0;
				return NULL;
			}
			if (memcmp(id,IDSTR_PT,4))
				num=0;
			return NULL;
		case 1: // PT
			if (num==0)
				return NULL;
			RFPLUGIN(f)->SetFilePointer(f,curpos,FILE_BEGIN);
			do
			{
				RFPLUGIN(f)->ReadFile(f,&start,sizeof(DWORD),&read);
				if (read<sizeof(DWORD))
					return NULL;
				curpos+=read;
				num--;
			}
			while ((start==0) && (num>0));
			if (start!=pos-curpos+read)
				return NULL;
			RFPLUGIN(f)->SetFilePointer(f,-(LONG)read,FILE_CURRENT);
			start=startpos;
			length=RFPLUGIN(f)->GetFileSize(f)-startpos;
			node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
			node->fsStart=start;
			node->fsLength=length;
			wsprintf(node->afDataString,"%lX",pos-startpos);
			lstrcpy(node->afName,"");
			*newpos=pos+4;
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"BNK file section correct.");
			return node;
		default: // ???
			return NULL;
	}
	return NULL;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate,nsamp,datastart;
	WORD   channels,bits;
    BYTE   compression;

    if (f==NULL)
		return NULL;

    if (!ReadHeader(f,&rate,&channels,&bits,&compression,&nsamp,&datastart))
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
    DWORD rate,nsamp,datastart;
	WORD  channels,bits;
    BYTE  compression;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!ReadHeader(f,&rate,&channels,&bits,&compression,&nsamp,&datastart))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
	}
	Plugin.fsh->CloseFile(f);
	if ((nsamp==0) || (datastart==-1))
		return -1;

	return MulDiv(1000,nsamp,rate);
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read,pcmSize,decoded,toRead;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

    pcmSize=0;
    while (LPData(file)->samplesLeft>0)
    {
		if (Plugin.fsh->EndOfFile(file))
			break;
		if (LPData(file)->samplesLeft<2) // ???
		{
			LPData(file)->samplesLeft=0;
			break;
		}
		toRead=min(0x1c,LPData(file)->samplesLeft);
		if (LPData(file)->align*toRead>buffsize)
			break;
		Plugin.fsh->ReadFile(file,LPData(file)->buffer,((LPData(file)->channels==1)?1:2)+(toRead/((LPData(file)->channels==1)?2:1)),&read);
		if (read>0)
			decoded=DecodeBlock(file,LPData(file)->buffer,read,buffer+pcmSize);
		else
			break; // ???
		pcmSize+=decoded*(LPData(file)->align);
		buffsize-=decoded*(LPData(file)->align);
		LPData(file)->samplesLeft-=decoded;
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
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opUseClipping=GetCheckBox(hwnd,ID_USECLIPPING);
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

AFile* ibNode;
DWORD  ibSize,ibRate,ibTime;
WORD   ibChannels,ibBits;
BYTE   ibCompression;

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
			wsprintf(str,"Format: EA BNK section\r\n"
						 "Compression: %s (%u)\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (ibCompression==SC_EA_ADPCM)?"EA ADPCM":"Unknown",
						 ibCompression,
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
	DWORD nsamp,datastart;
    FSHandle* f;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadHeader(f,&ibRate,&ibChannels,&ibBits,&ibCompression,&nsamp,&datastart))
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

SearchPattern patterns[]=
{
	{4,IDSTR_BNKl},
	{4,IDSTR_PT}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX BNK Audio File Decoder",
    "v0.90",
    "Electronic Arts EAS Audio Files (*.EAS)\0*.eas\0"
    "Electronic Arts BNK Audio Banks (*.BNK)\0*.bnk\0"
	"Electronic Arts VIV Resource Files (*.VIV)\0*.viv\0",
    "BNK",
    2,
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
    NULL
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
