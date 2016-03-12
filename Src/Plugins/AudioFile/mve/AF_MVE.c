/*
 * MVE plug-in source code
 * (Interplay video soundtracks)
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * The plug-in is based on the MVE2PCM program written by 
 * Dmitry Kirnocenskij (ejt@mail.ru)
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
#include <commctrl.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_MVE.h"
#include "resource.h"

#define WM_GAP_WAITINGTOY (WM_USER+1)

typedef struct tagData
{
	BOOL  bCompressed;
    WORD  align,channels,bits;
    DWORD rate,curBlockStart,curBlockSize,dataLeft;
	SHORT curSample1,curSample2;
	char *buffer;
} Data;

#define BUFFERSIZE (64000ul)
#define NO_BLOCK   (-1)

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

BOOL opNoWalk;

SHORT DPCM[0x100]=
{
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
	0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002F, 0x0033, 0x0038, 0x003D,
	0x0042, 0x0048, 0x004F, 0x0056, 0x005E, 0x0066, 0x0070, 0x007A, 0x0085, 0x0091, 0x009E, 0x00AD, 0x00BD, 0x00CE, 0x00E1, 0x00F5,
	0x010B, 0x0124, 0x013E, 0x015C, 0x017B, 0x019E, 0x01C4, 0x01ED, 0x021A, 0x024B, 0x0280, 0x02BB, 0x02FB, 0x0340, 0x038C, 0x03DF,
	0x0439, 0x049C, 0x0508, 0x057D, 0x05FE, 0x0689, 0x0722, 0x07C9, 0x087F, 0x0945, 0x0A1E, 0x0B0A, 0x0C0C, 0x0D25, 0x0E58, 0x0FA8,
	0x1115, 0x12A4, 0x1458, 0x1633, 0x183A, 0x1A6F, 0x1CD9, 0x1F7B, 0x225A, 0x257D, 0x28E8, 0x2CA4, 0x30B7, 0x3529, 0x3A03, 0x3F4E,
	0x4515, 0x4B62, 0x5244, 0x59C5, 0x61F6, 0x6AE7, 0x74A8, 0x7F4D, 0x8AEB, 0x9798, 0xA56E, 0xB486, 0xC4FF, 0xD6F9, 0xEA97, 0xFFFF,
	0x0001, 0x0001, 0x1569, 0x2907, 0x3B01, 0x4B7A, 0x5A92, 0x6868, 0x7515, 0x80B3, 0x8B58, 0x9519, 0x9E0A, 0xA63B, 0xADBC, 0xB49E,
	0xBAEB, 0xC0B2, 0xC5FD, 0xCAD7, 0xCF49, 0xD35C, 0xD718, 0xDA83, 0xDDA6, 0xE085, 0xE327, 0xE591, 0xE7C6, 0xE9CD, 0xEBA8, 0xED5C,
	0xEEEB, 0xF058, 0xF1A8, 0xF2DB, 0xF3F4, 0xF4F6, 0xF5E2, 0xF6BB, 0xF781, 0xF837, 0xF8DE, 0xF977, 0xFA02, 0xFA83, 0xFAF8, 0xFB64,
	0xFBC7, 0xFC21, 0xFC74, 0xFCC0, 0xFD05, 0xFD45, 0xFD80, 0xFDB5, 0xFDE6, 0xFE13, 0xFE3C, 0xFE62, 0xFE85, 0xFEA4, 0xFEC2, 0xFEDC,
	0xFEF5, 0xFF0B, 0xFF1F, 0xFF32, 0xFF43, 0xFF53, 0xFF62, 0xFF6F, 0xFF7B, 0xFF86, 0xFF90, 0xFF9A, 0xFFA2, 0xFFAA, 0xFFB1, 0xFFB8,
	0xFFBE, 0xFFC3, 0xFFC8, 0xFFCD, 0xFFD1, 0xFFD5, 0xFFD6, 0xFFD7, 0xFFD8, 0xFFD9, 0xFFDA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE, 0xFFDF,
	0xFFE0, 0xFFE1, 0xFFE2, 0xFFE3, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7, 0xFFE8, 0xFFE9, 0xFFEA, 0xFFEB, 0xFFEC, 0xFFED, 0xFFEE, 0xFFEF,
	0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0xFFF6, 0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF
};

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

	if (Plugin.szINIFileName==NULL)
    {
		opNoWalk=TRUE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"NoWalk","on",str,40,Plugin.szINIFileName);
    opNoWalk=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"NoWalk",SaveOptionBool(opNoWalk),Plugin.szINIFileName);
}

DWORD FindBlock(FSHandle* f, BYTE blockid)
{
	BOOL  found;
	DWORD read;
	MVEBlockHeader blockhdr;

	found=FALSE;
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		Plugin.fsh->ReadFile(f,&blockhdr,sizeof(MVEBlockHeader),&read);
		if (read<sizeof(MVEBlockHeader))
			break;
		found=(BOOL)(blockhdr.type==blockid);
		if (found)
			break;
		Plugin.fsh->SetFilePointer(f,(DWORD)blockhdr.size,FILE_CURRENT);
	}

	return (DWORD)((found)?(blockhdr.size):NO_BLOCK);
}

DWORD FindSubBlock(FSHandle* f, DWORD blockstart, DWORD blocksize, BYTE subblockid)
{
	BOOL  found;
	DWORD read;
	MVEBlockHeader blockhdr;

	found=FALSE;
	while (
		   (!(Plugin.fsh->EndOfFile(f))) &&
		   (blockstart+blocksize>Plugin.fsh->GetFilePointer(f))
		  )
	{
		Plugin.fsh->ReadFile(f,&blockhdr,sizeof(MVEBlockHeader),&read);
		if (read<sizeof(MVEBlockHeader))
			break;
		found=(BOOL)(blockhdr.type==subblockid);
		if (found)
			break;
		Plugin.fsh->SetFilePointer(f,(DWORD)blockhdr.size,FILE_CURRENT);
	}

	return (DWORD)((found)?(blockhdr.size):NO_BLOCK);
}

BOOL GetSoundInfo(
	FSHandle* file,
	DWORD* rate,
	WORD* channels,
	WORD* bits,
	BOOL* bCompressed
)
{
    DWORD read,start,size;
    MVEHeader mvehdr;
	MVESoundInfo mvesndinfo;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&mvehdr,sizeof(MVEHeader),&read);
	if (read<sizeof(MVEHeader))
		return FALSE;
    if (
		(memcmp(mvehdr.szID,IDSTR_MVE,20)) ||
		(mvehdr.magic1!=MVE_HEADER_MAGIC1) ||
		(mvehdr.magic2!=MVE_HEADER_MAGIC2) ||
		(((DWORD)(~(mvehdr.magic2))-(DWORD)mvehdr.magic3)!=MVE_HEADER_MAGIC3)
	   )
		return FALSE;

	size=FindBlock(file,MVE_BT_SOUNDINFO);
	if (size==NO_BLOCK)
		return FALSE;
	start=Plugin.fsh->GetFilePointer(file);
	if (FindSubBlock(file,start,size,MVE_SBT_SOUNDINFO)==NO_BLOCK)
		return FALSE;
	Plugin.fsh->ReadFile(file,&mvesndinfo,sizeof(MVESoundInfo),&read);
	if (read<sizeof(MVESoundInfo))
		return FALSE;
    *rate=(DWORD)mvesndinfo.rate;
    *channels=(mvesndinfo.flags & MVE_SI_STEREO)?2:1;
	*bits=(mvesndinfo.flags & MVE_SI_16BIT)?16:8;
	*bCompressed=(BOOL)(mvesndinfo.flags & MVE_SI_COMPRESSED);

    return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	BOOL bCompressed;

    if (f==NULL)
		return FALSE;

    if (!GetSoundInfo(f,rate,channels,bits,&bCompressed))
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
	LPData(f)->align=(*channels)*(*bits/8);
	LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
    LPData(f)->rate=*rate;
	LPData(f)->bCompressed=bCompressed;
	LPData(f)->curSample1=0L;
	LPData(f)->curSample2=0L;
	if (bCompressed)
	{
		LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
		if (LPData(f)->buffer==NULL)
		{
			GlobalFree(f->afData);
			SetLastError(PRIVEC(IDS_NOBUFFER));
			return FALSE;
		}
	}
	LPData(f)->curBlockStart=0L;
	LPData(f)->curBlockSize=NO_BLOCK;
	LPData(f)->dataLeft=0L;
	Plugin.fsh->SetFilePointer(f,sizeof(MVEHeader),FILE_BEGIN);

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

DWORD Decode_MVE_ADPCM(FSHandle* f, char* chunk, DWORD size, char* buff)
{
    DWORD  i;
	BOOL   left;
    SHORT *dstpcm16;
	BYTE   code;

    if (f==NULL)
		return 0;

	dstpcm16=(SHORT*)buff;
	left=TRUE;
	for (i=0;i<size;i++)
	{
		code=chunk[i];
		if (left)
		{
			LPData(f)->curSample1+=DPCM[code];
			*dstpcm16++=LPData(f)->curSample1;
		}
		else
		{
			LPData(f)->curSample2+=DPCM[code];
			*dstpcm16++=LPData(f)->curSample2;
		}
		if (LPData(f)->channels==2)
			left=!left;
	}

    return 2*size;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate;
    WORD   channels,bits;
	BOOL   bCompressed;

    if (f==NULL)
		return NULL;

    if (!GetSoundInfo(f,&rate,&channels,&bits,&bCompressed))
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
}

DWORD FindSoundBlock(FSHandle* f)
{
	BOOL  found;
	DWORD read;
	MVEBlockHeader blockhdr;

	if (f==NULL)
		return NO_BLOCK;

	found=FALSE;
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		Plugin.fsh->ReadFile(f,&blockhdr,sizeof(MVEBlockHeader),&read);
		if (read<sizeof(MVEBlockHeader))
			break;
		found=(BOOL)((blockhdr.type==MVE_BT_SOUNDFRAME) || (blockhdr.type==MVE_BT_IMAGEFRAME));
		if (found)
			break;
		Plugin.fsh->SetFilePointer(f,(DWORD)blockhdr.size,FILE_CURRENT);
	}

	return (DWORD)((found)?(blockhdr.size):NO_BLOCK);
}

DWORD FindSound(FSHandle* file)
{
	DWORD size;

	if (file==NULL)
		return NO_BLOCK;
    if (LPData(file)==NULL)
		return NO_BLOCK;
    if (Plugin.fsh->EndOfFile(file))
		return NO_BLOCK;

	while (1)
	{
		if (LPData(file)->curBlockSize==NO_BLOCK)
		{
			LPData(file)->curBlockSize=FindSoundBlock(file);
			if (LPData(file)->curBlockSize==NO_BLOCK)
				break;
			LPData(file)->curBlockStart=Plugin.fsh->GetFilePointer(file);
		}
		else
		{
			size=FindSubBlock(file,LPData(file)->curBlockStart,LPData(file)->curBlockSize,MVE_SBT_SOUNDDATA);
			if (size!=NO_BLOCK)
				return size;
			else
				LPData(file)->curBlockSize=NO_BLOCK;
		}
	}

	return NO_BLOCK;
}

#define PTR_SHORT(p) (*((SHORT*)(p)))

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read,toRead,pcmSize;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	pcmSize=0L;
	if (LPData(file)->bCompressed)
	{
		toRead=min(buffsize/2,BUFFERSIZE);
		if (LPData(file)->align/2>0)
			toRead=CORRALIGN(toRead,LPData(file)->align/2);
	}
	else
		toRead=CORRALIGN(buffsize,LPData(file)->align);
	while (
		   (!(Plugin.fsh->EndOfFile(file))) && 
		   (toRead>0)
		  )
	{
		if (LPData(file)->dataLeft==0)
		{
			if (
				(LPData(file)->bCompressed) && 
				(2*toRead<LPData(file)->align)
			   )
				break;
			LPData(file)->dataLeft=FindSound(file);
			if (LPData(file)->dataLeft==NO_BLOCK)
				break;
			Plugin.fsh->SetFilePointer(file,sizeof(MVESoundHeader),FILE_CURRENT);
			LPData(file)->dataLeft-=sizeof(MVESoundHeader);
			if (LPData(file)->bCompressed)
			{
				Plugin.fsh->ReadFile(file,&(LPData(file)->curSample1),sizeof(SHORT),&read);
				PTR_SHORT(buffer+pcmSize)=LPData(file)->curSample1;
				pcmSize+=2;
				toRead--;
				if (LPData(file)->channels==2)
				{
					Plugin.fsh->ReadFile(file,&(LPData(file)->curSample2),sizeof(SHORT),&read);
					PTR_SHORT(buffer+pcmSize)=LPData(file)->curSample2;
					pcmSize+=2;
					toRead--;
				}
				LPData(file)->dataLeft-=LPData(file)->align;
			}
		}
		if (LPData(file)->bCompressed)
		{
			Plugin.fsh->ReadFile(file,LPData(file)->buffer,min(toRead,LPData(file)->dataLeft),&read);
			pcmSize+=Decode_MVE_ADPCM(file,LPData(file)->buffer,read,buffer+pcmSize);
		}
		else
		{
			Plugin.fsh->ReadFile(file,buffer+pcmSize,min(toRead,LPData(file)->dataLeft),&read);
			pcmSize+=read;
		}
		LPData(file)->dataLeft-=read;
		toRead-=read;
		if (read==0)
			break; // ???
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
BOOL   ibCompressed;

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
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (ibCompressed)?"MVE ADPCM":"None",
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

DWORD __stdcall GetTime(AFile* node)
{
	FSHandle* f;
	MVESoundHeader sndhdr;
    DWORD rate,time,size,read;
	BOOL  compressed;
    WORD  channels,bits;
	HWND  pwnd,hwnd;

	if (node==NULL)
		return -1;
	if (opNoWalk)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

	if (!GetSoundInfo(f,&rate,&channels,&bits,&compressed))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
	}
	f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	LPData(f)->curBlockStart=0L;
	LPData(f)->curBlockSize=NO_BLOCK;
	Plugin.fsh->SetFilePointer(f,sizeof(MVEHeader),FILE_BEGIN);

	hwnd=GetFocus();
	pwnd=CreateDialog(Plugin.hDllInst,"Walking",Plugin.hMainWindow,(DLGPROC)WalkingDlgProc);
	SetWindowText(pwnd,"Calculating time");
	SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETRANGE,0,MAKELPARAM(0,100));
    SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,0,0L);
	time=0;
	while ((size=FindSound(f))!=NO_BLOCK)
	{
		Plugin.fsh->ReadFile(f,&sndhdr,sizeof(MVESoundHeader),&read);
		if (read<sizeof(MVESoundHeader))
			break; // ???
		Plugin.fsh->SetFilePointer(f,size-read,FILE_CURRENT);
		SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,MulDiv(100,Plugin.fsh->GetFilePointer(f),Plugin.fsh->GetFileSize(f)),0L);
		SendMessage(pwnd,WM_GAP_WAITINGTOY,0,0);
		time+=MulDiv(8000,(DWORD)sndhdr.outSize,channels*rate*bits);
	}
	SetFocus(hwnd);
    DestroyWindow(pwnd);
	UpdateWindow(hwnd);
	GlobalFree(f->afData);
	Plugin.fsh->CloseFile(f);

	return time;
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
    if (!GetSoundInfo(f,&ibRate,&ibChannels,&ibBits,&ibCompressed))
    {
		wsprintf(str,"This is not %s file or no soundtrack present.",Plugin.afID);
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

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    MVEHeader mvehdr;
	MVEBlockHeader blockhdr;
    DWORD     read;
    AFile*    node;

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying MVE file...");
    RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
    RFPLUGIN(f)->ReadFile(f,&mvehdr,sizeof(MVEHeader),&read);
	if (read<sizeof(MVEHeader))
		return NULL;
    if (
		(memcmp(mvehdr.szID,IDSTR_MVE,20)) ||
		(mvehdr.magic1!=MVE_HEADER_MAGIC1) ||
		(mvehdr.magic2!=MVE_HEADER_MAGIC2) ||
		(((DWORD)(~(mvehdr.magic2))-(DWORD)mvehdr.magic3)!=MVE_HEADER_MAGIC3)
	   )
		return NULL;
	SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Walking MVE blocks chain...");
	blockhdr.size=0;
	blockhdr.type=0;
	while (
		   (!(RFPLUGIN(f)->EndOfFile(f))) &&
		   (
		    (blockhdr.size!=0) ||          // ???
			(blockhdr.type!=MVE_BT_END) || // ???
			(blockhdr.subtype!=0x00)       // ???
		   )
		  )
	{
		RFPLUGIN(f)->ReadFile(f,&blockhdr,sizeof(MVEBlockHeader),&read);
		if (read<sizeof(MVEBlockHeader))
			break; // ???
		RFPLUGIN(f)->SetFilePointer(f,blockhdr.size,FILE_CURRENT);
		SendMessage(hwnd,WM_GAP_SHOWFILEPROGRESS,0,(LPARAM)f);
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=(RFPLUGIN(f)->GetFilePointer(f))-pos;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"MVE file correct.");

    return node;
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
			SetCheckBox(hwnd,ID_NOWALK,opNoWalk);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_NOWALK,TRUE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opNoWalk=GetCheckBox(hwnd,ID_NOWALK);
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

SearchPattern patterns[]=
{
	{20,IDSTR_MVE}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX MVE Audio File Decoder",
    "v0.90",
    "Interplay Video Files (*.MVE)\0*.mve\0",
    "MVE",
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
