/*
 * MGI plug-in source code
 * (Origin music: Wing Commander: Prophecy)
 *
 * Copyright (C) 2001 ANX Software
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

#include "AF_MGI.h"
#include "resource.h"

typedef struct tagDecState_EA_ADPCM
{
	long lPrevSample,lCurSample,lPrevCoeff,lCurCoeff;
	BYTE bShift;
} DecState_EA_ADPCM;

typedef struct tagSecEntry
{
	DWORD dwStart,dwSize,dwTail,dwOutSize;
} SecEntry;

typedef struct tagData
{
	WORD	  channels,bits,align;
	DWORD	  rate,blocksize,iSection,nSections,secpos;
	char	 *buffer;
	SecEntry *SecMap;
	DecState_EA_ADPCM dsLeft,dsRight;
} Data;

#define LPData(f) ((Data*)((f)->afData))

#define BUFFERBLOCKS (4000ul)

AFPlugin Plugin;

BOOL opCheckUnknown1,
	 opCheckUnknown2,
	 opCheckSecNum,
	 opCheckDataStart,
	 opCheckSecIndex,
	 opCheckLastSec,
	 opStorePos,
	 opCheckThoroughly,
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
		opCheckUnknown1=FALSE;
		opCheckUnknown2=FALSE;
		opCheckSecNum=TRUE;
		opCheckDataStart=TRUE;
		opCheckSecIndex=TRUE;
		opCheckLastSec=TRUE;
		opStorePos=TRUE;
		opCheckThoroughly=TRUE;
		opUseClipping=TRUE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"CheckUnknown1","off",str,40,Plugin.szINIFileName);
    opCheckUnknown1=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckUnknown2","off",str,40,Plugin.szINIFileName);
    opCheckUnknown2=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckSecNum","on",str,40,Plugin.szINIFileName);
    opCheckSecNum=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckDataStart","on",str,40,Plugin.szINIFileName);
    opCheckDataStart=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckSecIndices","on",str,40,Plugin.szINIFileName);
    opCheckSecIndex=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckLastSec","on",str,40,Plugin.szINIFileName);
    opCheckLastSec=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"StorePos","on",str,40,Plugin.szINIFileName);
    opStorePos=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckThoroughly","on",str,40,Plugin.szINIFileName);
    opCheckThoroughly=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"CheckUnknown1",SaveOptionBool(opCheckUnknown1),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckUnknown2",SaveOptionBool(opCheckUnknown2),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckSecNum",SaveOptionBool(opCheckSecNum),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckDataStart",SaveOptionBool(opCheckDataStart),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckSecIndices",SaveOptionBool(opCheckSecIndex),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckLastSec",SaveOptionBool(opCheckLastSec),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"StorePos",SaveOptionBool(opStorePos),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckThoroughly",SaveOptionBool(opCheckThoroughly),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
}

BOOL ReadMGIHeader
(
	FSHandle  *file,
	DWORD	  *rate,
	WORD	  *channels,
	WORD	  *bits,
	DWORD	  *pos,
	DWORD	  *nsec,
	SecEntry* *secmap
)
{
	MGIHeader   hdr;
	MGIIntDesc  intdesc;
	MGISecDesc *sdMap;
	DWORD	    i,read,maxIndex;

	if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&hdr,sizeof(MGIHeader),&read);
	if (
		(read<sizeof(MGIHeader)) ||
		(memcmp(hdr.szID,IDSTR_MGI,4)) ||
		((opCheckUnknown1) && (hdr.dwUnknown1!=0L)) ||
		((opCheckUnknown2) && (hdr.dwUnknown2!=0L))
	   )
		return FALSE; // ???

	if (*pos==0L)
	{
		maxIndex=0;
		while (!(Plugin.fsh->EndOfFile(file)))
		{
			Plugin.fsh->ReadFile(file,&intdesc,sizeof(MGIIntDesc),&read);
			if (read<sizeof(MGIIntDesc))
				return FALSE; // ???
			if ( ((LONG)intdesc.dwIndex<=0) || (intdesc.dwSection==0L) )
				continue;
			if (
				(intdesc.dwIndex>=hdr.dwNumIntIndices) ||
				(intdesc.dwIndex>=hdr.dwNumSecIndices)
			   )
			{
				if (
					( (!opCheckSecNum) || (intdesc.dwIndex>maxIndex) ) &&
					( (!opCheckDataStart) || (Plugin.fsh->GetFilePointer(file)-4+intdesc.dwIndex*sizeof(MGISecDesc)==intdesc.dwSection) )
				   )
				{
					Plugin.fsh->SetFilePointer(file,-(LONG)sizeof(MGIIntDesc),FILE_CURRENT);
					*pos=Plugin.fsh->GetFilePointer(file);
					break;
				}
				else if (intdesc.dwIndex>=hdr.dwNumIntIndices)
					return FALSE;
			}
			if (maxIndex<intdesc.dwSection)
				maxIndex=intdesc.dwSection;
		}
	}
	else
		Plugin.fsh->SetFilePointer(file,*pos,FILE_BEGIN);

	Plugin.fsh->ReadFile(file,nsec,sizeof(DWORD),&read);

	if (secmap!=NULL)
	{
		*secmap=(SecEntry*)GlobalAlloc(GPTR,(*nsec)*sizeof(SecEntry));
		if (*secmap==NULL)
			return TRUE;
		sdMap=(MGISecDesc*)GlobalAlloc(GPTR,(*nsec)*sizeof(MGISecDesc));
		if (sdMap==NULL)
		{
			GlobalFree(*secmap);
			*secmap=NULL;
			return TRUE;
		}
		Plugin.fsh->ReadFile(file,sdMap,(*nsec)*sizeof(MGISecDesc),&read);
		if (read<(*nsec)*sizeof(MGISecDesc))
		{
			GlobalFree(sdMap);
			GlobalFree(*secmap);
			*secmap=NULL;
			return TRUE;
		}
		if (
			(opCheckLastSec) &&
			(
			 (sdMap[(*nsec)-1].dwIndex!=0L) ||
			 (sdMap[(*nsec)-1].dwOutSize!=0L)
			)
		   )
		{
			GlobalFree(sdMap);
			GlobalFree(*secmap);
			*secmap=NULL;
			return FALSE;
		}
		for (i=0;i<(*nsec)-1;i++)
		{
			if ( (opCheckSecIndex) && (sdMap[i].dwIndex>=hdr.dwNumSecIndices) )
			{
				GlobalFree(sdMap);
				GlobalFree(*secmap);
				*secmap=NULL;
				return FALSE;
			}
			(*secmap)[i].dwStart=sdMap[i].dwStart;
			(*secmap)[i].dwSize=sdMap[i+1].dwStart-sdMap[i].dwStart;
			(*secmap)[i].dwTail=((*secmap)[i].dwSize*0x70-sdMap[i].dwOutSize*0x1E)/0x52;
			(*secmap)[i].dwSize-=(*secmap)[i].dwTail;
			(*secmap)[i].dwOutSize=sdMap[i].dwOutSize;
		}
		(*secmap)[i].dwStart=sdMap[i].dwStart;
		(*secmap)[i].dwSize=Plugin.fsh->GetFileSize(file)-sdMap[i].dwStart;
		(*secmap)[i].dwTail=((*secmap)[i].dwSize*0x70-sdMap[i].dwOutSize*0x1E)/0x52;
		(*secmap)[i].dwSize-=(*secmap)[i].dwTail;
		(*secmap)[i].dwOutSize=sdMap[i].dwOutSize;
		GlobalFree(sdMap);
	}

	*rate=22050; // ???
	*channels=2; // ???
	*bits=16;    // ???

	return TRUE;
}

void Init_EA_ADPCM(DecState_EA_ADPCM *pDS, long curSample, long prevSample)
{
	pDS->lCurSample=curSample;
	pDS->lPrevSample=prevSample;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	SecEntry *table=NULL;
	char	 *end;
	DWORD	  pos,nsec;

	if (f==NULL)
		return FALSE;

	pos=(f->node!=NULL) ? strtoul(f->node->afDataString,&end,0x10) : 0L;

    if (!ReadMGIHeader(f,rate,channels,bits,&pos,&nsec,&table))
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
	LPData(f)->iSection=0L;
	LPData(f)->secpos=0L;
	LPData(f)->nSections=nsec;
	LPData(f)->SecMap=table;
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
			(GlobalFree(LPData(f)->SecMap)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    AFile*	   node;
	FSHandle   fs;
	SecEntry  *table=NULL;
	MGISecDesc secdesc;
	WORD	   channels,bits;
	DWORD	   read,rate,stpos=0L,nsec,size;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // MGI ID
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying MGI file...");
			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			fs.rf=f;
			fs.start=pos;
			fs.length=RFPLUGIN(f)->GetFileSize(f)-pos;
			fs.node=NULL;
			if (!ReadMGIHeader(&fs,&rate,&channels,&bits,&stpos,&nsec,(opCheckThoroughly)?(&table):NULL))
				return NULL;
			if (table!=NULL)
			{
				size=table[nsec-1].dwStart;
				GlobalFree(table);
			}
			else
			{
				RFPLUGIN(f)->SetFilePointer(f,stpos+sizeof(DWORD)+(nsec-1)*sizeof(MGISecDesc),FILE_BEGIN);
				RFPLUGIN(f)->ReadFile(f,&secdesc,sizeof(MGISecDesc),&read);
				size=secdesc.dwStart;
			}
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"MGI file correct.");
			break;
		default: // ???
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=size;
	*newpos=node->fsStart+node->fsLength;
    wsprintf(node->afDataString,(opStorePos)?"%lX":"",stpos);
	lstrcpy(node->afName,"");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile*     node;
	SecEntry  *table=NULL;
	WORD	   channels,bits;
	DWORD	   rate,stpos=0L,nsec;

    if (f==NULL)
		return NULL;

    if (!ReadMGIHeader(f,&rate,&channels,&bits,&stpos,&nsec,(opCheckThoroughly)?(&table):NULL))
		return NULL;
	GlobalFree(table);
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    wsprintf(node->afDataString,(opStorePos)?"%lX":"",stpos);
	lstrcpy(node->afName,"");

    return node;
}

DWORD __stdcall GetTime(AFile* node)
{
    FSHandle*  f;
	SecEntry  *table=NULL;
	char	  *end;
	WORD	   channels,bits;
	DWORD	   rate,stpos=0L,i,nsec,outsize;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
	stpos=strtoul(node->afDataString,&end,0x10);
    if (!ReadMGIHeader(f,&rate,&channels,&bits,&stpos,&nsec,&table))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	Plugin.fsh->CloseFile(f);

	if (table!=NULL)
	{
		outsize=0L;
		for (i=0;i<nsec;i++)
			outsize+=table[i].dwOutSize;
		GlobalFree(table);
	}
	else
		return -1;

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
			Step_EA_ADPCM(&(LPData(f)->dsLeft),(BYTE)HINIBBLE(chunk[i]));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->dsLeft.lCurSample));
			buff[2*i]=(short)(LPData(f)->dsLeft.lCurSample);
			Step_EA_ADPCM(&(LPData(f)->dsRight),(BYTE)LONIBBLE(chunk[i]));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->dsRight.lCurSample));
			buff[2*i+1]=(short)(LPData(f)->dsRight.lCurSample);
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

	while ( LPData(file)->iSection < LPData(file)->nSections )
	{
		if ( LPData(file)->secpos < LPData(file)->SecMap[LPData(file)->iSection].dwSize )
		{
			toRead=min(CORRALIGN(buffsize/4,LPData(file)->blocksize),min(LPData(file)->blocksize*BUFFERBLOCKS,(LPData(file)->SecMap[LPData(file)->iSection].dwSize)-(LPData(file)->secpos)));
			Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
			if (read==0)
				break;
			LPData(file)->secpos+=read;
			for (i=0;i<read;)
			{
				if (LPData(file)->channels==2)
				{
					BlockInit_EA_ADPCM
					(
						&(LPData(file)->dsLeft),
						(BYTE)HINIBBLE(LPData(file)->buffer[i]),
						(BYTE)HINIBBLE(LPData(file)->buffer[i+1])
					);
					BlockInit_EA_ADPCM
					(
						&(LPData(file)->dsRight),
						(BYTE)LONIBBLE(LPData(file)->buffer[i]),
						(BYTE)LONIBBLE(LPData(file)->buffer[i+1])
					);
					i+=2;
				}
				else
				{
					BlockInit_EA_ADPCM
					(
						&(LPData(file)->dsLeft),
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
		}
		else if ( LPData(file)->secpos < LPData(file)->SecMap[LPData(file)->iSection].dwSize + LPData(file)->SecMap[LPData(file)->iSection].dwTail )
		{
			toRead=min(CORRALIGN(buffsize,LPData(file)->align),(LPData(file)->SecMap[LPData(file)->iSection].dwSize)+(LPData(file)->SecMap[LPData(file)->iSection].dwTail)-(LPData(file)->secpos));
			Plugin.fsh->ReadFile(file,buffer+pcmSize,toRead,&read);
			if (read==0)
				break;
			LPData(file)->secpos+=read;
			pcmSize+=read;
			buffsize-=read;
		}
		else
		{
			LPData(file)->iSection++;
			LPData(file)->secpos=0L;
			Init_EA_ADPCM(&(LPData(file)->dsLeft),0L,0L);
			Init_EA_ADPCM(&(LPData(file)->dsRight),0L,0L);
			if ( LPData(file)->iSection < LPData(file)->nSections )
				Plugin.fsh->SetFilePointer(file,LPData(file)->SecMap[LPData(file)->iSection].dwStart,FILE_BEGIN);
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
DWORD  ibSize,ibRate,ibTime,ibSecNum;
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
			wsprintf(str,"Compression: EA ADPCM\r\n"
						 "Sections Amount: %lu\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 ibSecNum,
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
			SetCheckBox(hwnd,ID_CHECKUNKNOWN1,opCheckUnknown1);
			SetCheckBox(hwnd,ID_CHECKUNKNOWN2,opCheckUnknown2);
			SetCheckBox(hwnd,ID_CHECKSECNUM,opCheckSecNum);
			SetCheckBox(hwnd,ID_CHECKDATASTART,opCheckDataStart);
			SetCheckBox(hwnd,ID_CHECKSECINDEX,opCheckSecIndex);
			SetCheckBox(hwnd,ID_CHECKLASTSEC,opCheckLastSec);
			SetCheckBox(hwnd,ID_STOREPOS,opStorePos);
			SetCheckBox(hwnd,ID_CHECKTHOROUGHLY,opCheckThoroughly);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_CHECKUNKNOWN1,FALSE);
					SetCheckBox(hwnd,ID_CHECKUNKNOWN2,FALSE);
					SetCheckBox(hwnd,ID_CHECKSECNUM,TRUE);
					SetCheckBox(hwnd,ID_CHECKDATASTART,TRUE);
					SetCheckBox(hwnd,ID_CHECKSECINDEX,TRUE);
					SetCheckBox(hwnd,ID_CHECKLASTSEC,TRUE);
					SetCheckBox(hwnd,ID_STOREPOS,TRUE);
					SetCheckBox(hwnd,ID_CHECKTHOROUGHLY,TRUE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opCheckUnknown1=GetCheckBox(hwnd,ID_CHECKUNKNOWN1);
					opCheckUnknown2=GetCheckBox(hwnd,ID_CHECKUNKNOWN2);
					opCheckSecNum=GetCheckBox(hwnd,ID_CHECKSECNUM);
					opCheckDataStart=GetCheckBox(hwnd,ID_CHECKDATASTART);
					opCheckSecIndex=GetCheckBox(hwnd,ID_CHECKSECINDEX);
					opCheckLastSec=GetCheckBox(hwnd,ID_CHECKLASTSEC);
					opStorePos=GetCheckBox(hwnd,ID_STOREPOS);
					opCheckThoroughly=GetCheckBox(hwnd,ID_CHECKTHOROUGHLY);
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
    char  str[256],*end;
	DWORD stpos;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
	stpos=strtoul(node->afDataString,&end,0x10);
    if (!ReadMGIHeader(f,&ibRate,&ibChannels,&ibBits,&stpos,&ibSecNum,NULL))
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
	LPData(f)->iSection=0l;
	while (
			( LPData(f)->iSection < LPData(f)->nSections ) &&
			( LPData(f)->SecMap[LPData(f)->iSection].dwOutSize <= filepos )
		  )
	{
		filepos-=LPData(f)->SecMap[LPData(f)->iSection].dwOutSize;
		LPData(f)->iSection++;
	}
	LPData(f)->secpos=0L;
	Init_EA_ADPCM(&(LPData(f)->dsLeft),0L,0L);
	Init_EA_ADPCM(&(LPData(f)->dsRight),0L,0L);
	if ( LPData(f)->iSection < LPData(f)->nSections )
		Plugin.fsh->SetFilePointer(f,LPData(f)->SecMap[LPData(f)->iSection].dwStart,FILE_BEGIN);
}

SearchPattern patterns[]=
{
	{4,IDSTR_MGI},
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX MGI Audio File Decoder",
    "v0.80",
	"Origin Audio Files (*.MGI)\0*.mgi\0",
    "MGI",
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
