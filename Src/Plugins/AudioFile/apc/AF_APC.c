/*
 * APC plug-in source code
 * (Cryo Interactive music/sfx/speech/video soundtracks: Atlantis, Atlantis 2, etc.)
 *
 * Copyright (C) 2001 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * IMA ADPCM decoder corrections were pointed out by Peter Pawlowski:
 * E-mail: peterpw666@hotmail.com, piotrpw@polbox.com
 * http://www.geocities.com/pp_666/
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

#include "AF_APC.h"
#include "resource.h"

#define BUFFERSIZE (64000ul)

typedef struct tagData
{
	WORD   channels,bits,align;
	DWORD  rate;
	LONG   Index1,Index2;
	LONG   Cur_Sample1,Cur_Sample2;
	char  *buffer;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

BOOL opCheckVersion,
	 opCheckRate,
	 opCheckChannels,
	 opCheckHNM,
	 opUseClipping,
	 opUseEnhancement;

short Index_Adjust[]=
{
    -1,
    -1,
    -1,
    -1,
     2,
     4,
     6,
     8,
	-1,
    -1,
    -1,
    -1,
     2,
     4,
     6,
     8
};

short Step_Table[]=
{
    7,	   8,	  9,	 10,	11,    12,     13,    14,    16,
    17,    19,	  21,	 23,	25,    28,     31,    34,    37,
    41,    45,	  50,	 55,	60,    66,     73,    80,    88,
    97,    107,   118,	 130,	143,   157,    173,   190,   209,
    230,   253,   279,	 307,	337,   371,    408,   449,   494,
    544,   598,   658,	 724,	796,   876,    963,   1060,  1166,
    1282,  1411,  1552,  1707,	1878,  2066,   2272,  2499,  2749,
    3024,  3327,  3660,  4026,	4428,  4871,   5358,  5894,  6484,
    7132,  7845,  8630,  9493,	10442, 11487,  12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086,  29794, 32767
};

#define NIBBLE(b,n) ( (n) ? (((b)&0xF0)>>4) : ((b)&0x0F) )

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opCheckVersion=FALSE;
		opCheckRate=TRUE;
		opCheckChannels=TRUE;
		opCheckHNM=TRUE;
		opUseClipping=TRUE;
		opUseEnhancement=FALSE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"CheckVersion","off",str,40,Plugin.szINIFileName);
    opCheckVersion=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckRate","on",str,40,Plugin.szINIFileName);
    opCheckRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckChannels","on",str,40,Plugin.szINIFileName);
    opCheckChannels=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckHNM","on",str,40,Plugin.szINIFileName);
    opCheckHNM=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseEnhancement","off",str,40,Plugin.szINIFileName);
    opUseEnhancement=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"CheckVersion",SaveOptionBool(opCheckVersion),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckRate",SaveOptionBool(opCheckRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckChannels",SaveOptionBool(opCheckChannels),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckHNM",SaveOptionBool(opCheckHNM),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseEnhancement",SaveOptionBool(opUseEnhancement),Plugin.szINIFileName);
}

BOOL ReadAPCHeader
(
	FSHandle* file, 
	DWORD* rate, 
	WORD*  channels, 
	WORD*  bits, 
	char*  version,
	DWORD* outsize,
	LONG*  sample1,
	LONG*  sample2
)
{
    APCHeader apchdr;
    DWORD     read;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&apchdr,sizeof(APCHeader),&read);
	if (
		(read<sizeof(APCHeader)) || 
		(memcmp(apchdr.szID,IDSTR_APC,8)) || 
		((opCheckVersion) && (memcmp(apchdr.szVersion,IDSTR_VER120,4))) ||
		((opCheckRate) && ((apchdr.dwRate>96000) || (apchdr.dwRate<4000))) ||
		((opCheckChannels) && (apchdr.dwStereo>0x1))
	   )
		return FALSE; // ???

	lstrcpyn(version,apchdr.szVersion,5);
	*outsize=apchdr.dwOutSize;
	*rate=apchdr.dwRate;
	*bits=16;
	*sample1=apchdr.lSample1;
	*sample2=apchdr.lSample2;
	*channels=((apchdr.dwStereo)?2:1);

	return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	char  version[5];
	DWORD outsize;
	LONG  sample1,sample2;

    if (f==NULL)
		return FALSE;

    if (!ReadAPCHeader(f,rate,channels,bits,version,&outsize,&sample1,&sample2))
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
	LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
	if (LPData(f)->buffer==NULL)
	{
		GlobalFree(f->afData);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
	}
	LPData(f)->Index1=0L;
	LPData(f)->Index2=0L;
	LPData(f)->Cur_Sample1=sample1;
	LPData(f)->Cur_Sample2=sample2;
	LPData(f)->align=(*channels)*((*bits)/8);
	LPData(f)->rate=*rate;
	LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;

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

void Step_IMA_ADPCM(BYTE code, long *pindex, long *pcur_sample)
{
	long delta;

	delta=Step_Table[*pindex]>>3;
	if (code & 4)
		delta+=Step_Table[*pindex];
	if (code & 2)
		delta+=Step_Table[*pindex]>>1;
	if (code & 1)
		delta+=Step_Table[*pindex]>>2;
	if (code & 8)
		(*pcur_sample)-=delta;
	else
		(*pcur_sample)+=delta;
	(*pindex)+=Index_Adjust[code];
	if ((*pindex)<0)
		*pindex=0;
    else if ((*pindex)>88)
	    *pindex=88;
}

void Clip16BitSample(long *sample)
{
	if ((*sample)>32767)
		(*sample)=32767;
    else if ((*sample)<-32768)
		(*sample)=-32768;
}

void Enhance16BitSample(long* sample)
{
	if (*sample>0)
		(*sample)--;
	else if (*sample)
		(*sample)++;
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    APCHeader apchdr;
	char      str[32];
    AFile*	  node;
	DWORD     read;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // CRYO_APC
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying APC file...");

			if (opCheckHNM)
			{
				RFPLUGIN(f)->SetFilePointer(f,pos-0x4C,FILE_BEGIN);
				RFPLUGIN(f)->ReadFile(f,str,4,&read);
				if (!memcmp(str,IDSTR_HNM6,4))
					return NULL;

				RFPLUGIN(f)->SetFilePointer(f,pos-0x2C,FILE_BEGIN);
				RFPLUGIN(f)->ReadFile(f,str,32,&read);
				if (!memcmp(str,IDSTR_COPYRIGHT,32))
					return NULL;
			}

			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			RFPLUGIN(f)->ReadFile(f,&apchdr,sizeof(APCHeader),&read);
			if (
				(read<sizeof(APCHeader)) || 
				(memcmp(apchdr.szID,IDSTR_APC,8)) || 
				((opCheckVersion) && (memcmp(apchdr.szVersion,IDSTR_VER120,4))) ||
				((opCheckRate) && ((apchdr.dwRate>96000) || (apchdr.dwRate<4000))) ||
				((opCheckChannels) && (apchdr.dwStereo>0x1))
			   )
				return NULL;
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"APC file correct.");
			break;
		default: // ???
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=sizeof(APCHeader)+apchdr.dwOutSize/((apchdr.dwStereo)?1:2);
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
	char   version[5];
    DWORD  rate,outsize;
    WORD   channels,bits;
	LONG   sample1,sample2;

    if (f==NULL)
		return NULL;

    if (!ReadAPCHeader(f,&rate,&channels,&bits,version,&outsize,&sample1,&sample2))
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
	char   version[5];
    DWORD  rate,outsize;
    WORD   channels,bits;
	LONG   sample1,sample2;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!ReadAPCHeader(f,&rate,&channels,&bits,version,&outsize,&sample1,&sample2))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	Plugin.fsh->CloseFile(f);

	return MulDiv(1000,outsize,rate);
}

DWORD Decode_IMA_ADPCM(FSHandle* f, char* chunk, DWORD size, short* buff)
{
	DWORD i;
	BYTE  Code;

	if (LPData(f)->channels==2)
	{
		for (i=0;i<size;i++)
		{
			Code=NIBBLE(chunk[i],1);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample1));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample1));
			buff[2*i]=(short)(LPData(f)->Cur_Sample1);
			Code=NIBBLE(chunk[i],0);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index2),&(LPData(f)->Cur_Sample2));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample2));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample2));
			buff[2*i+1]=(short)(LPData(f)->Cur_Sample2);
		}
	}
	else
	{
		for (i=0;i<size;i++)
		{
			Code=NIBBLE(chunk[i],1);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample1));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample1));
			buff[2*i]=(short)(LPData(f)->Cur_Sample1);
			Code=NIBBLE(chunk[i],0);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample1));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample1));
			buff[2*i+1]=(short)(LPData(f)->Cur_Sample1);
		}
	}

	return 4*size;
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
	toRead=min(buffsize/4,BUFFERSIZE);
	while ((!(Plugin.fsh->EndOfFile(file))) && (toRead>0))
	{
		Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
		if (read>0)
			decoded=Decode_IMA_ADPCM(file,LPData(file)->buffer,read,(short*)(buffer+pcmSize));
		else
			break; // ???
		pcmSize+=decoded;
		buffsize-=decoded;
		toRead=min(buffsize/4,BUFFERSIZE);
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
char   ibVersion[5];
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
			wsprintf(str,"Format: "IDSTR_APC"\r\n"
						 "Version: %s\r\n"
						 "Compression: IMA ADPCM\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 ibVersion,
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
			SetCheckBox(hwnd,IDC_CHECKVERSION,opCheckVersion);
			SetCheckBox(hwnd,IDC_CHECKRATE,opCheckRate);
			SetCheckBox(hwnd,IDC_CHECKCHANNELS,opCheckChannels);
			SetCheckBox(hwnd,IDC_CHECKHNM,opCheckHNM);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			SetCheckBox(hwnd,ID_USEENHANCEMENT,opUseEnhancement);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,IDC_CHECKVERSION,FALSE);
					SetCheckBox(hwnd,IDC_CHECKRATE,TRUE);
					SetCheckBox(hwnd,IDC_CHECKCHANNELS,TRUE);
					SetCheckBox(hwnd,IDC_CHECKHNM,TRUE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					SetCheckBox(hwnd,ID_USEENHANCEMENT,FALSE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opCheckVersion=GetCheckBox(hwnd,IDC_CHECKVERSION);
					opCheckRate=GetCheckBox(hwnd,IDC_CHECKRATE);
					opCheckChannels=GetCheckBox(hwnd,IDC_CHECKCHANNELS);
					opCheckHNM=GetCheckBox(hwnd,IDC_CHECKHNM);
					opUseClipping=GetCheckBox(hwnd,ID_USECLIPPING);
					opUseEnhancement=GetCheckBox(hwnd,ID_USEENHANCEMENT);
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
	LONG  sample1,sample2;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadAPCHeader(f,&ibRate,&ibChannels,&ibBits,ibVersion,&outsize,&sample1,&sample2))
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
	{8,IDSTR_APC}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX APC Audio File Decoder",
    "v0.80",
	"Cryo Interactive Audio Files (*.APC)\0*.apc\0",
    "APC",
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
    NULL
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
