/*
 * ACM plug-in source code: AF plug-in API stuff
 * (Interplay music/sfx/speech)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code uses ACM decompression engine written by
 * Alexander Belyakov (abel@krasu.ru)
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

#include "..\..\..\Common\ACMStream\ACMStream.h"

#include "AF_ACM.h"
#include "resource.h"

typedef struct tagData
{
    WORD   channels,bits,align;
	DWORD  rate,acm;
} Data;

#define LPData(f) ((Data*)((f)->afData))

void __cdecl DebugMessageBox(const char* title, const char* fmt, ...)
{
	char    str[512];
	va_list val;

	va_start(val,fmt);
	wvsprintf(str,fmt,val);
	MessageBox(GetFocus(),str,title,MB_OK | MB_ICONEXCLAMATION);
	va_end(val);
}

AFPlugin Plugin;

BOOL opDisableSearch,
	 opChannelsCheck;
int  acmChannels,
	 defChannels,
	 dlgChannels,
	 acmSize;

typedef struct tagIntOption 
{
    char *str;
    int   value;
} IntOption;

IntOption acmChannelsOptions[]=
{
	{"header",  ID_USEHEADER},
	{"ask",     ID_ASKUSER},
	{"default", ID_USEDEFAULT}
};

IntOption defChannelsOptions[]=
{
	{"mono",   ID_MONO},
	{"stereo", ID_STEREO}
};

IntOption acmSizeOptions[]=
{
	{"estimate", ID_SIZE_ESTIMATE},
	{"walk",     ID_SIZE_WALK}
};

int GetIntOption(const char* key, IntOption* pio, int num, int def)
{
    int i;
    char str[256];

    GetPrivateProfileString(Plugin.Description,key,pio[def].str,str,sizeof(str),Plugin.szINIFileName);
    for (i=0;i<num;i++)
		if (!lstrcmp(pio[i].str,str))
			return pio[i].value;

    return pio[def].value;
}

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opDisableSearch=FALSE;
		opChannelsCheck=TRUE;
		acmChannels=ID_USEHEADER;
		dlgChannels=defChannels=ID_MONO;
		acmSize=ID_SIZE_WALK;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"DisableSearch","off",str,40,Plugin.szINIFileName);
    opDisableSearch=LoadOptionBool(str);
	Plugin.numPatterns=(opDisableSearch)?(0L):1;
	GetPrivateProfileString(Plugin.Description,"ChannelsCheck","on",str,40,Plugin.szINIFileName);
	opChannelsCheck=LoadOptionBool(str);
	acmChannels=GetIntOption("ACMChannels",acmChannelsOptions,sizeof(acmChannelsOptions)/sizeof(IntOption),0);
	dlgChannels=defChannels=GetIntOption("DefChannels",defChannelsOptions,sizeof(defChannelsOptions)/sizeof(IntOption),0);
	acmSize=GetIntOption("ACMSize",acmSizeOptions,sizeof(acmSizeOptions)/sizeof(IntOption),1);
}

void WriteIntOption(const char* key, IntOption* pio, int num, int value)
{
    int i;

    for (i=0;i<num;i++)
		if (pio[i].value==value)
		{
			WritePrivateProfileString(Plugin.Description,key,pio[i].str,Plugin.szINIFileName);
			return;
		}
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"DisableSearch",SaveOptionBool(opDisableSearch),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"ChannelsCheck",SaveOptionBool(opChannelsCheck),Plugin.szINIFileName);
	WriteIntOption("ACMChannels",acmChannelsOptions,sizeof(acmChannelsOptions)/sizeof(IntOption),acmChannels);
	WriteIntOption("DefChannels",defChannelsOptions,sizeof(defChannelsOptions)/sizeof(IntOption),defChannels);
	WriteIntOption("ACMSize",acmSizeOptions,sizeof(acmSizeOptions)/sizeof(IntOption),acmSize);
}

const char* GetFileTitleEx(const char* path)
{
    char* name;

    name=strrchr(path,'\\');
    if (name==NULL)
		name=strrchr(path,'/');
	if (name==NULL)
		return path;
    else
		return (name+1);
}

int GetRadioButton(HWND hwnd, int first, int last)
{
    int i;

    for (i=first;i<=last;i++)
		if (IsDlgButtonChecked(hwnd,i)==BST_CHECKED)
			return i;

    return first;
}

BOOL CenterWindow(HWND hwndChild, HWND hwndParent);

char  dlgTitle[MAX_PATH];

BOOL CALLBACK AskACMDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char str[MAX_PATH+100];

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			wsprintf(str,"ACM channels number: %s",(!lstrcmpi(dlgTitle,"\0"))?NO_AFNAME:GetFileTitleEx(dlgTitle));
			SetWindowText(hwnd,str);
			CheckRadioButton(hwnd,ID_USEHEADER,ID_STEREO,dlgChannels);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					dlgChannels=GetRadioButton(hwnd,ID_USEHEADER,ID_STEREO);
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

WORD AskACMChannels(const char* rfName, const char* rfDataString)
{
    switch (acmChannels)
    {
		case ID_USEHEADER:
			return 0;
		case ID_ASKUSER:
			if ((rfDataString!=NULL) && (lstrcmpi(rfDataString,"")))
				lstrcpy(dlgTitle,rfDataString);
			else if ((rfName!=NULL) && (lstrcmpi(rfName,"")))
				lstrcpy(dlgTitle,rfName);
			else
				lstrcpy(dlgTitle,"");
			if (DialogBox(Plugin.hDllInst,"Channels",GetFocus(),AskACMDlgProc))
			{
				switch (dlgChannels)
				{
					case ID_USEHEADER:
						return 0;
					case ID_STEREO:
						return 2;
					case ID_MONO:
						return 1;
					default: // ???
						return 0;
				}
			}
			else
				return 0;
		case ID_USEDEFAULT:
			return (defChannels==ID_STEREO)?2:1;
		default: // ???
			return 0;
    }
}

WORD GetACMChannels(FSHandle* file, const char* rfName, const char* rfDataString)
{
	char *endptr;
	WORD  channels;

	if (file==NULL)
		return 0;

	channels=0;
	if (file->node!=NULL)
		channels=(WORD)strtoul(file->node->afDataString,&endptr,0x10);
    if (channels==0)
		channels=AskACMChannels(rfName,rfDataString);

	return channels;
}

BOOL GetInfo
(
	FSHandle* file, 
	const char* rfName, 
	const char* rfDataString, 
	DWORD *rate, 
	WORD  *channels, 
	WORD  *bits, 
	DWORD *size
)
{
	WORD chn;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
	if (!ACMStreamGetInfo((DWORD)(Plugin.fsh->ReadFile),(DWORD)file,rate,channels,bits,size))
		return FALSE;
	if (opChannelsCheck)
	{
		if ((*channels<1) || (*channels>2))
			return FALSE;
	}
	if ((chn=GetACMChannels(file,rfName,rfDataString))!=0)
		*channels=chn;

    return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	DWORD size;
	char *rfName,*rfDataString;

    if (f==NULL)
		return FALSE;

	if (f->node!=NULL)
	{
		rfName=f->node->rfName;
		rfDataString=f->node->rfDataString;
	}
	else
		rfDataString=rfName=NULL;
    if (!GetInfo(f,rfName,rfDataString,rate,channels,bits,&size))
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
	Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
	LPData(f)->acm=ACMStreamInit((DWORD)(Plugin.fsh->ReadFile),(DWORD)f);
	if (LPData(f)->acm==0L)
	{
		GlobalFree(f->afData);
		SetLastError(PRIVEC(IDS_INITFAILED));
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

	ACMStreamShutdown(LPData(f)->acm);

    return (GlobalFree(f->afData)==NULL);
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate,size;
	WORD   channels,bits;

    if (f==NULL)
		return NULL;

    if (!GetInfo(f,rfName,rfDataString,&rate,&channels,&bits,&size))
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    wsprintf(node->afDataString,"%X",channels);
	lstrcpy(node->afName,"");

    return node;
}

DWORD __stdcall GetTime(AFile* node)
{
    FSHandle *f;
    DWORD     size,rate;
	WORD	  channels,bits;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!GetInfo(f,node->rfName,node->rfDataString,&rate,&channels,&bits,&size))
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
    Plugin.fsh->CloseFile(f);

    return MulDiv(8000ul,size,channels*bits*rate);
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;

	*buffpos=-1;

	return ACMStreamReadAndDecompress(LPData(file)->acm,buffer,CORRALIGN(buffsize,LPData(file)->align));
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

void SetCheckBox(HWND hwnd, int cbId, BOOL cbVal)
{
    CheckDlgButton(hwnd,cbId,(cbVal)?BST_CHECKED:BST_UNCHECKED);
}

BOOL GetCheckBox(HWND hwnd, int cbId)
{
    return (BOOL)(IsDlgButtonChecked(hwnd,cbId)==BST_CHECKED);
}

void GrayButtons(HWND hwnd, int channels)
{
	EnableWindow(GetDlgItem(hwnd,ID_MONO),(channels==ID_USEDEFAULT));
	EnableWindow(GetDlgItem(hwnd,ID_STEREO),(channels==ID_USEDEFAULT));
}

void SetDefaults(HWND hwnd)
{
	CheckRadioButton(hwnd,ID_USEHEADER,ID_USEDEFAULT,ID_USEHEADER);
	CheckRadioButton(hwnd,ID_MONO,ID_STEREO,ID_MONO);
	CheckRadioButton(hwnd,ID_SIZE_ESTIMATE,ID_SIZE_WALK,ID_SIZE_WALK);
	SetCheckBox(hwnd,ID_NOSEARCH,FALSE);
	SetCheckBox(hwnd,ID_CHANNELSCHECK,TRUE);
	GrayButtons(hwnd,ID_USEHEADER);
}

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetCheckBox(hwnd,ID_NOSEARCH,opDisableSearch);
			SetCheckBox(hwnd,ID_CHANNELSCHECK,opChannelsCheck);
			CheckRadioButton(hwnd,ID_USEHEADER,ID_USEDEFAULT,acmChannels);
			CheckRadioButton(hwnd,ID_MONO,ID_STEREO,defChannels);
			CheckRadioButton(hwnd,ID_SIZE_ESTIMATE,ID_SIZE_WALK,acmSize);
			GrayButtons(hwnd,acmChannels);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetDefaults(hwnd);
					break;
				case ID_USEHEADER:
				case ID_ASKUSER:
				case ID_USEDEFAULT:
					GrayButtons(hwnd,LOWORD(wParam));
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opDisableSearch=GetCheckBox(hwnd,ID_NOSEARCH);
					Plugin.numPatterns=(opDisableSearch)?(0L):1;
					opChannelsCheck=GetCheckBox(hwnd,ID_CHANNELSCHECK);
					acmChannels=GetRadioButton(hwnd,ID_USEHEADER,ID_USEDEFAULT);
					dlgChannels=defChannels=GetRadioButton(hwnd,ID_MONO,ID_STEREO);
					acmSize=GetRadioButton(hwnd,ID_SIZE_ESTIMATE,ID_SIZE_WALK);
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

LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;

    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			hres=FindResource(Plugin.hDllInst,(LPCTSTR)IDR_CREDITS,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(Plugin.hDllInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_COPYRIGHT,(res!=NULL)?res:"No info available.");
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

DWORD  ibSize,ibRate,ibTime;
WORD   ibChannels,ibBits;
AFile *ibNode;

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
			wsprintf(str,"Compression: Unknown\r\n" // ???
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
	FSHandle *f;
    char      str[256];
	DWORD     size;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!GetInfo(f,node->rfName,node->rfDataString,&ibRate,&ibChannels,&ibBits,&size))
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

#define BUFFERSIZE (64000ul)

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    AFile      *node;

	FSHandle    fs;
	DWORD	    rate,buffpos,size;
	WORD	    bits,channels;
	char	   *buff;

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying ACM file...");
	RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
	fs.rf=f;
	fs.start=pos;
	fs.length=RFPLUGIN(f)->GetFileSize(f)-pos;
	fs.node=NULL;
	if (!GetInfo(&fs,NO_AFNAME,"",&rate,&channels,&bits,&size))
		return NULL;
	if (opChannelsCheck)
	{
		if ((channels<1) || (channels>2))
			return NULL;
	}
	switch (acmSize)
	{
		case ID_SIZE_ESTIMATE:
			node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
			node->fsStart=pos;
			node->fsLength=size/4+0xE;
			break;
		case ID_SIZE_WALK:
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Walking ACM file...");
			if (!InitPlayback(&fs,&rate,&channels,&bits))
				return NULL;
			buff=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
			while (FillPCMBuffer(&fs,buff,BUFFERSIZE,&buffpos)!=0) 
			{
				if ((BOOL)SendMessage(hwnd,WM_GAP_ISCANCELLED,0,0))
				{
					GlobalFree(buff);
					ShutdownPlayback(&fs);
					return NULL;
				}
				SendMessage(hwnd,WM_GAP_SHOWFILEPROGRESS,0,(LPARAM)f);
			}
			GlobalFree(buff);
			node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
			node->fsStart=pos;
			node->fsLength=Plugin.fsh->GetFilePointer(&fs);
			ShutdownPlayback(&fs);
			break;
		default: // ???
			return NULL;
	}
	*newpos=node->fsStart+node->fsLength;
	wsprintf(node->afDataString,"%X",channels);
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"ACM file correct.");

    return node;
}

SearchPattern patterns[]=
{
	{4,IDSTR_ACM}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX ACM Audio File Decoder",
    "v0.98",
    "Interplay ACM Audio Files (*.ACM)\0*.acm\0",
    "ACM",
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
