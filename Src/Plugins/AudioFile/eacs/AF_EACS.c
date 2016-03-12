/*
 * EACS plug-in source code
 * (Electronic Arts Canada music, sfx, speech, video soundtracks)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * EA ADPCM decompression code is based on WVE2PCM converter written by
 * Dmitry Kirnocenskij: ejt@mail.ru
 * 
 * Info on new SCDl blocks is provided by
 * Toni Wilen: nhlinfo@nhl-online.com
 * http://www.nhl-online.com/nhlinfo/
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
#include <stdio.h>

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_EACS.h"
#include "resource.h"

#define WM_GAP_WAITINGTOY (WM_USER+1)

#define BUFFERSIZE (64000ul)

#define ASF_BT_HEADER  (0)
#define ASF_BT_COUNT   (1)
#define ASF_BT_DATA    (2)
#define ASF_BT_LOOP    (3)
#define ASF_BT_END     (4)
#define ASF_BT_UNKNOWN (0xFF)

typedef struct tagASFBlockNode
{
	BYTE   type;
	DWORD  start,size,outsize;
	struct tagASFBlockNode *next;
} ASFBlockNode;

typedef struct tagMUSSectionNode
{
	BYTE   index;
	DWORD  start,outsize;
	BOOL   end;
	struct tagMUSSectionNode *next;
} MUSSectionNode;

typedef struct tagDecState_EA_ADPCM
{
	long lPrevSample,lCurSample,lPrevCoeff,lCurCoeff;
	BYTE bShift;
} DecState_EA_ADPCM;

typedef struct tagData
{
	WORD  channels,bits,align;
	BYTE  compression,split;
	DWORD rate,curTime,samplesToSkip;
    char *buffer;
	DecState_EA_ADPCM dsLeft,dsRight;

	MUSSectionNode *musChain,*curSection;
	DWORD  numSCDl;

	ASFBlockNode *chain,*curnode;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

BOOL opIgnoreLoops,
	 opWalkChain,
	 opNoMUSTime,
	 opUseClipping;
int  musMAP;

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

typedef struct tagIntOption 
{
    char *str;
    int   value;
} IntOption;

IntOption musMAPOptions[]=
{
    {"default", ID_MAP_DEFAULT},
    {"ask",	ID_MAP_ASK}
};

int GetIntOption(const char* key, IntOption* pio, int num, int def)
{
    int  i;
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
		opIgnoreLoops=TRUE;
		opWalkChain=FALSE;
		opNoMUSTime=FALSE;
		opUseClipping=TRUE;
		musMAP=ID_MAP_DEFAULT;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"IgnoreLoops","on",str,40,Plugin.szINIFileName);
    opIgnoreLoops=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"WalkChain","off",str,40,Plugin.szINIFileName);
    opWalkChain=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"NoMUSTime","off",str,40,Plugin.szINIFileName);
    opNoMUSTime=LoadOptionBool(str);
	musMAP=GetIntOption("MAPForMUS",musMAPOptions,sizeof(musMAPOptions)/sizeof(IntOption),0);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
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

	WritePrivateProfileString(Plugin.Description,"IgnoreLoops",SaveOptionBool(opIgnoreLoops),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"WalkChain",SaveOptionBool(opWalkChain),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"NoMUSTime",SaveOptionBool(opNoMUSTime),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
	WriteIntOption("MAPForMUS",musMAPOptions,sizeof(musMAPOptions)/sizeof(IntOption),musMAP);
}

void AlignFilePointer4(FSHandle* f)
{
	DWORD pos=Plugin.fsh->GetFilePointer(f);

	if (pos%4)
		Plugin.fsh->SetFilePointer(f,4-pos%4,FILE_CURRENT);
}

DWORD ReadBytes(FSHandle* file, BYTE count)
{
	BYTE  i,byte;
	DWORD result,read;

	result=0L;
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
   DWORD* datastart,
   BYTE* split
)
{
	char  id[4];
	DWORD read;
	BYTE  byte;
	BOOL  flag,subflag;

	*rate=22050; // ???
	*channels=2; // ???
	*bits=16; // ???
	*numsamples=0;
	*datastart=-1;
	*compression=SC_NONE;
	*split=0;
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
						case 0x80: // ???
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*compression=SC_EA_ADPCM; // ???
							*split=(BYTE)ReadBytes(file,byte); // ???
							break;
						case 0xA0: // ???
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							if ((BYTE)ReadBytes(file,byte)==0x8)
								*compression=SC_NONE; // ???
							break;
						case 0x85:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*numsamples=ReadBytes(file,byte);
							break;
						case 0x82:
							Plugin.fsh->ReadFile(file,&byte,sizeof(BYTE),&read);
							*channels=(WORD)ReadBytes(file,byte); // ???
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
						case 0xFF: // ???
							subflag=FALSE;
							flag=FALSE;
							break;
						case 0x8A:
							subflag=FALSE;
						case 0x86: // ???
						case 0x87: // ???
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
	DWORD* datastart,
	BYTE* split
)
{
    DWORD read;
	char  id[4];

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
	lstrcpyn(id,"\0\0\0",4);
	Plugin.fsh->ReadFile(file,id,4,&read);
	if (!memcmp(id,IDSTR_SCHl,4))
	{
		Plugin.fsh->SetFilePointer(file,sizeof(ASFBlockHeader),FILE_BEGIN);
		if (!ParsePTHeader(file,rate,channels,bits,compression,numsamples,datastart,split))
			return FALSE;
	}
	else
		return FALSE;

    return TRUE;
}

void FreeASFBlockChain(ASFBlockNode *chain)
{
	ASFBlockNode *walker=chain,*tmp;

	while (walker!=NULL)
	{
		tmp=walker->next;
		GlobalFree(walker);
		walker=tmp;
	}
}

void ClearMessageQueue(HWND hwnd, DWORD timeout)
{
	MSG   msg;
	DWORD end;

	end=GetTickCount()+timeout;
	while (
			(PeekMessage(&msg,hwnd,0,0,PM_REMOVE)) &&
			(end>GetTickCount())
		  )
	{}
}

BOOL CenterWindow(HWND hwndChild, HWND hwndParent);

LRESULT CALLBACK WalkingDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			return TRUE;
		case WM_GAP_WAITINGTOY:
			ClearMessageQueue(hwnd,10); // ???
			break;
		default:
			break;
    }
    return FALSE;
}

LRESULT CALLBACK WaitingDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static DWORD time;
	static int min,max,x,y,shift,xsize,ysize;
	DWORD timeout=50;
	RECT rect;

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			GetWindowRect(GetDlgItem(hwnd,ID_TOY),&rect);
			MapWindowPoints(NULL,hwnd,(LPPOINT)&rect,2);
			xsize=rect.right-rect.left;
			ysize=rect.bottom-rect.top;
			x=rect.left;
			y=rect.top;
			GetWindowRect(GetDlgItem(hwnd,ID_WAITMSG),&rect);
			MapWindowPoints(NULL,hwnd,(LPPOINT)&rect,2);
			min=rect.left;
			max=rect.right-xsize;
			shift=5;
			time=GetTickCount();
			return TRUE;
		case WM_GAP_WAITINGTOY:
			ClearMessageQueue(hwnd,10); // ???
			if (GetTickCount()>time+timeout)
			{
				MoveWindow(GetDlgItem(hwnd,ID_TOY),x+=shift,y,xsize,ysize,TRUE);
				if ((x<=min) || (x>=max))
					shift=-shift;
				UpdateWindow(hwnd);
				UpdateWindow(GetParent(hwnd));
				time=GetTickCount();
			}
			break;
		default:
			break;
    }
    return FALSE;
}

ASFBlockNode* ReadASFBlockChain(FSHandle* f)
{
    DWORD read;
	HWND  pwnd,hwnd;
	ASFBlockNode  *chain,**pcurnode;
	ASFBlockHeader asfblockhdr;

	chain=NULL;
	pcurnode=&chain;
	hwnd=GetFocus();
	pwnd=CreateDialog(Plugin.hDllInst,"Walking",Plugin.hMainWindow,(DLGPROC)WalkingDlgProc);
	SetWindowText(pwnd,"Playback startup");
	SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETRANGE,0,MAKELPARAM(0,100));
    SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,0,0L);
	Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		Plugin.fsh->ReadFile(f,&asfblockhdr,sizeof(ASFBlockHeader),&read);
		if (read<sizeof(ASFBlockHeader))
			break;
		(*pcurnode)=(ASFBlockNode*)GlobalAlloc(GPTR,sizeof(ASFBlockNode));
		(*pcurnode)->next=NULL;
		(*pcurnode)->start=Plugin.fsh->GetFilePointer(f);
		(*pcurnode)->size=asfblockhdr.size-sizeof(ASFBlockHeader);
		(*pcurnode)->outsize=0;
		if (!memcmp(asfblockhdr.id,IDSTR_SCHl,4))
			(*pcurnode)->type=ASF_BT_HEADER;
		else if (!memcmp(asfblockhdr.id,IDSTR_SCCl,4))
			(*pcurnode)->type=ASF_BT_COUNT;
		else if (!memcmp(asfblockhdr.id,IDSTR_SCDl,4))
		{
			Plugin.fsh->ReadFile(f,&((*pcurnode)->outsize),sizeof(DWORD),&read);
			Plugin.fsh->SetFilePointer(f,(*pcurnode)->start,FILE_BEGIN);
			(*pcurnode)->type=ASF_BT_DATA;
		}
		else if (!memcmp(asfblockhdr.id,IDSTR_SCLl,4))
			(*pcurnode)->type=ASF_BT_LOOP;
		else if (!memcmp(asfblockhdr.id,IDSTR_SCEl,4))
			(*pcurnode)->type=ASF_BT_END;
		else
			(*pcurnode)->type=ASF_BT_UNKNOWN;
		pcurnode=&((*pcurnode)->next);
		Plugin.fsh->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
		if (!memcmp(asfblockhdr.id,IDSTR_SCEl,4)) // ???
			AlignFilePointer4(f);
		SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,MulDiv(100,Plugin.fsh->GetFilePointer(f),Plugin.fsh->GetFileSize(f)),0L);
		SendMessage(pwnd,WM_GAP_WAITINGTOY,0,0);
    }
	SetFocus(hwnd);
    DestroyWindow(pwnd);
	UpdateWindow(hwnd);

	return chain;
}

char* GetFileExtension(const char* filename)
{
	char *ch;

	ch=strrchr(filename,'.');
	if ((ch>strrchr(filename,'\\')) && (ch>strrchr(filename,'/')))
		return ch;
	else
		return NULL;
}

void CutFileExtension(char* filename)
{
	char *ch;

	ch=GetFileExtension(filename);
	if (ch!=NULL)
		*ch=0;
}

BOOL CheckFileExtension(FSHandle *f, const char* ext)
{
	if (f==NULL)
		return FALSE;
	if (f->node==NULL)
		return FALSE;

	return (!lstrcmpi(GetFileExtension(f->node->rfName),ext));
}

void FreeMUSSectionChain(MUSSectionNode *chain)
{
	MUSSectionNode *walker=chain,*tmp;

	while (walker!=NULL)
	{
		if (walker->end)
			tmp=NULL;
		else
			tmp=walker->next;
		GlobalFree(walker);
		walker=tmp;
	}
}

MUSSectionNode* ReadMUSSectionChain(HWND hwnd, FSHandle* f, HANDLE mapfile)
{
	BYTE  curSection,newSection,compression,split;
	WORD  channels,bits;
	DWORD start,read,rate,outsize,datastart;
	MAPHeader       maphdr;
	MUSSectionNode *chain,*walker,**pcurnode;
	MAPSectionDef   mapsecdef;

	SetFilePointer(mapfile,0,NULL,FILE_BEGIN);
	chain=NULL;
	pcurnode=&chain;
	ReadFile(mapfile,&maphdr,sizeof(MAPHeader),&read,NULL);
	if (read<sizeof(MAPHeader))
		return NULL;
	if (memcmp(maphdr.id,IDSTR_PFDx,4))
		return NULL;
	curSection=maphdr.firstSection;
	while (1)
	{
		SetFilePointer(mapfile,sizeof(MAPHeader)+curSection*sizeof(MAPSectionDef),NULL,FILE_BEGIN);
		read=0L;
		ReadFile(mapfile,&mapsecdef,sizeof(MAPSectionDef),&read,NULL);
		if ((read<sizeof(MAPSectionDef)) || (mapsecdef.numRecords>8))
		{
			FreeMUSSectionChain(chain);
			return NULL;
		}
		if (mapsecdef.numRecords>0)
			newSection=mapsecdef.records[mapsecdef.numRecords-1].section;
		else
			newSection=0xFF;
		if (newSection>=maphdr.numSections) // ???
			newSection=0xFF;
		SetFilePointer(mapfile,sizeof(MAPHeader)+maphdr.numSections*sizeof(MAPSectionDef)+(maphdr.numRecords)*(maphdr.recordSize)+curSection*sizeof(DWORD),NULL,FILE_BEGIN);
		ReadFile(mapfile,&start,sizeof(DWORD),&read,NULL);
		start=SWAPDWORD(start);
		(*pcurnode)=(MUSSectionNode*)GlobalAlloc(GPTR,sizeof(MUSSectionNode));
		(*pcurnode)->next=NULL;
		(*pcurnode)->end=FALSE;
		(*pcurnode)->start=start;
		(*pcurnode)->index=curSection;
		(*pcurnode)->outsize=0;
		if (f!=NULL)
		{
			Plugin.fsh->SetFilePointer(f,start+sizeof(ASFBlockHeader),FILE_BEGIN);
			if (!ParsePTHeader(f,&rate,&channels,&bits,&compression,&outsize,&datastart,&split))
			{
				FreeMUSSectionChain(chain);
				return NULL;
			}
			(*pcurnode)->outsize=outsize;
		}
		if (newSection!=0xFF)
		{
			walker=chain;
			while (walker!=(*pcurnode))
			{
				if (walker->index==newSection)
				{
					(*pcurnode)->end=TRUE;
					(*pcurnode)->next=walker;
					break;
				}
				walker=walker->next;
			}
		}
		else
		{
			(*pcurnode)->end=TRUE;
			(*pcurnode)->next=NULL;
		}
		if ((*pcurnode)->end)
			break;
		pcurnode=&((*pcurnode)->next);
		curSection=newSection;
		SendMessage(hwnd,WM_GAP_WAITINGTOY,0,0);
	}

	return chain;
}

char* GetFileTitleEx(char* path)
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

void GetDefMAPFileName(char* name, DWORD size, const char* mus)
{
    lstrcpyn(name,mus,size);
    CutFileExtension(name);
    lstrcat(name,".lin");
}

void ReplaceExtension(char* name, const char* ext)
{
	CutFileExtension(name);
    lstrcat(name,ext);
}

BOOL GetMAPFileName(HWND hwnd, char* mapname, DWORD size, char* musname)
{
	BOOL retval;
    OPENFILENAME ofn={0};
    char szDirName[MAX_PATH];
    char szTitle[MAX_PATH+100];
	char szExt[]="LIN and MAP Files (*.LIN;*.MAP)\0*.lin;*.map\0LIN Files (*.LIN)\0*.lin\0MAP Files (*.MAP)\0*.map\0All Files (*.*)\0*.*\0";

	wsprintf(szTitle,"Specify .LIN/.MAP file for %s",GetFileTitleEx(musname));
	GetDefMAPFileName(szDirName,sizeof(szDirName),musname);
	lstrcpy(mapname,GetFileTitleEx(szDirName));
	*(GetFileTitleEx(szDirName))=0;
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = szExt;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = mapname;
    ofn.lpstrTitle= szTitle;
    ofn.lpstrDefExt=NULL;
    ofn.nMaxFile = size;
    ofn.lpstrFileTitle = NULL;
    ofn.lpstrInitialDir = szDirName;
    ofn.Flags = OFN_NODEREFERENCELINKS |
				OFN_SHAREAWARE |
				OFN_PATHMUSTEXIST |
				OFN_FILEMUSTEXIST |
				OFN_EXPLORER |
				OFN_READONLY |
				OFN_NOCHANGEDIR;

	retval=GetOpenFileName(&ofn);

	return retval;
}

HANDLE OpenMAPFile(FSHandle *f)
{
	char   mapname[MAX_PATH];
	HANDLE mapfile=INVALID_HANDLE_VALUE;

	lstrcpy(mapname,"");
	if (f->node!=NULL)
		lstrcpy(mapname,f->node->afDataString);
	if (!lstrcmp(mapname,"<mus>"))
	{
		if (musMAP==ID_MAP_DEFAULT)
		{
			if (f->node==NULL)
				return mapfile;
			lstrcpy(mapname,f->node->rfName);
			CutFileExtension(mapname);
			lstrcat(mapname,".lin");
		}
		else
		{
			if (!GetMAPFileName(GetFocus(),mapname,sizeof(mapname),f->node->rfName))
				return mapfile;
			lstrcpy(f->node->afDataString,mapname); // ???
		}
	}
	mapfile=CreateFile(mapname,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_READONLY,NULL);
	if (mapfile==INVALID_HANDLE_VALUE)
	{
		ReplaceExtension(mapname,".map");
		mapfile=CreateFile(mapname,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_READONLY,NULL);
	}

	return mapfile;
}

BOOL IsSection(FSHandle *f)
{
	if (f==NULL)
		return FALSE;
	if (f->node==NULL)
		return FALSE;

	return (!lstrcmpi(f->node->afDataString,""));
}

void Init_EA_ADPCM(DecState_EA_ADPCM *pDS, long curSample, long prevSample)
{
	pDS->lCurSample=curSample;
	pDS->lPrevSample=prevSample;
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

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	BYTE   compression,split;
	DWORD  nsamp,datastart;
	HANDLE mapfile;
	HWND   pwnd,hwnd;
	ASFBlockNode *chain;

    if (f==NULL)
		return FALSE;

    if (!ReadHeader(f,rate,channels,bits,&compression,&nsamp,&datastart,&split))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	switch (compression)
	{
		case SC_EA_ADPCM:
		case SC_NONE:
			break;
		default:
			SetLastError(PRIVEC(IDS_NODECODER));
			return FALSE;
	}
	if (
		(opWalkChain) &&
		(
		 (!CheckFileExtension(f,".mus")) ||
		 (IsSection(f))
		)
	   )
	{
		chain=ReadASFBlockChain(f);
		if (chain==NULL)
		{
			SetLastError(PRIVEC(IDS_INCORRECTFILE));
			return FALSE;
		}
	}
	else
		chain=NULL;
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		FreeASFBlockChain(chain);
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
	LPData(f)->chain=chain;
	LPData(f)->curnode=chain;
	if ((compression!=SC_NONE) || (split))
	{
		LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
		if (LPData(f)->buffer==NULL)
		{
			GlobalFree(f->afData);
			FreeASFBlockChain(chain);
			SetLastError(PRIVEC(IDS_NOBUFFER));
			return FALSE;
		}
	}
	else
		LPData(f)->buffer=NULL;
	LPData(f)->align=(*channels)*(*bits/8);
    LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->rate=*rate;
	LPData(f)->compression=compression;
	LPData(f)->split=split;
	Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
	Init_EA_ADPCM(&(LPData(f)->dsLeft),0L,0L);
	Init_EA_ADPCM(&(LPData(f)->dsRight),0L,0L);
	LPData(f)->curTime=0;
	LPData(f)->samplesToSkip=0;

	if (
		(CheckFileExtension(f,".mus")) &&
		(!IsSection(f))
	   )
	{
		mapfile=OpenMAPFile(f);
		if (mapfile==INVALID_HANDLE_VALUE)
		{
			GlobalFree(LPData(f)->buffer);
			GlobalFree(f->afData);
			FreeASFBlockChain(chain); // ???
			SetLastError(PRIVEC(IDS_NOMAP));
			return FALSE;
		}
		hwnd=GetFocus();
		pwnd=CreateDialog(Plugin.hDllInst,"Wait",Plugin.hMainWindow,(DLGPROC)WaitingDlgProc);
		SetWindowText(pwnd,"Playback startup");
		SetDlgItemText(pwnd,ID_WAITMSG,"Walking MUS sections chain. Please wait...");
		if ((LPData(f)->musChain=ReadMUSSectionChain(pwnd,(opWalkChain)?f:NULL,mapfile))==NULL)
		{
			SetFocus(hwnd);
			DestroyWindow(pwnd);
			UpdateWindow(hwnd);
			CloseHandle(mapfile);
			GlobalFree(LPData(f)->buffer);
			GlobalFree(f->afData);
			FreeASFBlockChain(chain); // ???
			SetLastError(PRIVEC(IDS_MAPERROR));
			return FALSE;
		}
		SetFocus(hwnd);
		DestroyWindow(pwnd);
		UpdateWindow(hwnd);
		CloseHandle(mapfile);
		LPData(f)->curSection=LPData(f)->musChain;
		Plugin.fsh->SetFilePointer(f,LPData(f)->curSection->start,FILE_BEGIN);
	}
	else
	{
		LPData(f)->musChain=NULL;
		LPData(f)->curSection=NULL;
		LPData(f)->numSCDl=0;
	}

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

	FreeASFBlockChain(LPData(f)->chain);
	FreeMUSSectionChain(LPData(f)->musChain);
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

DWORD DecodeSCDl(FSHandle* f, char* chunk, DWORD size, char* buff, DWORD outsamples)
{
    DWORD  i,outsize,count,maxoutsize;
    short *dstpcm16;
	char  *leftdata,*rightdata;
	
	BOOL  usehinibble;
	DWORD blocksize;

    if (f==NULL)
		return 0;

	switch (LPData(f)->compression)
	{
		case SC_EA_ADPCM:
			dstpcm16=(short*)buff;
			outsize=0;
			i=0;
			if (LPData(f)->split)
			{
				leftdata=chunk+8+(*((DWORD*)chunk));
				rightdata=chunk+8+(*((DWORD*)(chunk+4)));
				LPData(f)->dsLeft.lCurSample=(long)(*((short*)leftdata));
				LPData(f)->dsLeft.lPrevSample=(long)(*((short*)(leftdata+2)));
				leftdata+=4;
				LPData(f)->dsRight.lCurSample=(long)(*((short*)rightdata));
				LPData(f)->dsRight.lPrevSample=(long)(*((short*)(rightdata+2)));
				rightdata+=4;
			}
			blocksize=(LPData(f)->channels==1)?0x0e:0x1c;
			maxoutsize=outsamples*(LPData(f)->align);
			while ((i<size) && (outsize<maxoutsize))
			{
				switch (LPData(f)->channels)
				{
					case 2:
						if (LPData(f)->split)
						{
							BlockInit_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(leftdata[i]),(BYTE)LONIBBLE(leftdata[i]));
							BlockInit_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)HINIBBLE(rightdata[i]),(BYTE)LONIBBLE(rightdata[i]));
						}
						else
						{
							BlockInit_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]),(BYTE)HINIBBLE(chunk[i+1]));
							BlockInit_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)LONIBBLE(chunk[i]),(BYTE)LONIBBLE(chunk[i+1]));
							i++;
						}
						i++;
						break;
					case 1:
						BlockInit_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]),(BYTE)LONIBBLE(chunk[i]));
						i++;
						break;
					default: // ???
						break;
				}
				usehinibble=TRUE;
				for (count=0;count<blocksize;count++)
				{
					if ((i>=size) || (outsize>=maxoutsize))
						break;
					switch (LPData(f)->channels)
					{
						case 2:
							if (LPData(f)->split)
							{
								Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)((usehinibble)?HINIBBLE(leftdata[i]):LONIBBLE(leftdata[i])));
								Step_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)((usehinibble)?HINIBBLE(rightdata[i]):LONIBBLE(rightdata[i])));
								if (!usehinibble)
									i++;
								usehinibble=!usehinibble;
							}
							else
							{
								Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]));
								Step_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)LONIBBLE(chunk[i]));
								i++;
							}
							if (opUseClipping)
							{
								Clip16bitSample(&(LPData(f)->dsLeft.lCurSample));
								Clip16bitSample(&(LPData(f)->dsRight.lCurSample));
							}
							*dstpcm16++=(SHORT)(LPData(f)->dsLeft.lCurSample);
							*dstpcm16++=(SHORT)(LPData(f)->dsRight.lCurSample);
							outsize+=4;
							break;
						case 1:
							Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]));
							if (opUseClipping)
								Clip16bitSample(&(LPData(f)->dsLeft.lCurSample));
							*dstpcm16++=(SHORT)(LPData(f)->dsLeft.lCurSample);
							Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)LONIBBLE(chunk[i]));
							if (opUseClipping)
								Clip16bitSample(&(LPData(f)->dsLeft.lCurSample));
							*dstpcm16++=(SHORT)(LPData(f)->dsLeft.lCurSample);
							i++;
							outsize+=4;
							break;
						default: // ???
							break;
					}
				}
			}
			break;
		default: // ???
			outsize=0;
			break;
	}

    return outsize;
}

DWORD InterleaveSplitSCDl(char* chunk, DWORD size, char* buff, DWORD outsize)
{
	DWORD  i;
	short *dstpcm16,*leftdata,*rightdata;

	leftdata=(short*)(chunk+8+(*((DWORD*)chunk)));
	rightdata=(short*)(chunk+8+(*((DWORD*)(chunk+4))));
	dstpcm16=(short*)buff;
	for (i=0;i<outsize;i++)
	{
		*dstpcm16++=leftdata[i];
		*dstpcm16++=rightdata[i];
	}

	return (4*outsize);
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
	ASFBlockHeader asfblockhdr;
    DWORD	       read,curpos;
    AFile         *node;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // SCHl
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying EACS file...");
			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Walking EACS blocks chain...");
			curpos=pos;
			while (!(RFPLUGIN(f)->EndOfFile(f)))
			{
				if ((BOOL)SendMessage(hwnd,WM_GAP_ISCANCELLED,0,0))
					return NULL;
				RFPLUGIN(f)->ReadFile(f,&asfblockhdr,sizeof(ASFBlockHeader),&read);
				if (read<sizeof(ASFBlockHeader))
					break; // ???
				curpos=RFPLUGIN(f)->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
				if (!memcmp(asfblockhdr.id,IDSTR_SCEl,4))
					break;
				SendMessage(hwnd,WM_GAP_SHOWFILEPROGRESS,0,(LPARAM)f);
			}
			break;
		default: // ???
			break;
	}
	node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
	node->fsStart=pos;
    node->fsLength=curpos-pos;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"EACS file correct.");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate,nsamp,datastart;
	WORD   channels,bits;
    BYTE   compression,split;
	char   mapname[MAX_PATH];

    if (f==NULL)
		return NULL;

    if (!ReadHeader(f,&rate,&channels,&bits,&compression,&nsamp,&datastart,&split))
		return NULL;
	if (!lstrcmpi(GetFileExtension(rfName),".mus"))
		lstrcpy(mapname,"<mus>");
	else
		lstrcpy(mapname,"");
	if (
		(musMAP==ID_MAP_ASK) &&
		(!lstrcmpi(GetFileExtension(rfName),".mus"))
	   )
	{
		if (!GetMAPFileName(GetFocus(),mapname,sizeof(mapname),(char*)rfName))
			lstrcpy(mapname,"<mus>");
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    lstrcpy(node->afDataString,mapname);
	lstrcpy(node->afName,"");

    return node;
}

DWORD __stdcall GetTime(AFile* node)
{
    FSHandle* f;
    DWORD  fsamp,rate,nsamp,datastart;
	WORD   channels,bits;
    BYTE   compression,split;
	HWND   pwnd,hwnd;
	HANDLE mapfile;
	MUSSectionNode *musChain,*walker;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

	fsamp=0;

	if (
		(CheckFileExtension(f,".mus")) &&
		(!IsSection(f))
	   )
	{
		if (opNoMUSTime)
			return -1;
		hwnd=GetFocus();
		pwnd=CreateDialog(Plugin.hDllInst,"Wait",Plugin.hMainWindow,(DLGPROC)WaitingDlgProc);
		SetWindowText(pwnd,"Calculating time");
		SetDlgItemText(pwnd,ID_WAITMSG,"Walking MUS sections chain. Please wait...");
		if (!ReadHeader(f,&rate,&channels,&bits,&compression,&nsamp,&datastart,&split))
		{
			Plugin.fsh->CloseFile(f);
			SetFocus(hwnd);
			DestroyWindow(pwnd);
			UpdateWindow(hwnd);
			return -1;
		}
		mapfile=OpenMAPFile(f);
		if (mapfile==INVALID_HANDLE_VALUE)
		{
			Plugin.fsh->CloseFile(f);
			SetFocus(hwnd);
			DestroyWindow(pwnd);
			UpdateWindow(hwnd);
			return -1;
		}
		if ((musChain=ReadMUSSectionChain(pwnd,f,mapfile))==NULL)
		{
			CloseHandle(mapfile);
			Plugin.fsh->CloseFile(f);
			SetFocus(hwnd);
			DestroyWindow(pwnd);
			UpdateWindow(hwnd);
			return -1;
		}
		CloseHandle(mapfile);
		walker=musChain;
		while (walker!=NULL)
		{
			fsamp+=walker->outsize;
			if (walker->end)
				walker=NULL;
			else
				walker=walker->next;
		}
		FreeMUSSectionChain(musChain);
		SetFocus(hwnd);
		DestroyWindow(pwnd);
		UpdateWindow(hwnd);
	}
	else if (!ReadHeader(f,&rate,&channels,&bits,&compression,&fsamp,&datastart,&split))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
	}

	Plugin.fsh->CloseFile(f);

	return MulDiv(1000,fsamp,rate);
}

void SeekToPos(FSHandle* f, DWORD pos);

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD toRead,read,pcmSize,outSize,decoded,shift,loopPos;
	short sample1,sample2;
	ASFBlockHeader asfblockhdr;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	*buffpos=LPData(file)->curTime;
    pcmSize=0;
	if (LPData(file)->chain!=NULL)
	{
		lstrcpyn(asfblockhdr.id,"\0\0\0",4);
		asfblockhdr.size=0L;
	}
    while (1)
    {
		if (LPData(file)->musChain==NULL)
		{
			if (LPData(file)->chain!=NULL)
			{
				if (LPData(file)->curnode==NULL)
					break;
			}
			else
			{
				if (Plugin.fsh->EndOfFile(file))
					break;
			}
		}
		else
		{
			if (LPData(file)->curSection==NULL)
				break;
		}
		shift=0;
		outSize=0;
		while (outSize==0)
		{
			if (LPData(file)->musChain==NULL)
			{
				if (LPData(file)->chain==NULL)
				{
					if (Plugin.fsh->EndOfFile(file))
						break;
					Plugin.fsh->ReadFile(file,&asfblockhdr,sizeof(ASFBlockHeader),&read);
					if (read<sizeof(ASFBlockHeader))
						return 0; // ???
				}
				else
				{
					if (LPData(file)->curnode==NULL)
						break;
					if (LPData(file)->curnode->type==ASF_BT_UNKNOWN)
					{
						LPData(file)->curnode=LPData(file)->curnode->next;
						continue;
					}
				}
			}
			else
			{
				if (LPData(file)->curSection==NULL)
					break;
				Plugin.fsh->ReadFile(file,&asfblockhdr,sizeof(ASFBlockHeader),&read);
				if (read<sizeof(ASFBlockHeader))
					return 0; // ???
			}
			if (
			    (!memcmp(asfblockhdr.id,IDSTR_SCDl,4)) ||
				((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_DATA))
			   )
			{
				if (LPData(file)->curnode!=NULL)
					Plugin.fsh->SetFilePointer(file,LPData(file)->curnode->start,FILE_BEGIN);
				Plugin.fsh->ReadFile(file,&outSize,sizeof(DWORD),&read);
				if (read<sizeof(DWORD))
					return 0; // ???
				shift=4;
				break;
			}
			else if (
					 (!memcmp(asfblockhdr.id,IDSTR_SCCl,4)) ||
					 ((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_COUNT))
					)
			{
				if (LPData(file)->musChain!=NULL)
				{
					Plugin.fsh->ReadFile(file,&(LPData(file)->numSCDl),sizeof(DWORD),&read);
					if (read<sizeof(DWORD))
						return 0; // ???
					Plugin.fsh->SetFilePointer(file,-(LONG)read,FILE_CURRENT);
				}
			}
			else if (
					 (!memcmp(asfblockhdr.id,IDSTR_SCLl,4)) ||
					 ((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_LOOP))
					)
			{
				if (LPData(file)->curnode!=NULL)
					Plugin.fsh->SetFilePointer(file,LPData(file)->curnode->start,FILE_BEGIN);
				if ((!opIgnoreLoops) && (LPData(file)->musChain==NULL))
				{
					Plugin.fsh->ReadFile(file,&loopPos,sizeof(DWORD),&read);
					if (read<sizeof(DWORD))
						loopPos=0L; // ???
					SeekToPos(file,loopPos);
					LPData(file)->curTime=MulDiv(1000,loopPos,LPData(file)->rate);
					if (pcmSize>0)
						return pcmSize;
					*buffpos=LPData(file)->curTime;
					continue;
				}
			}
			if (LPData(file)->chain==NULL)
			{
				Plugin.fsh->SetFilePointer(file,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
				if (!memcmp(asfblockhdr.id,IDSTR_SCEl,4)) // ???
					AlignFilePointer4(file);
			}
			else
				LPData(file)->curnode=LPData(file)->curnode->next;
		}
		if (outSize==0)
			break;
		if (outSize>buffsize/LPData(file)->align)
		{
			Plugin.fsh->SetFilePointer(file,-(LONG)(shift+sizeof(ASFBlockHeader)),FILE_CURRENT);
			break;
		}
		switch (LPData(file)->compression)
		{
			case SC_EA_ADPCM:
				if (!(LPData(file)->split))
				{
					Plugin.fsh->ReadFile(file,&sample1,2,&read);
					Plugin.fsh->ReadFile(file,&sample2,2,&read);
					Init_EA_ADPCM(&(LPData(file)->dsLeft),(long)sample1,(long)sample2);
					shift+=4;
					if (LPData(file)->channels==2)
					{
						Plugin.fsh->ReadFile(file,&sample1,2,&read);
						Plugin.fsh->ReadFile(file,&sample2,2,&read);
						Init_EA_ADPCM(&(LPData(file)->dsRight),(long)sample1,(long)sample2);
						shift+=4;
					}
				}
				break;
			case SC_NONE:
				break;
			default: // ???
				break;
		}
		toRead=((LPData(file)->curnode==NULL)?(asfblockhdr.size-sizeof(ASFBlockHeader)):(LPData(file)->curnode->size))-shift;
		switch (LPData(file)->compression)
		{
			case SC_EA_ADPCM:
				Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
				if (read<toRead)
					return 0; // ???
				decoded=DecodeSCDl(file,LPData(file)->buffer,read,buffer+pcmSize,outSize);
				break;
			case SC_NONE:
				if (LPData(file)->split)
				{
					Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
					if (read<toRead)
						return 0; // ???
					decoded=InterleaveSplitSCDl(LPData(file)->buffer,read,buffer+pcmSize,outSize);
				}
				else
				{
					Plugin.fsh->ReadFile(file,buffer+pcmSize,toRead,&decoded);
					if (decoded<toRead)
						return 0; // ???
				}
				break;
			default: // ???
				break;
		}
		if (decoded==0L)
			break; // ???
		if (LPData(file)->samplesToSkip>0)
		{
			decoded-=(LPData(file)->samplesToSkip)*(LPData(file)->align);
			memmove(buffer+pcmSize,buffer+pcmSize+(LPData(file)->samplesToSkip)*(LPData(file)->align),decoded);
			LPData(file)->samplesToSkip=0;
		}
		if (LPData(file)->curnode!=NULL)
			LPData(file)->curnode=LPData(file)->curnode->next;
		if (LPData(file)->musChain!=NULL)
		{
			LPData(file)->numSCDl--;
			if (LPData(file)->numSCDl==0)
			{
				if (LPData(file)->curSection->end)
				{
					if ((opIgnoreLoops) || (LPData(file)->curSection->next==NULL))
						LPData(file)->curSection=NULL;
					else
					{
						MUSSectionNode *walker;
						DWORD fsamp=0,rate,datastart;
						WORD  channels,bits;
						BYTE  compression,split;

						LPData(file)->curSection=LPData(file)->curSection->next;
						walker=LPData(file)->musChain;
						while (walker!=LPData(file)->curSection)
						{
							if (walker->outsize==0)
							{
								Plugin.fsh->SetFilePointer(file,walker->start+sizeof(ASFBlockHeader),FILE_BEGIN);
								ParsePTHeader(file,&rate,&channels,&bits,&compression,&(walker->outsize),&datastart,&split);
							}
							fsamp+=walker->outsize;
							walker=walker->next;
						}
						LPData(file)->curTime=MulDiv(1000,fsamp,LPData(file)->rate);
						if (pcmSize>0)
						{
							Plugin.fsh->SetFilePointer(file,LPData(file)->curSection->start,FILE_BEGIN);
							return pcmSize;
						}
						*buffpos=LPData(file)->curTime;
					}
				}
				else
					LPData(file)->curSection=LPData(file)->curSection->next;
				if (LPData(file)->curSection!=NULL)
					Plugin.fsh->SetFilePointer(file,LPData(file)->curSection->start,FILE_BEGIN);
			}
		}
		pcmSize+=decoded;
		buffsize-=decoded;
    }
	LPData(file)->curTime+=MulDiv(1000,pcmSize,(LPData(file)->align)*(LPData(file)->rate));

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

AFile* ibNode;
DWORD  ibSize,ibRate,ibTime;
WORD   ibChannels,ibBits;
BYTE   ibCompression,ibSplit;

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
			wsprintf(str,"Format: %s (SCDl %s)\r\n"
						 "Compression: %s (%u)\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (!lstrcmpi(ibNode->afDataString,""))?"ASF":"MUS",
						 (ibSplit)?"split":"interleaved",
						 (ibCompression==SC_EA_ADPCM)?"EA ADPCM":((ibCompression==SC_NONE)?"None":"Unknown"),
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

void SetCheckBox(HWND hwnd, int cbId, BOOL cbVal)
{
    CheckDlgButton(hwnd,cbId,(cbVal)?BST_CHECKED:BST_UNCHECKED);
}

BOOL GetCheckBox(HWND hwnd, int cbId)
{
    return (BOOL)(IsDlgButtonChecked(hwnd,cbId)==BST_CHECKED);
}

int GetRadioButton(HWND hwnd, int first, int last)
{
    int i;

    for (i=first;i<=last;i++)
		if (IsDlgButtonChecked(hwnd,i)==BST_CHECKED)
			return i;

    return first;
}

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetCheckBox(hwnd,ID_IGNORELOOPS,opIgnoreLoops);
			SetCheckBox(hwnd,ID_WALK,opWalkChain);
			SetCheckBox(hwnd,ID_NOMUSTIME,opNoMUSTime);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			CheckRadioButton(hwnd,ID_MAP_DEFAULT,ID_MAP_ASK,musMAP);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_IGNORELOOPS,TRUE);
					SetCheckBox(hwnd,ID_WALK,FALSE);
					SetCheckBox(hwnd,ID_NOMUSTIME,FALSE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					CheckRadioButton(hwnd,ID_MAP_DEFAULT,ID_MAP_ASK,ID_MAP_DEFAULT);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opIgnoreLoops=GetCheckBox(hwnd,ID_IGNORELOOPS);
					opWalkChain=GetCheckBox(hwnd,ID_WALK);
					opNoMUSTime=GetCheckBox(hwnd,ID_NOMUSTIME);
					opUseClipping=GetCheckBox(hwnd,ID_USECLIPPING);
					musMAP=GetRadioButton(hwnd,ID_MAP_DEFAULT,ID_MAP_ASK);
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
    char      str[256];
	DWORD     nsamp,datastart;
    FSHandle *f;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadHeader(f,&ibRate,&ibChannels,&ibBits,&ibCompression,&nsamp,&datastart,&ibSplit))
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

void SeekToPos(FSHandle* f, DWORD pos)
{
    DWORD			read,outsize,rate,datastart;
	WORD			channels,bits;
    BYTE			compression,split;
	ASFBlockNode   *walker;
	MUSSectionNode *mwalker;
	ASFBlockHeader	asfblockhdr;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

	if (pos==0)
	{
		if (LPData(f)->musChain!=NULL)
		{
			LPData(f)->curSection=LPData(f)->musChain;
			Plugin.fsh->SetFilePointer(f,LPData(f)->curSection->start,FILE_BEGIN);
		}
		else
		{
			Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
			LPData(f)->curnode=LPData(f)->chain;
		}
		return;
	}

	if (LPData(f)->musChain!=NULL)
	{
		mwalker=LPData(f)->musChain;
		while (mwalker!=NULL)
		{
			if (mwalker->outsize==0)
			{
				Plugin.fsh->SetFilePointer(f,mwalker->start+sizeof(ASFBlockHeader),FILE_BEGIN);
				ParsePTHeader(f,&rate,&channels,&bits,&compression,&(mwalker->outsize),&datastart,&split);
			}
			if (mwalker->outsize>pos)
				break;
			else
				pos-=mwalker->outsize;
			if (mwalker->end)
				mwalker=NULL;
			else
				mwalker=mwalker->next;
		}
		if (mwalker==NULL)
			return;
		LPData(f)->curSection=mwalker;
		Plugin.fsh->SetFilePointer(f,mwalker->start,FILE_BEGIN);
		while (!(Plugin.fsh->EndOfFile(f)))
		{
			Plugin.fsh->ReadFile(f,&asfblockhdr,sizeof(ASFBlockHeader),&read);
			if (read<sizeof(ASFBlockHeader))
				break; // ???
			if (!memcmp(asfblockhdr.id,IDSTR_SCCl,4))
			{
				Plugin.fsh->ReadFile(f,&(LPData(f)->numSCDl),sizeof(DWORD),&read);
				if (read<sizeof(DWORD))
					LPData(f)->numSCDl=0L; // ???
				Plugin.fsh->SetFilePointer(f,-(LONG)read,FILE_CURRENT);
			}
			else if (!memcmp(asfblockhdr.id,IDSTR_SCDl,4))
			{
				Plugin.fsh->ReadFile(f,&outsize,sizeof(DWORD),&read);
				if (read<sizeof(DWORD))
					break; // ???
				Plugin.fsh->SetFilePointer(f,-(LONG)read,FILE_CURRENT);
				if (outsize>pos)
					break;
				else
				{
					pos-=outsize;
					LPData(f)->numSCDl--;
					if (LPData(f)->numSCDl==0)
						return; // ???
				}
			}
			Plugin.fsh->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
		}
		if (outsize<=pos)
			return;
	}
	else if (LPData(f)->chain!=NULL)
	{
		walker=LPData(f)->chain;
		while (walker!=NULL)
		{
			if (walker->outsize>pos)
				break;
			pos-=walker->outsize;
			walker=walker->next;
		}
		if (walker==NULL)
			return;
		LPData(f)->curnode=walker;
		switch (walker->type)
		{
			case ASF_BT_DATA:
				Plugin.fsh->SetFilePointer(f,walker->start,FILE_BEGIN);
				break;
			default: // ???
				return;
		}
		outsize=walker->outsize;
	}
	else
	{
		Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
		outsize=0;
		while (!(Plugin.fsh->EndOfFile(f)))
		{
			Plugin.fsh->ReadFile(f,&asfblockhdr,sizeof(ASFBlockHeader),&read);
			if (read<sizeof(ASFBlockHeader))
				break; // ???
			if (!memcmp(asfblockhdr.id,IDSTR_SCDl,4))
			{
				Plugin.fsh->ReadFile(f,&outsize,sizeof(DWORD),&read);
				if (read<sizeof(DWORD))
					break; // ???
				Plugin.fsh->SetFilePointer(f,-(LONG)read,FILE_CURRENT);
				if (outsize>pos)
					break;
				pos-=outsize;
			}
			Plugin.fsh->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
			if (!memcmp(asfblockhdr.id,IDSTR_SCEl,4)) // ???
				AlignFilePointer4(f);
		}
		if (outsize<=pos)
			return;
	}
	Plugin.fsh->SetFilePointer(f,-(LONG)sizeof(ASFBlockHeader),FILE_CURRENT);
	LPData(f)->samplesToSkip=pos;
}

void __stdcall Seek(FSHandle* f, DWORD pos)
{
	LPData(f)->curTime=pos;
	SeekToPos(f,MulDiv(pos,LPData(f)->rate,1000));
}

SearchPattern patterns[]=
{
	{4,IDSTR_SCHl}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX EACS Audio File Decoder",
    "v0.98",
    "Electronic Arts Music Files (*.ASF;*.STR;*.MUS)\0*.asf;*.str;*.mus\0"
	"Electronic Arts Video Files (*.DCT;*.MAD;*.TGQ;*.WVE;*.UV;*.UV2)\0*.dct;*.mad;*.tgq;*.wve;*.uv;*.uv2\0",
    "EACS",
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
