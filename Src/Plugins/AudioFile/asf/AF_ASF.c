/*
 * ASF plug-in source code
 * (Electronic Arts music/sfx/speech)
 *
 * Copyright (C) 1999-2000 ANX Software
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <commctrl.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_ASF.h"
#include "resource.h"

void __cdecl DebugMessageBox(const char* title, const char* fmt, ...)
{
	char    str[512];
	va_list val;

	va_start(val,fmt);
	wvsprintf(str,fmt,val);
	MessageBox(GetFocus(),str,title,MB_OK | MB_ICONEXCLAMATION);
	va_end(val);
}

#define WM_GAP_WAITINGTOY (WM_USER+1)

#define BUFFERSIZE (64000ul)

#define ASF_BT_HEADER  (0)
#define ASF_BT_DATA    (1)
#define ASF_BT_LOOP    (2)
#define ASF_BT_END     (3)
#define ASF_BT_UNKNOWN (0xFF)

typedef struct tagASFBlockNode
{
	BYTE   type;
	DWORD  start,size,datasize;
	struct tagASFBlockNode *next;
} ASFBlockNode;

typedef struct tagData
{
    BYTE   type,compression;
	WORD   channels,bits,align,factor;
	DWORD  rate,curTime;
	DWORD  datastart,datasize;
    DWORD  blockRemained;
	DWORD  loopPos;
    long   Index1,Index2;
    long   Cur_Sample1,Cur_Sample2;
    char  *buffer;
	ASFBlockNode *chain,*curnode;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

BOOL opIgnoreLoops,
     opWalkChain,
	 opCheckRate,
	 opCheckChannels,
	 opCheckBits,
	 opUseClipping;

int Index_Adjust[]=
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

int Step_Table[]=
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
		opIgnoreLoops=TRUE;
		opWalkChain=FALSE;
		opCheckRate=TRUE;
		opCheckChannels=TRUE;
		opCheckBits=TRUE;
		opUseClipping=TRUE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"IgnoreLoops","on",str,40,Plugin.szINIFileName);
    opIgnoreLoops=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"WalkChain","off",str,40,Plugin.szINIFileName);
    opWalkChain=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckRate","on",str,40,Plugin.szINIFileName);
	opCheckRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckChannels","on",str,40,Plugin.szINIFileName);
	opCheckChannels=LoadOptionBool(str);
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

	WritePrivateProfileString(Plugin.Description,"IgnoreLoops",SaveOptionBool(opIgnoreLoops),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"WalkChain",SaveOptionBool(opWalkChain),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckRate",SaveOptionBool(opCheckRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckChannels",SaveOptionBool(opCheckChannels),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckBits",SaveOptionBool(opCheckBits),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
}

BOOL ReadHeader
(
	FSHandle* file,
	DWORD* rate,
	WORD* channels,
	WORD* bits,
	BYTE* type,
	BYTE* compression,
	DWORD* numsamples,
	DWORD* datastart
)
{
	EACSHeader eacshdr;
    DWORD      read,shift;
	char	   id[4],*endptr;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
	lstrcpyn(id,"\0\0\0",4);
	Plugin.fsh->ReadFile(file,id,4,&read);
	if (!memcmp(id,IDSTR_EACS,4))
		Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
	else if (!memcmp(id,IDSTR_1SNh,4))
		Plugin.fsh->SetFilePointer(file,sizeof(ASFBlockHeader),FILE_BEGIN);
	else if (file->node!=NULL)
	{
		shift=strtoul(file->node->afDataString,&endptr,0x10);
		if (shift==-1)
			return FALSE;
		Plugin.fsh->SetFilePointer(file,shift,FILE_BEGIN);
		lstrcpyn(id,"\0\0\0",4);
		Plugin.fsh->ReadFile(file,id,4,&read);
		if (!memcmp(id,IDSTR_EACS,4))
			Plugin.fsh->SetFilePointer(file,shift,FILE_BEGIN);
		else
			return FALSE;
	}
	else
		return FALSE;
    Plugin.fsh->ReadFile(file,&eacshdr,sizeof(EACSHeader),&read);
	if (read<sizeof(EACSHeader))
		return FALSE;
    if (memcmp(eacshdr.id,IDSTR_EACS,4))
		return FALSE;
	if (opCheckRate)
	{
		if ((eacshdr.dwRate>96000) || (eacshdr.dwRate<4000)) // ???
			return FALSE;
	}
	if (opCheckChannels)
	{
		if ((eacshdr.bChannels>2) || (eacshdr.bChannels<1)) // ???
			return FALSE;
	}
	if (opCheckBits)
	{
		if ((eacshdr.bBits>2) || (eacshdr.bBits<1)) // ???
			return FALSE;
	}
	*rate=eacshdr.dwRate;
	*channels=eacshdr.bChannels;
	*bits=8*(eacshdr.bBits);
	*compression=eacshdr.bCompression;
	*type=eacshdr.bType;
	*numsamples=eacshdr.dwNumSamples;
	*datastart=eacshdr.dwDataStart;

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

ASFBlockNode* ReadASFBlockChain(FSHandle* f)
{
    DWORD          read;
	ASFBlockNode  *chain,**pcurnode;
	ASFBlockHeader asfblockhdr;
	EACSHeader     eacshdr;
	HWND	       pwnd,hwnd;

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
			break; // ???
		(*pcurnode)=(ASFBlockNode*)GlobalAlloc(GPTR,sizeof(ASFBlockNode));
		(*pcurnode)->next=NULL;
		(*pcurnode)->start=Plugin.fsh->GetFilePointer(f);
		(*pcurnode)->size=asfblockhdr.size-sizeof(ASFBlockHeader);
		(*pcurnode)->datasize=asfblockhdr.size-sizeof(ASFBlockHeader);
		if (!memcmp(asfblockhdr.id,IDSTR_1SNh,4))
		{
			Plugin.fsh->ReadFile(f,&eacshdr,sizeof(EACSHeader),&read);
			Plugin.fsh->SetFilePointer(f,-(LONG)sizeof(EACSHeader),FILE_CURRENT);
			(*pcurnode)->type=ASF_BT_HEADER;
			(*pcurnode)->start+=sizeof(EACSHeader);
			(*pcurnode)->datasize-=sizeof(EACSHeader)+((eacshdr.bCompression==EACS_IMA_ADPCM)?((eacshdr.bChannels==2)?20:12):0);
		}
		else if (!memcmp(asfblockhdr.id,IDSTR_1SNd,4))
		{
			(*pcurnode)->type=ASF_BT_DATA;
			(*pcurnode)->datasize-=((eacshdr.bCompression==EACS_IMA_ADPCM)?((eacshdr.bChannels==2)?20:12):0);
		}
		else if (!memcmp(asfblockhdr.id,IDSTR_1SNl,4))
		{
			(*pcurnode)->type=ASF_BT_LOOP;
			(*pcurnode)->datasize=0L;
		}
		else if (!memcmp(asfblockhdr.id,IDSTR_1SNe,4))
		{
			(*pcurnode)->type=ASF_BT_END;
			(*pcurnode)->datasize=0L;
			break;
		}
		else
		{
			(*pcurnode)->type=ASF_BT_UNKNOWN;
			(*pcurnode)->datasize=0L;
		}
		pcurnode=&((*pcurnode)->next);
		Plugin.fsh->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
		SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,MulDiv(100,Plugin.fsh->GetFilePointer(f),Plugin.fsh->GetFileSize(f)),0L);
		SendMessage(pwnd,WM_GAP_WAITINGTOY,0,0);
    }
	SetFocus(hwnd);
    DestroyWindow(pwnd);
	UpdateWindow(hwnd);

	return chain;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	DWORD nsamp,datastart;
	BYTE  type,compression;
	ASFBlockNode *chain;

    if (f==NULL)
		return FALSE;

    if (!ReadHeader(f,rate,channels,bits,&type,&compression,&nsamp,&datastart))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	switch (compression)
	{
		case EACS_PCM:
		case EACS_IMA_ADPCM:
			break;
		default:
			SetLastError(PRIVEC(IDS_NODECODER));
			return FALSE;
	}
	switch (type)
	{
		case EACS_MULTIBLOCK:
			if (opWalkChain)
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
			break;
		case EACS_SINGLEBLOCK:
			chain=NULL;
			break;
		default:
			SetLastError(PRIVEC(IDS_UNKNOWNTYPE));
			return FALSE;
	}
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		FreeASFBlockChain(chain);
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
    LPData(f)->blockRemained=0;
	LPData(f)->chain=chain;
	LPData(f)->curnode=chain;
    LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
    if (LPData(f)->buffer==NULL)
    {
		GlobalFree(f->afData);
		FreeASFBlockChain(chain);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
    }
	LPData(f)->align=(*channels)*(*bits/8);
	LPData(f)->factor=(compression==EACS_IMA_ADPCM)?(((*bits)==16)?4:2):1; // ???
    LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->rate=*rate;
	LPData(f)->type=type;
	LPData(f)->compression=compression;
	switch (type)
	{
		case EACS_MULTIBLOCK:
			Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
			break;
		case EACS_SINGLEBLOCK:
			Plugin.fsh->SetFilePointer(f,datastart,FILE_BEGIN);
			LPData(f)->datastart=datastart;
			LPData(f)->datasize=MulDiv(nsamp,LPData(f)->align,LPData(f)->factor);
			LPData(f)->blockRemained=LPData(f)->datasize;
			break;
		default: // ???
			break;
	}
	LPData(f)->Index1=0;
	LPData(f)->Index2=0;
	LPData(f)->Cur_Sample1=0;
	LPData(f)->Cur_Sample2=0;
	LPData(f)->loopPos=-1;
	LPData(f)->curTime=0;

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

	FreeASFBlockChain(LPData(f)->chain);
    return (
			(GlobalFree(LPData(f)->buffer)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

#define SIGNED8(x) ((x)^0x80)

void Clip16BitSample(long *sample)
{
	if ((*sample)>32767)
		(*sample)=32767;
    else if ((*sample)<-32768)
		(*sample)=-32768;
}

void ClipIndex(long *pindex)
{
	if ((*pindex)>88)
	    *pindex=88;
	else if ((*pindex)<0)
		*pindex=0;
}

void StepADPCM(BYTE code, long *pindex, long *pcur_sample)
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
	ClipIndex(pindex);
}

DWORD DecodeBuffer(FSHandle* f, char* chunk, DWORD size, char* buff)
{
    DWORD  i;
	BYTE   Code;
	char  *srcpcm8;
	BYTE  *dstpcm8;
    short *dstpcm16;
    DWORD  outsize;

    if (f==NULL)
		return 0;

	switch (LPData(f)->compression)
	{
		case EACS_IMA_ADPCM:
			if (LPData(f)->bits==16)
			{
				dstpcm16=(signed short*)buff;
				outsize=0;
				for (i=0;i<size;i++)
				{
					Code=NIBBLE(chunk[i],1);
					StepADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
					if (opUseClipping)
						Clip16BitSample(&(LPData(f)->Cur_Sample1));
					*dstpcm16++=(signed short)(LPData(f)->Cur_Sample1);
					outsize+=2;
					Code=NIBBLE(chunk[i],0);
					if (LPData(f)->channels==2)
					{
						StepADPCM(Code,&(LPData(f)->Index2),&(LPData(f)->Cur_Sample2));
						if (opUseClipping)
							Clip16BitSample(&(LPData(f)->Cur_Sample2));
						*dstpcm16++=(signed short)(LPData(f)->Cur_Sample2);
					}
					else
					{
						StepADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
						if (opUseClipping)
							Clip16BitSample(&(LPData(f)->Cur_Sample1));
						*dstpcm16++=(signed short)(LPData(f)->Cur_Sample1);
					}
					outsize+=2;
				}
			}
			else
			{
				dstpcm8=(BYTE*)buff;
				outsize=0;
				for (i=0;i<size;i++)
				{
					Code=NIBBLE(chunk[i],1);
					StepADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
					if (opUseClipping)
						Clip16BitSample(&(LPData(f)->Cur_Sample1));
					*dstpcm8++=SIGNED8( (CHAR) ( ( (LPData(f)->Cur_Sample1) >> 8 ) & 0xFF ) );
					outsize+=1;
					Code=NIBBLE(chunk[i],0);
					if (LPData(f)->channels==2)
					{
						StepADPCM(Code,&(LPData(f)->Index2),&(LPData(f)->Cur_Sample2));
						if (opUseClipping)
							Clip16BitSample(&(LPData(f)->Cur_Sample2));
						*dstpcm8++=SIGNED8( (CHAR) ( ( (LPData(f)->Cur_Sample2) >> 8 ) & 0xFF ) );
					}
					else
					{
						StepADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
						if (opUseClipping)
							Clip16BitSample(&(LPData(f)->Cur_Sample1));
						*dstpcm8++=SIGNED8( (CHAR) ( ( (LPData(f)->Cur_Sample1) >> 8 ) & 0xFF ) );
					}
					outsize+=1;
				}
			}
			break;
		case EACS_PCM:
			outsize=size;
			switch (LPData(f)->bits)
			{
				case 16:
					if (buff!=chunk)
						memcpy(buff,chunk,size);
					break;
				case 8:
					srcpcm8=(signed char*)chunk;
					dstpcm8=(BYTE*)buff;
					for (i=0;i<size;i++,srcpcm8++,dstpcm8++)
						*dstpcm8=SIGNED8(*srcpcm8);
					break;
				default:
					break;
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
	ASFBlockHeader asfblockhdr;
	EACSHeader     eacshdr;
    DWORD	       read,start,length,curpos;
    AFile         *node;
	char	       id[4],datastr[100];

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying ASF file...");
    RFPLUGIN(f)->SetFilePointer(f,pos-sizeof(ASFBlockHeader),FILE_BEGIN);
	lstrcpyn(id,"\0\0\0",4);
    RFPLUGIN(f)->ReadFile(f,id,4,&read);
	if (!memcmp(id,IDSTR_1SNh,4))
	{
		SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Walking ASF blocks chain...");
		RFPLUGIN(f)->SetFilePointer(f,-4,FILE_CURRENT);
		curpos=start=RFPLUGIN(f)->GetFilePointer(f);
		while (!(RFPLUGIN(f)->EndOfFile(f)))
		{
			if ((BOOL)SendMessage(hwnd,WM_GAP_ISCANCELLED,0,0))
				return NULL;
			RFPLUGIN(f)->ReadFile(f,&asfblockhdr,sizeof(ASFBlockHeader),&read);
			if (read<sizeof(ASFBlockHeader))
				break; // ???
			curpos=RFPLUGIN(f)->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
			if (!memcmp(asfblockhdr.id,IDSTR_1SNe,4))
				break;
			SendMessage(hwnd,WM_GAP_SHOWFILEPROGRESS,0,(LPARAM)f);
		}
		length=curpos-start;
		lstrcpy(datastr,"");
		*newpos=start+length;
	}
	else
	{
		RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
		RFPLUGIN(f)->ReadFile(f,&eacshdr,sizeof(EACSHeader),&read);
		if (read<sizeof(EACSHeader))
			return NULL;
		if (opCheckRate)
		{
			if ((eacshdr.dwRate>96000) || (eacshdr.dwRate<4000)) // ???
				return NULL;
		}
		if (opCheckChannels)
		{
			if ((eacshdr.bChannels>2) || (eacshdr.bChannels<1)) // ???
				return NULL;
		}
		if (opCheckBits)
		{
			if ((eacshdr.bBits>2) || (eacshdr.bBits<1)) // ???
				return NULL;
		}
		start=0;
		length=RFPLUGIN(f)->GetFileSize(f);
		wsprintf(datastr,"%lX",pos);
		*newpos=pos+sizeof(EACSHeader);
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=start;
    node->fsLength=length;
    lstrcpy(node->afDataString,datastr);
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"ASF file correct.");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate,nsamp,datastart;
	WORD   channels,bits;
    BYTE   type,compression;

    if (f==NULL)
		return NULL;

    if (!ReadHeader(f,&rate,&channels,&bits,&type,&compression,&nsamp,&datastart))
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
    BYTE  type,compression;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!ReadHeader(f,&rate,&channels,&bits,&type,&compression,&nsamp,&datastart))
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	Plugin.fsh->CloseFile(f);

	return MulDiv(1000,nsamp,rate);
}

void SeekToPos(FSHandle* f, DWORD pos);

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
	BOOL  ready;
    DWORD sizeToLoad,read,pcmSize;
	ASFBlockHeader asfblockhdr;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	*buffpos=LPData(file)->curTime;
    switch (LPData(file)->compression)
    {
		case EACS_IMA_ADPCM:
			if (LPData(file)->bits==16) // ???
				sizeToLoad=CORRALIGN(buffsize,LPData(file)->align)/4;
			else
				sizeToLoad=CORRALIGN(buffsize,LPData(file)->align)/2;
			break;
		case EACS_PCM:
			sizeToLoad=CORRALIGN(buffsize,LPData(file)->align);
			break;
		default: // ???
			sizeToLoad=0;
			break;
    }
    pcmSize=0;
	if (LPData(file)->chain!=NULL)
	{
		lstrcpyn(asfblockhdr.id,"\0\0\0",4);
		asfblockhdr.size=0L;
	}
    while (sizeToLoad>0)
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
		if (LPData(file)->blockRemained==0)
		{
			if (LPData(file)->type==EACS_MULTIBLOCK)
			{
				ready=FALSE;
				while (1)
				{
					if (LPData(file)->chain!=NULL)
					{
						if (LPData(file)->curnode==NULL)
							break;
						if (LPData(file)->curnode->type==ASF_BT_UNKNOWN)
						{
							LPData(file)->curnode=LPData(file)->curnode->next;
							continue;
						}
					}
					else
					{
						if (Plugin.fsh->EndOfFile(file))
							break;
						Plugin.fsh->ReadFile(file,&asfblockhdr,sizeof(ASFBlockHeader),&read);
						if (read<sizeof(ASFBlockHeader))
						{
							Plugin.fsh->SetFilePointer(file,-(LONG)read,FILE_CURRENT);
							break; // ???
						}
					}
					if (
						(!memcmp(asfblockhdr.id,IDSTR_1SNh,4)) ||
						((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_HEADER))
					   )
					{
						if (LPData(file)->curnode!=NULL)
							Plugin.fsh->SetFilePointer(file,LPData(file)->curnode->start,FILE_BEGIN);
						else
							Plugin.fsh->SetFilePointer(file,sizeof(EACSHeader),FILE_CURRENT);
						LPData(file)->blockRemained=((LPData(file)->curnode==NULL)?(asfblockhdr.size-sizeof(ASFBlockHeader)):(LPData(file)->curnode->size))-sizeof(EACSHeader);
						break;
					}
					else if (
							 (!memcmp(asfblockhdr.id,IDSTR_1SNd,4)) ||
							 ((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_DATA))
							)
					{
						if (LPData(file)->curnode!=NULL)
							Plugin.fsh->SetFilePointer(file,LPData(file)->curnode->start,FILE_BEGIN);
						LPData(file)->blockRemained=(LPData(file)->curnode==NULL)?(asfblockhdr.size-sizeof(ASFBlockHeader)):(LPData(file)->curnode->size);
						break;
					}
					else if (
							 (!memcmp(asfblockhdr.id,IDSTR_1SNl,4)) ||
							 ((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_LOOP))
							)
					{
						if (LPData(file)->curnode!=NULL)
							Plugin.fsh->SetFilePointer(file,LPData(file)->curnode->start,FILE_BEGIN);
						Plugin.fsh->ReadFile(file,&(LPData(file)->loopPos),sizeof(DWORD),&read);
						if (read<sizeof(DWORD))
							LPData(file)->loopPos=-1; // ???
						Plugin.fsh->SetFilePointer(file,-(LONG)read,FILE_CURRENT);
					}
					else if (
							 (!memcmp(asfblockhdr.id,IDSTR_1SNe,4)) ||
							 ((LPData(file)->curnode!=NULL) && (LPData(file)->curnode->type==ASF_BT_END))
							)
					{
						if ((LPData(file)->loopPos!=-1) && (!opIgnoreLoops))
						{
							SeekToPos(file,LPData(file)->loopPos);
							LPData(file)->curTime=MulDiv(1000,LPData(file)->loopPos,LPData(file)->rate);
							if (pcmSize>0)
								return pcmSize;
							*buffpos=LPData(file)->curTime;
							ready=TRUE;
						}
						break;
					}
					if (LPData(file)->chain==NULL)
						Plugin.fsh->SetFilePointer(file,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
					else
						LPData(file)->curnode=LPData(file)->curnode->next;
				}
				if (LPData(file)->blockRemained==0)
					break;
				if (ready)
					break;
				switch (LPData(file)->compression)
				{
					case EACS_IMA_ADPCM:
						Plugin.fsh->ReadFile(file,&(LPData(file)->blockRemained),4,&read);
						if (read<4)
							LPData(file)->blockRemained=0L; // ???
						Plugin.fsh->ReadFile(file,&(LPData(file)->Index1),4,&read);
						if (read<4)
							LPData(file)->Index1=0L; // ???
						ClipIndex(&(LPData(file)->Index1));
						if (LPData(file)->channels==2)
						{
							Plugin.fsh->ReadFile(file,&(LPData(file)->Index2),4,&read);
							if (read<4)
								LPData(file)->Index2=0L; // ???
							ClipIndex(&(LPData(file)->Index2));
						}
						Plugin.fsh->ReadFile(file,&(LPData(file)->Cur_Sample1),4,&read);
						if (read<4)
							LPData(file)->Cur_Sample1=0L; // ???
						if (LPData(file)->channels==2)
						{
							Plugin.fsh->ReadFile(file,&(LPData(file)->Cur_Sample2),4,&read);
							if (read<4)
								LPData(file)->Cur_Sample2=0L; // ???
						}
						break;
					case EACS_PCM:
						break;
					default: // ???
						break;
				}
			}
			else if (LPData(file)->type==EACS_SINGLEBLOCK)
				break;
			else // ???
				break;
		}
		Plugin.fsh->ReadFile(file,LPData(file)->buffer,min(min(LPData(file)->blockRemained,BUFFERSIZE),sizeToLoad),&read);
		if (read>0)
			pcmSize+=DecodeBuffer(file,LPData(file)->buffer,read,buffer+pcmSize);
		else
			break;
		sizeToLoad-=read;
		LPData(file)->blockRemained-=read;
		if ((LPData(file)->curnode!=NULL) && (LPData(file)->blockRemained==0))
			LPData(file)->curnode=LPData(file)->curnode->next;
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
BYTE   ibType,ibCompression;

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
			wsprintf(str,"Format: %s "IDSTR_EACS" (%u)\r\n"
						 "Compression: %s (%u)\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (ibType==EACS_MULTIBLOCK)?"Multi-block":((ibType==EACS_SINGLEBLOCK)?"Single-block":"Unknown"),
						 ibType,
						 (ibCompression==EACS_IMA_ADPCM)?"IMA ADPCM":((ibCompression==EACS_PCM)?"None":"Unknown"),
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

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetCheckBox(hwnd,ID_IGNORELOOPS,opIgnoreLoops);
			SetCheckBox(hwnd,ID_WALK,opWalkChain);
			SetCheckBox(hwnd,ID_CHECKRATE,opCheckRate);
			SetCheckBox(hwnd,ID_CHECKCHANNELS,opCheckChannels);
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
					SetCheckBox(hwnd,ID_IGNORELOOPS,TRUE);
					SetCheckBox(hwnd,ID_WALK,FALSE);
					SetCheckBox(hwnd,ID_CHECKRATE,TRUE);
					SetCheckBox(hwnd,ID_CHECKCHANNELS,TRUE);
					SetCheckBox(hwnd,ID_CHECKBITS,TRUE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opIgnoreLoops=GetCheckBox(hwnd,ID_IGNORELOOPS);
					opWalkChain=GetCheckBox(hwnd,ID_WALK);
					opCheckRate=GetCheckBox(hwnd,ID_CHECKRATE);
					opCheckChannels=GetCheckBox(hwnd,ID_CHECKCHANNELS);
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
    if (!ReadHeader(f,&ibRate,&ibChannels,&ibBits,&ibType,&ibCompression,&nsamp,&datastart))
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
		ibTime=MulDiv(1000,nsamp,ibRate);
    DialogBox(Plugin.hDllInst,"InfoBox",hwnd,InfoBoxDlgProc);
}

void Walk_IMA_ADPCM(FSHandle* f, DWORD count)
{
	DWORD read,i;
	BYTE  Code;

	while (count>0)
	{
		Plugin.fsh->ReadFile(f,LPData(f)->buffer,min(count,min(LPData(f)->blockRemained,BUFFERSIZE)),&read);
		LPData(f)->blockRemained-=read;
		count-=read;
		if (read>0)
		{
			for (i=0;i<read;i++)
			{
				Code=NIBBLE(LPData(f)->buffer[i],1);
				StepADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
				if (opUseClipping)
					Clip16BitSample(&(LPData(f)->Cur_Sample1));
				Code=NIBBLE(LPData(f)->buffer[i],0);
				if (LPData(f)->channels==2)
				{
					StepADPCM(Code,&(LPData(f)->Index2),&(LPData(f)->Cur_Sample2));
					if (opUseClipping)
						Clip16BitSample(&(LPData(f)->Cur_Sample2));
				}
				else
				{
					StepADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
					if (opUseClipping)
						Clip16BitSample(&(LPData(f)->Cur_Sample1));
				}
			}
		}
	}
}

void SeekToPos(FSHandle* f, DWORD pos)
{
    DWORD datapos,read,datasize;
	ASFBlockNode *walker;
	ASFBlockHeader asfblockhdr;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

    datapos=MulDiv(pos,LPData(f)->align,LPData(f)->factor);
	switch (LPData(f)->type)
	{
		case EACS_MULTIBLOCK:
			if (datapos==0)
			{
				Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
				LPData(f)->curnode=LPData(f)->chain;
				LPData(f)->blockRemained=0L;
				return;
			}
			if (LPData(f)->chain!=NULL)
			{
				walker=LPData(f)->chain;
				while ((walker!=NULL) && (walker->datasize<=datapos))
				{
					datapos-=walker->datasize;
					walker=walker->next;
				}
				if (walker==NULL)
					return;
				LPData(f)->curnode=walker;
				switch (walker->type)
				{
					case ASF_BT_HEADER:
					case ASF_BT_DATA:
						Plugin.fsh->SetFilePointer(f,walker->start,FILE_BEGIN);
						break;
					default: // ???
						return;
				}
				datasize=walker->datasize;
			}
			else
			{
				Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
				datasize=0L;
				lstrcpyn(asfblockhdr.id,"\0\0\0",4);
				asfblockhdr.size=0L;
				while (!(Plugin.fsh->EndOfFile(f)))
				{
					Plugin.fsh->ReadFile(f,&asfblockhdr,sizeof(ASFBlockHeader),&read);
					if (read<sizeof(ASFBlockHeader))
						break; // ???
					datasize=asfblockhdr.size-sizeof(ASFBlockHeader);
					if (!memcmp(asfblockhdr.id,IDSTR_1SNh,4))
						datasize-=sizeof(EACSHeader)+((LPData(f)->compression==EACS_IMA_ADPCM)?((LPData(f)->channels==2)?20:12):0);
					else if (!memcmp(asfblockhdr.id,IDSTR_1SNd,4))
						datasize-=((LPData(f)->compression==EACS_IMA_ADPCM)?((LPData(f)->channels==2)?20:12):0);
					else
						datasize=0L;
					if (
						(!memcmp(asfblockhdr.id,IDSTR_1SNe,4)) ||
						(datasize>datapos)
					   )
						break;
					datapos-=datasize;
					Plugin.fsh->SetFilePointer(f,asfblockhdr.size-sizeof(ASFBlockHeader),FILE_CURRENT);
				}
				if (datasize<=datapos)
					return;
				if (!memcmp(asfblockhdr.id,IDSTR_1SNh,4))
					Plugin.fsh->SetFilePointer(f,sizeof(EACSHeader),FILE_CURRENT);
			}
			switch (LPData(f)->compression)
			{
				case EACS_IMA_ADPCM:
					Plugin.fsh->ReadFile(f,&(LPData(f)->blockRemained),4,&read);
					if (read<4)
						LPData(f)->blockRemained=0L; // ???
					Plugin.fsh->ReadFile(f,&(LPData(f)->Index1),4,&read);
					if (read<4)
						LPData(f)->Index1=0L; // ???
					ClipIndex(&(LPData(f)->Index1));
					if (LPData(f)->channels==2)
					{
						Plugin.fsh->ReadFile(f,&(LPData(f)->Index2),4,&read);
						if (read<4)
							LPData(f)->Index2=0L; // ???
						ClipIndex(&(LPData(f)->Index2));
					}
					Plugin.fsh->ReadFile(f,&(LPData(f)->Cur_Sample1),4,&read);
					if (read<4)
						LPData(f)->Cur_Sample1=0L; // ???
					if (LPData(f)->channels==2)
					{
						Plugin.fsh->ReadFile(f,&(LPData(f)->Cur_Sample2),4,&read);
						if (read<4)
							LPData(f)->Cur_Sample2=0L; // ???
					}
					Walk_IMA_ADPCM(f,datapos);
					break;
				case EACS_PCM:
					Plugin.fsh->SetFilePointer(f,CORRALIGN(datapos,LPData(f)->align),FILE_CURRENT);
					LPData(f)->blockRemained=CORRALIGN(datasize-datapos,LPData(f)->align);
					break;
				default: // ???
					break;
			}
			break;
		case EACS_SINGLEBLOCK:
			switch (LPData(f)->compression)
			{
				case EACS_IMA_ADPCM:
					Plugin.fsh->SetFilePointer(f,LPData(f)->datastart,FILE_BEGIN);
					LPData(f)->blockRemained=LPData(f)->datasize;
					LPData(f)->Index1=0;
					LPData(f)->Index2=0;
					LPData(f)->Cur_Sample1=0;
					LPData(f)->Cur_Sample2=0;
					Walk_IMA_ADPCM(f,datapos);
					break;
				case EACS_PCM:
					Plugin.fsh->SetFilePointer(f,LPData(f)->datastart+CORRALIGN(datapos,LPData(f)->align),FILE_BEGIN);
					LPData(f)->blockRemained=CORRALIGN(LPData(f)->datasize-datapos,LPData(f)->align);
					break;
				default: // ???
					break;
			}
			break;
		default: // ???
			break;
	}
}

void __stdcall Seek(FSHandle* f, DWORD pos)
{
	LPData(f)->curTime=pos;
	SeekToPos(f,MulDiv(pos,LPData(f)->rate,1000));
}

SearchPattern patterns[]=
{
	{4,IDSTR_EACS}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX ASF Audio File Decoder",
    "v0.98",
    "Electronic Arts Audio Files (*.ASF;*.AS4;*.SPH)\0*.asf;*.as4;*.sph\0"
	"Electronic Arts Sound Files (*.EAS)\0*.eas\0"
	"Electronic Arts Video Files (*.TGV)\0*.tgv\0",
    "ASF",
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
