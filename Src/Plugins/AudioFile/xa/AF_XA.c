/*
 * XA plug-in source code
 * (Maxis music/sfx/speech: SimCity3000)
 *
 * Copyright (C) 2000-2001 ANX Software
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

#include <string.h>

#include <windows.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_XA.h"
#include "resource.h"

typedef struct tagDecState_EA_ADPCM
{
	long lPrevSample,lCurSample,lPrevCoeff,lCurCoeff;
	BYTE bShift;
} DecState_EA_ADPCM;

typedef struct tagData
{
	WORD    channels,bits,align;
	DWORD   rate,blocksize;
	char   *buffer;
	DecState_EA_ADPCM dsLeft,dsRight;
} Data;

#define LPData(f) ((Data*)((f)->afData))

#define BUFFERBLOCKS (4000ul)

AFPlugin Plugin;

BOOL opCheckTag,
	 opCheckChannels,
	 opCheckRate,
	 opCheckAvgRate,
	 opCheckAlign,
	 opCheckBits,
	 opUseClipping;

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
		opCheckTag=FALSE;
		opCheckChannels=TRUE;
		opCheckRate=TRUE;
		opCheckAvgRate=TRUE;
		opCheckAlign=TRUE;
		opCheckBits=TRUE;
		opUseClipping=TRUE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"CheckTag","off",str,40,Plugin.szINIFileName);
    opCheckTag=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckChannels","on",str,40,Plugin.szINIFileName);
    opCheckChannels=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckRate","on",str,40,Plugin.szINIFileName);
    opCheckRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckAvgRate","on",str,40,Plugin.szINIFileName);
    opCheckAvgRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckAlign","on",str,40,Plugin.szINIFileName);
    opCheckAlign=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckBits","on",str,40,Plugin.szINIFileName);
    opCheckBits=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"CheckTag",SaveOptionBool(opCheckTag),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckChannels",SaveOptionBool(opCheckChannels),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckRate",SaveOptionBool(opCheckRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckAvgRate",SaveOptionBool(opCheckAvgRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckAlign",SaveOptionBool(opCheckAlign),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckBits",SaveOptionBool(opCheckBits),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
}

BOOL ReadXAHeader
(
	FSHandle* file, 
	char*  type,
	DWORD* rate, 
	WORD*  channels, 
	WORD*  bits, 
	DWORD* outsize
)
{
	XAHeader hdr;
	DWORD    read;

	if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&hdr,sizeof(XAHeader),&read);
	if (
		(read<sizeof(XAHeader)) || 
		(
			(memcmp(hdr.szID,IDSTR_XAI,4)) &&
			(memcmp(hdr.szID,IDSTR_XAJ,4))
		) ||
		((opCheckTag) && (hdr.wTag!=0x0001)) ||
		((opCheckChannels) && ((hdr.wChannels<1) || (hdr.wChannels>2))) ||
		((opCheckRate) && ((hdr.dwRate<4000) || (hdr.dwRate>96000))) ||
		((opCheckAvgRate) && (hdr.dwAvgByteRate!=hdr.dwRate*hdr.wAlign)) ||
		((opCheckAlign) && (hdr.wAlign!=(hdr.wChannels*hdr.wBits)/8)) ||
		((opCheckBits) && ((hdr.wBits<8) || (hdr.wBits>16)))
	   )
		return FALSE; // ???

	lstrcpy(type,hdr.szID);
	*outsize=hdr.dwOutSize;
	*rate=hdr.dwRate;
	*bits=hdr.wBits;
	*channels=hdr.wChannels;

	return TRUE;
}

void Init_EA_ADPCM(DecState_EA_ADPCM *pDS, long curSample, long prevSample)
{
	pDS->lCurSample=curSample;
	pDS->lPrevSample=prevSample;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	char  type[4];
	DWORD outsize;

    if (f==NULL)
		return FALSE;

    if (!ReadXAHeader(f,type,rate,channels,bits,&outsize))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	f->afData=GlobalAlloc(GPTR,sizeof(Data));
	if (f->afData==NULL)
	{
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
	}
	LPData(f)->buffer=(char*)GlobalAlloc(GPTR,(*channels)*0xF*BUFFERBLOCKS);
	if (LPData(f)->buffer==NULL)
	{
		GlobalFree(f->afData);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
	}
	LPData(f)->align=(*channels)*((*bits)/8);
	LPData(f)->rate=*rate;
	LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->blocksize=(*channels)*0xF;
	Init_EA_ADPCM(&(LPData(f)->dsLeft),0L,0L);
	Init_EA_ADPCM(&(LPData(f)->dsRight),0L,0L);

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

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    AFile*	 node;
	FSHandle fs;
	char     type[4];
	WORD     channels,bits;
	DWORD    rate,outsize,size,blocksize;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // XAI
		case 1: // XAJ
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying XA file...");
			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			fs.rf=f;
			fs.start=pos;
			fs.length=sizeof(XAHeader);
			fs.node=NULL;
			if (!ReadXAHeader(&fs,type,&rate,&channels,&bits,&outsize))
				return NULL;
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"XA file correct.");
			break;
		default: // ???
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
	size=outsize/4;
	blocksize=0xE*channels;
	size=(size/blocksize)*(blocksize+channels)+(size%blocksize+(size%blocksize)?channels:0);
    node->fsLength=sizeof(XAHeader)+size;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
	char   type[4];
	WORD   channels,bits;
	DWORD  rate,outsize;

    if (f==NULL)
		return NULL;

    if (!ReadXAHeader(f,type,&rate,&channels,&bits,&outsize))
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
	char  type[4];
	WORD  channels,bits;
	DWORD rate,outsize;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!ReadXAHeader(f,type,&rate,&channels,&bits,&outsize))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	Plugin.fsh->CloseFile(f);

	return MulDiv(8000,outsize,rate*bits*channels);
}

void Clip16BitSample(long *sample)
{
	if ((*sample)>32767)
		(*sample)=32767;
    else if ((*sample)<-32768)
		(*sample)=-32768;
}

void BlockInit_EA_ADPCM(DecState_EA_ADPCM *pDS, BYTE cCode, BYTE sCode)
{
	pDS->lCurCoeff=EATable[cCode];
	pDS->lPrevCoeff=EATable[cCode+4];
	pDS->bShift=sCode+8;
}

void Step_EA_ADPCM(DecState_EA_ADPCM *pDS, BYTE code)
{
	long value;

	value=(code<<0x1C)>>(pDS->bShift);
	value=(value+(pDS->lCurSample)*(pDS->lCurCoeff)+(pDS->lPrevSample)*(pDS->lPrevCoeff)+0x80)>>8;
	pDS->lPrevSample=pDS->lCurSample;
	pDS->lCurSample=value;
}

DWORD Decode_EA_ADPCM(FSHandle* f, char* chunk, DWORD size, short* buff)
{
	DWORD i;

	if (LPData(f)->channels==2)
	{
		for (i=0;i<size;i++)
		{
			if (i%2==0)
			{
				Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]));
				if (opUseClipping)
					Clip16BitSample(&(LPData(f)->dsLeft.lCurSample));
				buff[2*i]=(short)(LPData(f)->dsLeft.lCurSample);
				Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)LONIBBLE(chunk[i]));
				if (opUseClipping)
					Clip16BitSample(&(LPData(f)->dsLeft.lCurSample));
				buff[2*i+2]=(short)(LPData(f)->dsLeft.lCurSample);
			}
			else
			{
				Step_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)HINIBBLE(chunk[i]));
				if (opUseClipping)
					Clip16BitSample(&(LPData(f)->dsRight.lCurSample));
				buff[2*i-1]=(short)(LPData(f)->dsRight.lCurSample);
				Step_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)LONIBBLE(chunk[i]));
				if (opUseClipping)
					Clip16BitSample(&(LPData(f)->dsRight.lCurSample));
				buff[2*i+1]=(short)(LPData(f)->dsRight.lCurSample);
			}
		}
	}
	else
	{
		for (i=0;i<size;i++)
		{
			Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->dsLeft.lCurSample));
			buff[2*i]=(short)(LPData(f)->dsLeft.lCurSample);
			Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)LONIBBLE(chunk[i]));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->dsLeft.lCurSample));
			buff[2*i+1]=(short)(LPData(f)->dsLeft.lCurSample);
		}
	}

	return 4*size;
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD i,read,toRead,pcmSize,decoded;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	pcmSize=0;
	toRead=min(CORRALIGN(buffsize/4,LPData(file)->blocksize),LPData(file)->blocksize*BUFFERBLOCKS);
	while (
			(!(Plugin.fsh->EndOfFile(file))) && 
			(toRead>0)
		  )
	{
		Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
		if (read==0)
			break;
		for (i=0;i<read;)
		{
			BlockInit_EA_ADPCM
			(
				&(LPData(file)->dsLeft),
				(BYTE)HINIBBLE(LPData(file)->buffer[i]),
				(BYTE)LONIBBLE(LPData(file)->buffer[i])
			);
			i++;
			if (LPData(file)->channels==2)
			{
				BlockInit_EA_ADPCM
				(
					&(LPData(file)->dsRight),
					(BYTE)HINIBBLE(LPData(file)->buffer[i]),
					(BYTE)LONIBBLE(LPData(file)->buffer[i])
				);
				i++;
			}
			decoded=Decode_EA_ADPCM
			(
				file,
				LPData(file)->buffer+i,
				(LPData(file)->blocksize)-(LPData(file)->channels),
				(short*)(buffer+pcmSize)
			);
			i+=(LPData(file)->blocksize)-(LPData(file)->channels);
			pcmSize+=decoded;
			buffsize-=decoded;
		}
		toRead=min(CORRALIGN(buffsize/4,LPData(file)->blocksize),LPData(file)->blocksize*BUFFERBLOCKS);
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

AFile* ibNode;
DWORD  ibSize,ibRate,ibTime;
char   ibType[4];
WORD   ibChannels,ibBits;

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
			wsprintf(str,"Format: %s (%s)\r\n"
						 "Compression: EA ADPCM\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 ibType,
						 (ibType[2]=='I')?"sound":((ibType[2]=='J')?"music":"unknown"),
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
			SetCheckBox(hwnd,ID_CHECKTAG,opCheckTag);
			SetCheckBox(hwnd,ID_CHECKCHANNELS,opCheckChannels);
			SetCheckBox(hwnd,ID_CHECKRATE,opCheckRate);
			SetCheckBox(hwnd,ID_CHECKAVGRATE,opCheckAvgRate);
			SetCheckBox(hwnd,ID_CHECKALIGN,opCheckAlign);
			SetCheckBox(hwnd,ID_CHECKBITS,opCheckBits);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_CHECKTAG,FALSE);
					SetCheckBox(hwnd,ID_CHECKCHANNELS,TRUE);
					SetCheckBox(hwnd,ID_CHECKRATE,TRUE);
					SetCheckBox(hwnd,ID_CHECKAVGRATE,TRUE);
					SetCheckBox(hwnd,ID_CHECKALIGN,TRUE);
					SetCheckBox(hwnd,ID_CHECKBITS,TRUE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opCheckTag=GetCheckBox(hwnd,ID_CHECKTAG);
					opCheckChannels=GetCheckBox(hwnd,ID_CHECKCHANNELS);
					opCheckRate=GetCheckBox(hwnd,ID_CHECKRATE);
					opCheckAvgRate=GetCheckBox(hwnd,ID_CHECKAVGRATE);
					opCheckAlign=GetCheckBox(hwnd,ID_CHECKALIGN);
					opCheckBits=GetCheckBox(hwnd,ID_CHECKBITS);
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

void __stdcall InfoBox(AFile* node, HWND hwnd)
{
	FSHandle* f;
    char  str[256];
	DWORD outsize;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadXAHeader(f,ibType,&ibRate,&ibChannels,&ibBits,&outsize))
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
	{4,IDSTR_XAI},
	{4,IDSTR_XAJ}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX XA Audio File Decoder",
    "v0.80",
	"Maxis Audio Files (*.XA)\0*.xa\0",
    "XA",
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
