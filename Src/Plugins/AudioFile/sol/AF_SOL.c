/*
 * SOL plug-in source code
 * (Sierra On-Line music/sfx/speech)
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

#include <string.h>

#include <windows.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_SOL.h"
#include "resource.h"

typedef struct tagData
{
    WORD   align,channels,bits;
	BOOL   bCompressed,bSigned,bOld;
    DWORD  rate;
	BYTE   shift;
	LONG   curSample1,curSample2;
	char  *buffer;
} Data;

#define BUFFERSIZE (64000ul)

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

#define xlabs(x)      (((x)<0)?(-(x)):(x))

#define UNSIGNED16(x) ((x)^0x8000)
#define SIGNED8(x)    ((x)^0x80)

#define NIBBLE(b,n)   ( (n) ? (((b)&0xF0)>>4) : ((b)&0x0F) )

#define INDEX4C(c)    (0xF-(c))
#define INDEX4(c)     ((c)&7)

#define INDEX8C(c)    (0xFF-(c))
#define INDEX8(c)     ((c)&0x7F)

BYTE SOLTable3bit[]=
{
	0,
	1,
	2,
	3,
	6,
	0xA,
	0xF,
	0x15
};

WORD SOLTable7bit[]=
{
    0x0,   0x8,    0x10,   0x20,   0x30,   0x40,   0x50,   0x60,
    0x70,  0x80,   0x90,   0xA0,   0xB0,   0xC0,   0xD0,   0xE0,
    0xF0,  0x100,  0x110,  0x120,  0x130,  0x140,  0x150,  0x160,
    0x170, 0x180,  0x190,  0x1A0,  0x1B0,  0x1C0,  0x1D0,  0x1E0,
    0x1F0, 0x200,  0x208,  0x210,  0x218,  0x220,  0x228,  0x230,
    0x238, 0x240,  0x248,  0x250,  0x258,  0x260,  0x268,  0x270,
    0x278, 0x280,  0x288,  0x290,  0x298,  0x2A0,  0x2A8,  0x2B0,
    0x2B8, 0x2C0,  0x2C8,  0x2D0,  0x2D8,  0x2E0,  0x2E8,  0x2F0,
    0x2F8, 0x300,  0x308,  0x310,  0x318,  0x320,  0x328,  0x330,
    0x338, 0x340,  0x348,  0x350,  0x358,  0x360,  0x368,  0x370,
    0x378, 0x380,  0x388,  0x390,  0x398,  0x3A0,  0x3A8,  0x3B0,
    0x3B8, 0x3C0,  0x3C8,  0x3D0,  0x3D8,  0x3E0,  0x3E8,  0x3F0,
    0x3F8, 0x400,  0x440,  0x480,  0x4C0,  0x500,  0x540,  0x580,
    0x5C0, 0x600,  0x640,  0x680,  0x6C0,  0x700,  0x740,  0x780,
    0x7C0, 0x800,  0x900,  0xA00,  0xB00,  0xC00,  0xD00,  0xE00,
    0xF00, 0x1000, 0x1400, 0x1800, 0x1C00, 0x2000, 0x3000, 0x4000
};

typedef struct tagIntOption 
{
    char *str;
    int   value;
} IntOption;

IntOption decoderChoiceOptions[]=
{
	{"header", ID_USEHEADER},
	{"data",   ID_USEDATA}
};

BOOL opExtraChecks,
	 opUseClipping,
	 opUseFilter8,
	 opUseFilter16;
int  decoderChoice;

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
		opExtraChecks=TRUE;
		opUseClipping=TRUE;
		opUseFilter8=FALSE;
		opUseFilter16=FALSE;
		decoderChoice=ID_USEDATA;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"ExtraChecks","on",str,40,Plugin.szINIFileName);
    opExtraChecks=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseFilter8","off",str,40,Plugin.szINIFileName);
    opUseFilter8=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseFilter16","off",str,40,Plugin.szINIFileName);
    opUseFilter16=LoadOptionBool(str);
	decoderChoice=GetIntOption("DecoderChoice",decoderChoiceOptions,sizeof(decoderChoiceOptions)/sizeof(IntOption),1);
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

	WritePrivateProfileString(Plugin.Description,"ExtraChecks",SaveOptionBool(opExtraChecks),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseFilter8",SaveOptionBool(opUseFilter8),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseFilter16",SaveOptionBool(opUseFilter16),Plugin.szINIFileName);
	WriteIntOption("DecoderChoice",decoderChoiceOptions,sizeof(decoderChoiceOptions)/sizeof(IntOption),decoderChoice);
}

long ClipSample(long sample, WORD bits, BOOL bSigned)
{
	long range,max,min;

	range=1<<(bits-1);
	if (bSigned)
	{
		max=range-1;
		min=-range;
	}
	else
	{
		max=(range<<1)-1;
		min=0;
	}
	if (sample>max)
		return max;
    else if (sample<min)
		return min;

	return sample;
}

void Step_SOL_ADPCM(long* sample, BYTE code, WORD bits, BOOL bOld)
{
	if (bits==16)
	{
		if (code & 0x80)
			(*sample)-=SOLTable7bit[(bOld)?INDEX8C(code):INDEX8(code)];
		else
			(*sample)+=SOLTable7bit[code];
	}
	else
	{
		if (code & 8)
			(*sample)-=SOLTable3bit[(bOld)?INDEX4C(code):INDEX4(code)];
		else
			(*sample)+=SOLTable3bit[code];
	}
}

BOOL ReadHeader(
	FSHandle* file,
	DWORD* rate,
	WORD* channels,
	WORD* bits,
	BOOL* bCompressed,
	BOOL* bSigned,
	BOOL* bOld,
	BYTE* shift
)
{
	BYTE  code;
    DWORD i,read;
	BOOL  left;
	LONG  sample1,sample2,mean1,mean2,start,outsize;
	char  buffer[1024];
    SOLHeader solhdr;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&solhdr,sizeof(SOLHeader),&read);
	if (read<sizeof(SOLHeader))
		return FALSE;
    if (memcmp(solhdr.id,IDSTR_SOL,4))
		return FALSE;
	if (opExtraChecks)
	{
		if ((solhdr.pad!=SOL_ID_8D) && (solhdr.pad!=SOL_ID_D))
			return FALSE;
	}
    *rate=solhdr.rate;
    *channels=(solhdr.flags & SOL_STEREO)?2:1;
	*bits=(solhdr.flags & SOL_16BIT)?16:8;
	*bCompressed=(BOOL)(solhdr.flags & SOL_COMPRESSED);
	*bSigned=(BOOL)(solhdr.flags & SOL_SIGNED);
	switch (decoderChoice)
	{
		case ID_USEHEADER:
			if (!(*bCompressed))
				break;
			*bOld=(BOOL)(solhdr.shift+2==sizeof(SOLHeader));
			break;
		case ID_USEDATA:
			if (!(*bCompressed))
				break;
			start=(*bSigned)?0:1;
			start<<=(*bits)-1;
			sample2=sample1=start;
			mean1=mean2=0;
			left=TRUE;
			outsize=0;
			Plugin.fsh->SetFilePointer(file,solhdr.shift+2,FILE_BEGIN);
			Plugin.fsh->ReadFile(file,buffer,sizeof(buffer),&read);
			if (read==0L)
				return FALSE; // ???
			for (i=0;i<read;i++)
			{
				if (*bits==16)
				{
					code=buffer[i];
					if (left)
					{
						Step_SOL_ADPCM(&sample1,code,*bits,TRUE);
						Step_SOL_ADPCM(&sample2,code,*bits,FALSE);
						sample1=ClipSample(sample1,*bits,*bSigned);
						sample2=ClipSample(sample2,*bits,*bSigned);
						mean1+=(*bSigned)?((SHORT)sample1):((WORD)sample1);
						mean2+=(*bSigned)?((SHORT)sample2):((WORD)sample2);
						outsize++;
					}
					if (*channels==2)
						left=!left;
			    }
				else
				{
					code=NIBBLE(buffer[i],1);
					Step_SOL_ADPCM(&sample1,code,*bits,TRUE);
					Step_SOL_ADPCM(&sample2,code,*bits,FALSE);
					sample1=ClipSample(sample1,*bits,*bSigned);
					sample2=ClipSample(sample2,*bits,*bSigned);
					mean1+=(*bSigned)?((CHAR)sample1):((BYTE)sample1);
					mean2+=(*bSigned)?((CHAR)sample2):((BYTE)sample2);
					outsize++;
					if (*channels==1)
					{
						code=NIBBLE(buffer[i],0);
						Step_SOL_ADPCM(&sample1,code,*bits,TRUE);
						Step_SOL_ADPCM(&sample2,code,*bits,FALSE);
						sample1=ClipSample(sample1,*bits,*bSigned);
						sample2=ClipSample(sample2,*bits,*bSigned);
						mean1+=(*bSigned)?((CHAR)sample1):((BYTE)sample1);
						mean2+=(*bSigned)?((CHAR)sample2):((BYTE)sample2);
						outsize++;
					}
				}
			}
			mean1/=outsize;
			mean2/=outsize;
			*bOld=(BOOL)(xlabs(mean1-start)<xlabs(mean2-start));
			break;
		default: // ???
			break;
	}
	*shift=solhdr.shift;

    return TRUE;
}

void __stdcall Seek(FSHandle* f, DWORD pos);

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	BOOL bCompressed,bSigned,bOld;
	BYTE shift;

    if (f==NULL)
		return FALSE;

    if (!ReadHeader(f,rate,channels,bits,&bCompressed,&bSigned,&bOld,&shift))
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
	LPData(f)->bSigned=bSigned;
	LPData(f)->bOld=bOld;
	LPData(f)->shift=shift;
	if (bCompressed)
	{
		LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
		if (LPData(f)->buffer==NULL)
		{
			GlobalFree(f->afData);
			SetLastError(PRIVEC(IDS_NOBUFFER));
			return FALSE;
		}
		LPData(f)->curSample1=(bSigned)?0:1;
		LPData(f)->curSample1<<=(*bits)-1;
		LPData(f)->curSample2=LPData(f)->curSample1;
	}
	Plugin.fsh->SetFilePointer(f,shift+2,FILE_BEGIN);

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

DWORD Decode_SOL_ADPCM(FSHandle* f, char* chunk, DWORD size, char* buff)
{
    DWORD  i;
	BOOL   left;
    short *dstpcm16;
	BYTE   code;

    if (f==NULL)
		return 0;

    if (LPData(f)->bits==16)
    {
		dstpcm16=(short*)buff;
		left=TRUE;
		for (i=0;i<size;i++)
		{
			code=chunk[i];
			if (left)
			{
				Step_SOL_ADPCM(&(LPData(f)->curSample1),code,LPData(f)->bits,LPData(f)->bOld);
				if (opUseClipping)
					LPData(f)->curSample1=ClipSample(LPData(f)->curSample1,LPData(f)->bits,LPData(f)->bSigned);
				*dstpcm16++=(LPData(f)->bSigned)?((SHORT)(LPData(f)->curSample1)):UNSIGNED16((SHORT)(LPData(f)->curSample1));
			}
			else
			{
				Step_SOL_ADPCM(&(LPData(f)->curSample2),code,LPData(f)->bits,LPData(f)->bOld);
				if (opUseClipping)
					LPData(f)->curSample2=ClipSample(LPData(f)->curSample2,LPData(f)->bits,LPData(f)->bSigned);
				*dstpcm16++=(LPData(f)->bSigned)?((SHORT)(LPData(f)->curSample2)):UNSIGNED16((SHORT)(LPData(f)->curSample2));
			}
			if (LPData(f)->channels==2)
				left=!left;
		}
    }
    else
    {
		for (i=0;i<size;i++)
		{
			code=NIBBLE(chunk[i],1);
			Step_SOL_ADPCM(&(LPData(f)->curSample1),code,LPData(f)->bits,LPData(f)->bOld);
			if (opUseClipping)
				LPData(f)->curSample1=ClipSample(LPData(f)->curSample1,LPData(f)->bits,LPData(f)->bSigned);
			*buff++=(LPData(f)->bSigned)?SIGNED8((BYTE)(LPData(f)->curSample1)):((BYTE)(LPData(f)->curSample1));
			code=NIBBLE(chunk[i],0);
			if (LPData(f)->channels==2)
			{
				Step_SOL_ADPCM(&(LPData(f)->curSample2),code,LPData(f)->bits,LPData(f)->bOld);
				if (opUseClipping)
					LPData(f)->curSample2=ClipSample(LPData(f)->curSample2,LPData(f)->bits,LPData(f)->bSigned);
				*buff++=(LPData(f)->bSigned)?SIGNED8((BYTE)(LPData(f)->curSample2)):((BYTE)(LPData(f)->curSample2));
			}
			else
			{
				Step_SOL_ADPCM(&(LPData(f)->curSample1),code,LPData(f)->bits,LPData(f)->bOld);
				if (opUseClipping)
					LPData(f)->curSample1=ClipSample(LPData(f)->curSample1,LPData(f)->bits,LPData(f)->bSigned);
				*buff++=(LPData(f)->bSigned)?SIGNED8((BYTE)(LPData(f)->curSample1)):((BYTE)(LPData(f)->curSample1));
			}
		}
    }

    return 2*size;
}

void DecodeSigned8Bit(char *buffer, DWORD size)
{
	for (;size>0;size--,buffer++)
		*buffer=SIGNED8(*buffer);
}

void DecodeUnsigned16Bit(char *buffer, DWORD size)
{
	signed short *pcm16=(signed short*)buffer;

	if (size%2!=0)
		return;

	for (;size>0;size-=2,pcm16+=2)
		*pcm16=UNSIGNED16(*pcm16);
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate;
    WORD   channels,bits;
	BOOL   bCompressed,bSigned,bOld;
	BYTE   shift;

    if (f==NULL)
		return NULL;

    if (!ReadHeader(f,&rate,&channels,&bits,&bCompressed,&bSigned,&bOld,&shift))
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
    DWORD rate,filesize;
    FSHandle* f;
    WORD  channels,bits;
	BOOL bCompressed,bSigned,bOld;
	BYTE shift;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

    if (!ReadHeader(f,&rate,&channels,&bits,&bCompressed,&bSigned,&bOld,&shift))
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
    filesize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);

    return MulDiv(((bCompressed)?2:1)*8000,filesize-shift-2,channels*bits*rate);
}

DWORD Apply_SOL_LPF8(FSHandle* f, char* buffer, DWORD size)
{
	DWORD i,shift=2*(LPData(f)->channels);
	BYTE *pcm8=(BYTE*)buffer;

	for (i=0;i<size-shift;i++,pcm8++)
		pcm8[0]=(BYTE)(((WORD)pcm8[0]+(WORD)pcm8[shift])>>1);

	return (size-shift);
}

DWORD Apply_SOL_LPF16(FSHandle* f, char* buffer, DWORD size)
{
	DWORD  i,shift=2*(LPData(f)->channels);
	SHORT *pcm16=(SHORT*)buffer;

	for (i=0;i<size/2-shift;i++,pcm16++)
		pcm16[0]=(SHORT)(((LONG)pcm16[0]+(LONG)pcm16[shift])>>1);

	return (size-2*shift);
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read,toRead,pcmSize,decoded;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	if (LPData(file)->bCompressed)
	{
		pcmSize=0;
		toRead=min(buffsize/2,BUFFERSIZE);
		if (LPData(file)->align/2>0)
			toRead=CORRALIGN(toRead,LPData(file)->align/2);
		while ((!(Plugin.fsh->EndOfFile(file))) && (toRead>0))
		{
			Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
			if (read>0)
				decoded=Decode_SOL_ADPCM(file,LPData(file)->buffer,read,buffer+pcmSize);
			else
				break; // ???
			pcmSize+=decoded;
			buffsize-=decoded;
			toRead=min(buffsize/2,BUFFERSIZE);
			if (LPData(file)->align/2>0)
				toRead=CORRALIGN(toRead,LPData(file)->align/2);
		}
	}
	else
	{
		toRead=CORRALIGN(buffsize,LPData(file)->align);
		Plugin.fsh->ReadFile(file,buffer,toRead,&read);
		if ((LPData(file)->bSigned) && (LPData(file)->bits==8))
			DecodeSigned8Bit(buffer,read);
		if ((!(LPData(file)->bSigned)) && (LPData(file)->bits==16))
			DecodeUnsigned16Bit(buffer,read);
		pcmSize=read;
	}
	if (LPData(file)->bCompressed)
	{
		switch (LPData(file)->bits)
		{
			case 8:
				if (opUseFilter8)
				{
					
					LPData(file)->curSample1=(LONG)(((BYTE*)buffer)[pcmSize-3*(LPData(file)->channels)]);
					if (LPData(file)->channels==2)
						LPData(file)->curSample2=(LONG)(((BYTE*)buffer)[pcmSize-3*(LPData(file)->channels)+1]);
					pcmSize=Apply_SOL_LPF8(file,buffer,pcmSize);
					if (!(Plugin.fsh->EndOfFile(file)))
						Plugin.fsh->SetFilePointer(file,-(LONG)(LPData(file)->channels),FILE_CURRENT);
				}
				break;
			case 16:
				if (opUseFilter16)
				{
					LPData(file)->curSample1=(LONG)(((SHORT*)buffer)[(pcmSize/2)-3*(LPData(file)->channels)]);
					if (LPData(file)->channels==2)
						LPData(file)->curSample2=(LONG)(((SHORT*)buffer)[(pcmSize/2)-3*(LPData(file)->channels)+1]);
					pcmSize=Apply_SOL_LPF16(file,buffer,pcmSize);
					if (!(Plugin.fsh->EndOfFile(file)))
						Plugin.fsh->SetFilePointer(file,-(LONG)2*(LPData(file)->channels),FILE_CURRENT);
				}
				break;
			default: // ???
				break;
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
DWORD  ibSize,ibRate,ibTime;
WORD   ibChannels,ibBits;
BOOL   ibCompressed,ibSigned,ibOld;

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
			wsprintf(str,"Compression: %s %s\r\n"
						 "Format: %s\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (ibCompressed)?"SOL ADPCM":"None",
						 (ibCompressed)?((ibOld)?"(old)":"(new)"):"",
						 (ibSigned)?"Signed":"Unsigned",
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
    FSHandle* f;
	BYTE shift;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadHeader(f,&ibRate,&ibChannels,&ibBits,&ibCompressed,&ibSigned,&ibOld,&shift))
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
			SetCheckBox(hwnd,ID_EXTRACHECKS,opExtraChecks);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			SetCheckBox(hwnd,ID_USEFILTER8,opUseFilter8);
			SetCheckBox(hwnd,ID_USEFILTER16,opUseFilter16);
			CheckRadioButton(hwnd,ID_USEHEADER,ID_USEDATA,decoderChoice);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,ID_EXTRACHECKS,TRUE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					SetCheckBox(hwnd,ID_USEFILTER8,FALSE);
					SetCheckBox(hwnd,ID_USEFILTER16,FALSE);
					CheckRadioButton(hwnd,ID_USEHEADER,ID_USEDATA,ID_USEDATA);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opExtraChecks=GetCheckBox(hwnd,ID_EXTRACHECKS);
					opUseClipping=GetCheckBox(hwnd,ID_USECLIPPING);
					opUseFilter8=GetCheckBox(hwnd,ID_USEFILTER8);
					opUseFilter16=GetCheckBox(hwnd,ID_USEFILTER16);
					decoderChoice=GetRadioButton(hwnd,ID_USEHEADER,ID_USEDATA);
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

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    SOLHeader solhdr;
    DWORD     read;
    AFile*    node;

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying SOL file...");
    RFPLUGIN(f)->SetFilePointer(f,pos-2,FILE_BEGIN);
    RFPLUGIN(f)->ReadFile(f,&solhdr,sizeof(SOLHeader),&read);
	if (read<sizeof(SOLHeader))
		return NULL;
    if (memcmp(solhdr.id,IDSTR_SOL,4)) // ???
		return NULL;
	if (opExtraChecks)
	{
		if ((solhdr.pad!=SOL_ID_8D) && (solhdr.pad!=SOL_ID_D))
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos-2;
    node->fsLength=solhdr.size+solhdr.shift+2;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"SOL file correct.");

    return node;
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
	if (LPData(f)->bCompressed)
		filepos/=2;
    Plugin.fsh->SetFilePointer(f,LPData(f)->shift+2+filepos,FILE_BEGIN);
}

SearchPattern patterns[]=
{
	{4,IDSTR_SOL}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX SOL Audio File Decoder",
    "v0.98",
    "Sierra On-Line Audio Files (*.AUD;*.SFX)\0*.aud;*.sfx\0",
    "SOL",
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
