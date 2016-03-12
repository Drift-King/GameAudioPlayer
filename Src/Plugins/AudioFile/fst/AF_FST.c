/*
 * FST plug-in source code
 * (FutureVision video soundtracks: Harvester, etc.)
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

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_FST.h"
#include "resource.h"

typedef struct tagData
{
	WORD  channels,bits,align;
	DWORD rate,nframes,iframe,skip;
	FSTFrameEntry *table;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

BOOL opCheckWidth,
	 opCheckHeight,
	 opCheckUnknown1,
	 opCheckFrameRate,
	 opCheckRate,
	 opCheckBits, 
	 opCheckUnknown2;

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opCheckWidth=TRUE;
		opCheckHeight=TRUE;
		opCheckUnknown1=FALSE;
		opCheckFrameRate=FALSE;
		opCheckRate=TRUE;
		opCheckBits=TRUE;
		opCheckUnknown2=FALSE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"CheckWidth","on",str,40,Plugin.szINIFileName);
    opCheckWidth=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckHeight","on",str,40,Plugin.szINIFileName);
    opCheckHeight=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckUnknown1","off",str,40,Plugin.szINIFileName);
    opCheckUnknown1=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckFrameRate","off",str,40,Plugin.szINIFileName);
    opCheckFrameRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckRate","on",str,40,Plugin.szINIFileName);
    opCheckRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckBits","on",str,40,Plugin.szINIFileName);
    opCheckBits=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckUnknown2","off",str,40,Plugin.szINIFileName);
    opCheckUnknown2=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;
	
	WritePrivateProfileString(Plugin.Description,"CheckWidth",SaveOptionBool(opCheckWidth),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckHeight",SaveOptionBool(opCheckHeight),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckUnknown1",SaveOptionBool(opCheckUnknown1),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckFrameRate",SaveOptionBool(opCheckFrameRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckRate",SaveOptionBool(opCheckRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckBits",SaveOptionBool(opCheckBits),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckUnknown2",SaveOptionBool(opCheckUnknown2),Plugin.szINIFileName);
}

BOOL ReadFSTHeader
(
	FSHandle    *file, 
	DWORD       *rate, 
	WORD        *channels, 
	WORD        *bits, 
	DWORD       *nframes,
	FSTFrameEntry* *table
)
{
    FSTHeader fsthdr;
    DWORD     read;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&fsthdr,sizeof(FSTHeader),&read);
	if (
		(read<sizeof(FSTHeader)) || 
		(memcmp(fsthdr.szID,IDSTR_2TSF,4)) || 
		((opCheckWidth) && (fsthdr.dwWidth>2048l)) ||
		((opCheckHeight) && (fsthdr.dwHeight>2048l)) ||
		((opCheckUnknown1) && (fsthdr.dwUnknown1!=0x00043800)) ||
		((opCheckFrameRate) && ((fsthdr.dwFrameRate>100) || (fsthdr.dwFrameRate<5))) ||
		((opCheckRate) && ((fsthdr.dwRate>96000) || (fsthdr.dwRate<4000))) ||
		((opCheckBits) && ((fsthdr.wBits<8) || (fsthdr.wBits>16))) ||
		((opCheckUnknown2) && (fsthdr.wUnknown2!=0x0000))
	   )
		return FALSE; // ???

	*nframes=fsthdr.nFrames;
	*rate=fsthdr.dwRate;
	*bits=fsthdr.wBits;
	*channels=1; // ???

	if (table!=NULL)
	{
		*table=GlobalAlloc(GPTR,sizeof(FSTFrameEntry)*fsthdr.nFrames);
		if (*table==NULL)
			return TRUE;
		Plugin.fsh->ReadFile(file,*table,sizeof(FSTFrameEntry)*fsthdr.nFrames,&read);
		if (read<sizeof(FSTFrameEntry)*fsthdr.nFrames)
		{
			GlobalFree(*table);
			*table=NULL;
		}
	}

	return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	DWORD nframes;
	FSTFrameEntry *table=NULL;

    if (f==NULL)
		return FALSE;

    if (!ReadFSTHeader(f,rate,channels,bits,&nframes,&table))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	if (table==NULL)
	{
		SetLastError(PRIVEC(IDS_NOTABLE));
		return FALSE;
	}
	f->afData=GlobalAlloc(GPTR,sizeof(Data));
	if (f->afData==NULL)
	{
		GlobalFree(table);
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
	}
	LPData(f)->align=(*channels)*((*bits)/8);
	LPData(f)->rate=*rate;
	LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->iframe=0l;
	LPData(f)->skip=(table[1].wSoundSize!=0)?(table[0].wSoundSize/table[1].wSoundSize-1):0l;
	LPData(f)->nframes=nframes;
	LPData(f)->table=table;

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

    return (
			(GlobalFree(LPData(f)->table)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    FSTHeader  fsthdr;
	FSTFrameEntry frmentry;
    AFile*	   node;
	DWORD      i,read,size;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // 2TSF
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying FST file...");
			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			RFPLUGIN(f)->ReadFile(f,&fsthdr,sizeof(FSTHeader),&read);
			if (
				(read<sizeof(FSTHeader)) || 
				(memcmp(fsthdr.szID,IDSTR_2TSF,4)) || 
				((opCheckWidth) && (fsthdr.dwWidth>2048l)) ||
				((opCheckHeight) && (fsthdr.dwHeight>2048l)) ||
				((opCheckUnknown1) && (fsthdr.dwUnknown1!=0x00043800)) ||
				((opCheckFrameRate) && ((fsthdr.dwFrameRate>100) || (fsthdr.dwFrameRate<5))) ||
				((opCheckRate) && ((fsthdr.dwRate>96000) || (fsthdr.dwRate<4000))) ||
				((opCheckBits) && ((fsthdr.wBits<8) || (fsthdr.wBits>16))) ||
				((opCheckUnknown2) && (fsthdr.wUnknown2!=0x0000))
			)
				return NULL;

			size=sizeof(FSTHeader)+sizeof(FSTFrameEntry)*(fsthdr.nFrames);
			for (i=0;i<fsthdr.nFrames;i++)
			{
				RFPLUGIN(f)->ReadFile(f,&frmentry,sizeof(FSTFrameEntry),&read);
				size+=frmentry.dwImageSize+frmentry.wSoundSize;
			}

			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"FST file correct.");
			break;
		default: // ???
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=size;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate,nframes;
    WORD   channels,bits;

    if (f==NULL)
		return NULL;

    if (!ReadFSTHeader(f,&rate,&channels,&bits,&nframes,NULL))
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
    FSHandle   *f;
	FSTFrameEntry *table=NULL;
    DWORD       i,rate,nframes,skip,soundsize;
    WORD        channels,bits;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!ReadFSTHeader(f,&rate,&channels,&bits,&nframes,&table))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	Plugin.fsh->CloseFile(f);

	if (table==NULL)
		return -1;

	soundsize=0;
	skip=(table[1].wSoundSize!=0)?(table[0].wSoundSize/table[1].wSoundSize-1):0l;
	for (i=0;i<nframes-skip;i++)
		soundsize+=table[i].wSoundSize;
	
	GlobalFree(table);

	return MulDiv(8000,soundsize,rate*channels*bits);
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read,pcmSize;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	pcmSize=0;
	while (
			(!(Plugin.fsh->EndOfFile(file))) &&
			(LPData(file)->iframe<(LPData(file)->nframes)-(LPData(file)->skip)) &&
			(buffsize>LPData(file)->table[LPData(file)->iframe].wSoundSize)
		  )
	{
		Plugin.fsh->SetFilePointer(file,LPData(file)->table[LPData(file)->iframe].dwImageSize,FILE_CURRENT);
		Plugin.fsh->ReadFile(file,buffer+pcmSize,LPData(file)->table[LPData(file)->iframe].wSoundSize,&read);
		LPData(file)->iframe++;
		if (read==0)
			break; // ???
		pcmSize+=read;
		buffsize-=read;
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
			wsprintf(str,"Format: "IDSTR_2TSF"\r\n"
						 "Compression: None (PCM)\r\n"
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
			SetCheckBox(hwnd,IDC_CHECKWIDTH,opCheckWidth);
			SetCheckBox(hwnd,IDC_CHECKHEIGHT,opCheckHeight);
			SetCheckBox(hwnd,IDC_CHECKUNKNOWN1,opCheckUnknown1);
			SetCheckBox(hwnd,IDC_CHECKFRAMERATE,opCheckFrameRate);
			SetCheckBox(hwnd,IDC_CHECKRATE,opCheckRate);
			SetCheckBox(hwnd,IDC_CHECKBITS,opCheckBits);
			SetCheckBox(hwnd,IDC_CHECKUNKNOWN2,opCheckUnknown2);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,IDC_CHECKWIDTH,TRUE);
					SetCheckBox(hwnd,IDC_CHECKHEIGHT,TRUE);
					SetCheckBox(hwnd,IDC_CHECKUNKNOWN1,FALSE);
					SetCheckBox(hwnd,IDC_CHECKFRAMERATE,FALSE);
					SetCheckBox(hwnd,IDC_CHECKRATE,TRUE);
					SetCheckBox(hwnd,IDC_CHECKBITS,TRUE);
					SetCheckBox(hwnd,IDC_CHECKUNKNOWN2,FALSE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opCheckWidth=GetCheckBox(hwnd,IDC_CHECKWIDTH);
					opCheckHeight=GetCheckBox(hwnd,IDC_CHECKHEIGHT);
					opCheckUnknown1=GetCheckBox(hwnd,IDC_CHECKUNKNOWN1);
					opCheckFrameRate=GetCheckBox(hwnd,IDC_CHECKFRAMERATE);
					opCheckRate=GetCheckBox(hwnd,IDC_CHECKRATE);
					opCheckBits=GetCheckBox(hwnd,IDC_CHECKBITS);
					opCheckUnknown2=GetCheckBox(hwnd,IDC_CHECKUNKNOWN2);
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
	DWORD nframes;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadFSTHeader(f,&ibRate,&ibChannels,&ibBits,&nframes,NULL))
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
    DWORD filepos,seekpos;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

    filepos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
	filepos=CORRALIGN(filepos,LPData(f)->align);
	seekpos=sizeof(FSTHeader)+sizeof(FSTFrameEntry)*(LPData(f)->nframes);
	LPData(f)->iframe=0l;
	while (
			(LPData(f)->iframe<(LPData(f)->nframes)-(LPData(f)->skip)) &&
			(LPData(f)->table[LPData(f)->iframe].wSoundSize<=filepos)
		  )
	{
		seekpos+=LPData(f)->table[LPData(f)->iframe].dwImageSize+LPData(f)->table[LPData(f)->iframe].wSoundSize;
		filepos-=LPData(f)->table[LPData(f)->iframe].wSoundSize;
		LPData(f)->iframe++;
	}
    Plugin.fsh->SetFilePointer(f,seekpos,FILE_BEGIN);
}

SearchPattern patterns[]=
{
	{4,IDSTR_2TSF}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX FST Audio File Plug-In",
    "v0.80",
	"FutureVision Video Files (*.FST)\0*.fst\0",
    "FST",
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
