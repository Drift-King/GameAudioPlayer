/*
 * RAW plug-in source code
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code uses A-law/u-law->PCM parts of g711.c by
 * Sun Microsystems, Inc.
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

#include "resource.h"

#define BUFFERSIZE (64000ul)

#define SIGN_BIT   (0x80) /* Sign bit for a A-law byte. */
#define QUANT_MASK (0xf)  /* Quantization field mask. */
#define NSEGS      (8)    /* Number of A-law segments. */
#define SEG_SHIFT  (4)    /* Left shift for segment number. */
#define SEG_MASK   (0x70) /* Segment field mask. */

typedef struct tagIntOption 
{
    char *str;
    int   value;
} IntOption;

typedef struct tagData
{
	BYTE  flags;
    WORD  channels,bits,align;
    DWORD rate;
	char *buffer;
} Data;

#define RAW_INTEL    (0x1)
#define RAW_SIGNED   (0)
#define RAW_UNSIGNED (1)
#define RAW_SIGNBIT  (2)
#define RAW_U_LAW    (3)
#define RAW_A_LAW    (4)

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

int   addFiles,
      rawParams,
      defChannels,
      defResolution,
      defByteOrder,
      defFormat;
DWORD defRate;

char  dlgTitle[MAX_PATH];
int   dlgChannels,
      dlgResolution,
      dlgByteOrder,
      dlgFormat;
DWORD dlgRate;

#define RATENUM (9)
DWORD ratelist[RATENUM]=
{
    48000,
    44100,
    32075,
    32000,
    22050,
    16000,
    11025,
    8000,
    6000
};

char* ratestr[RATENUM]=
{
    "48000",
    "44100",
    "32075",
    "32000",
    "22050",
    "16000",
    "11025",
    "8000",
    "6000"
};

IntOption addFilesOptions[]=
{
    {"always", ID_ADD_ALWAYS},
    {"ext",    ID_ADD_EXT},
    {"never",  ID_ADD_NEVER}
};

IntOption rawParamsOptions[]=
{
    {"ask",	 ID_PARAMS_ASK},
    {"defaults", ID_PARAMS_DEFAULTS}
};

IntOption defChannelsOptions[]=
{
    {"mono",   ID_MONO},
    {"stereo", ID_STEREO}
};

IntOption defResolutionOptions[]=
{
    {"8-bit",  ID_8BIT},
    {"16-bit", ID_16BIT}
};

IntOption defByteOrderOptions[]=
{
    {"intel",	 ID_INTEL},
    {"motorola", ID_MOTOROLA}
};

IntOption defFormatOptions[]=
{
    {"signed",	 ID_SIGNED},
    {"unsigned", ID_UNSIGNED},
    {"sign-bit",  ID_SIGNBIT},
	{"u-law",    ID_U_LAW},
	{"A-law",    ID_A_LAW}
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

void __stdcall Init(void)
{
	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		addFiles=ID_ADD_EXT;
		rawParams=ID_PARAMS_ASK;
		dlgChannels=defChannels=ID_MONO;
		dlgResolution=defResolution=ID_16BIT;
		dlgByteOrder=defByteOrder=ID_INTEL;
		dlgFormat=defFormat=ID_SIGNED;
		dlgRate=defRate=22050;
		return;
    }
    addFiles=GetIntOption("AddFiles",addFilesOptions,sizeof(addFilesOptions)/sizeof(IntOption),1);
    rawParams=GetIntOption("RawParams",rawParamsOptions,sizeof(rawParamsOptions)/sizeof(IntOption),0);
    dlgChannels=defChannels=GetIntOption("DefChannels",defChannelsOptions,sizeof(defChannelsOptions)/sizeof(IntOption),0);
    dlgResolution=defResolution=GetIntOption("DefResolution",defResolutionOptions,sizeof(defResolutionOptions)/sizeof(IntOption),1);
    dlgByteOrder=defByteOrder=GetIntOption("DefByteOrder",defByteOrderOptions,sizeof(defByteOrderOptions)/sizeof(IntOption),0);
    dlgFormat=defFormat=GetIntOption("DefFormat",defFormatOptions,sizeof(defFormatOptions)/sizeof(IntOption),0);
    dlgRate=defRate=(DWORD)GetPrivateProfileInt(Plugin.Description,"DefRate",22050,Plugin.szINIFileName);
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

void __stdcall Quit(void)
{
    char str[40];

    if (Plugin.szINIFileName==NULL)
		return;

    WriteIntOption("AddFiles",addFilesOptions,sizeof(addFilesOptions)/sizeof(IntOption),addFiles);
    WriteIntOption("RawParams",rawParamsOptions,sizeof(rawParamsOptions)/sizeof(IntOption),rawParams);
    WriteIntOption("DefChannels",defChannelsOptions,sizeof(defChannelsOptions)/sizeof(IntOption),defChannels);
    WriteIntOption("DefResolution",defResolutionOptions,sizeof(defResolutionOptions)/sizeof(IntOption),defResolution);
    WriteIntOption("DefByteOrder",defByteOrderOptions,sizeof(defByteOrderOptions)/sizeof(IntOption),defByteOrder);
    WriteIntOption("DefFormat",defFormatOptions,sizeof(defFormatOptions)/sizeof(IntOption),defFormat);
    wsprintf(str,"%lu",defRate);
    WritePrivateProfileString(Plugin.Description,"DefRate",str,Plugin.szINIFileName);
}

void GrayButtons(HWND hwnd, int resolution)
{
	BOOL res16bit=(resolution==ID_16BIT);

	EnableWindow(GetDlgItem(hwnd,ID_INTEL),res16bit);
    EnableWindow(GetDlgItem(hwnd,ID_MOTOROLA),res16bit);
	if (
		(!res16bit) &&
		(
		 (IsDlgButtonChecked(hwnd,ID_U_LAW)==BST_CHECKED) ||
		 (IsDlgButtonChecked(hwnd,ID_A_LAW)==BST_CHECKED)
		)
	   )
		CheckRadioButton(hwnd,ID_SIGNED,ID_A_LAW,ID_UNSIGNED);
	EnableWindow(GetDlgItem(hwnd,ID_U_LAW),res16bit);
	EnableWindow(GetDlgItem(hwnd,ID_A_LAW),res16bit);
}

void SetSampleRate(HWND hwnd, DWORD sr)
{
    char str[20];

    wsprintf(str,"%lu",sr);
    SetDlgItemText(hwnd,ID_RATE,str);
}

DWORD get_decimal(const char* str)
{
	char  ch;
	DWORD retval;
	BOOL  success;

	retval=0L;
	success=FALSE;
	while (1)
	{
		ch=*str;
		if ((ch>='0') && (ch<='9'))
		{
			retval*=10;
			retval+=ch-'0';
			success=TRUE;
		}
		else
			break;
		str++;
	}

	return ((success)?retval:(-1));
}

DWORD GetSampleRate(HWND hwnd)
{
    char	str[20];

    SendDlgItemMessage(hwnd,ID_RATE,WM_GETTEXT,(WPARAM)20,(LPARAM)(LPSTR)str);

    return get_decimal(str);
}

int GetRadioButton(HWND hwnd, int first, int last)
{
    int i;

    for (i=first;i<=last;i++)
		if (IsDlgButtonChecked(hwnd,i)==BST_CHECKED)
			return i;

    return first;
}

void SetRateList(HWND hwnd, DWORD rate)
{
    int i;

    for (i=0;i<RATENUM;i++)
    {
		if (ratelist[i]==rate)
		{
			SendDlgItemMessage(hwnd,ID_RATELIST,LB_SETCURSEL,i,0L);
			break;
		}
		else
			SendDlgItemMessage(hwnd,ID_RATELIST,LB_SETCURSEL,4,0L);
    }
}

void SetDefaults(HWND hwnd)
{
    CheckRadioButton(hwnd,ID_MONO,ID_STEREO,defChannels);
    CheckRadioButton(hwnd,ID_8BIT,ID_16BIT,defResolution);
    CheckRadioButton(hwnd,ID_INTEL,ID_MOTOROLA,defByteOrder);
    CheckRadioButton(hwnd,ID_SIGNED,ID_A_LAW,defFormat);
    GrayButtons(hwnd,defResolution);
    SetSampleRate(hwnd,defRate);
    SetRateList(hwnd,defRate);
}

void InitOptions(HWND hwnd)
{
    CheckRadioButton(hwnd,ID_MONO,ID_STEREO,dlgChannels);
    CheckRadioButton(hwnd,ID_8BIT,ID_16BIT,dlgResolution);
    CheckRadioButton(hwnd,ID_INTEL,ID_MOTOROLA,dlgByteOrder);
    CheckRadioButton(hwnd,ID_SIGNED,ID_A_LAW,dlgFormat);
    GrayButtons(hwnd,dlgResolution);
    SetSampleRate(hwnd,dlgRate);
    SetRateList(hwnd,dlgRate);
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

BOOL CALLBACK AskRawDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int  cursel;
    char str[MAX_PATH+100];

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			wsprintf(str,"Raw File Type: %s",(!lstrcmpi(dlgTitle,"\0"))?NO_AFNAME:GetFileTitleEx(dlgTitle));
			SetWindowText(hwnd,str);
			for (cursel=0;cursel<RATENUM;cursel++)
				SendDlgItemMessage(hwnd,ID_RATELIST,LB_ADDSTRING,0,(LPARAM)ratestr[cursel]);
			InitOptions(hwnd);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_RATELIST:
					if (HIWORD(wParam)==LBN_SELCHANGE)
					{
						cursel=(int)SendDlgItemMessage(hwnd,ID_RATELIST,LB_GETCURSEL,0,0L);
						SetSampleRate(hwnd,ratelist[cursel]);
					}
					break;
				case ID_DEFAULTS:
					SetDefaults(hwnd);
					break;
				case ID_16BIT:
					GrayButtons(hwnd,ID_16BIT);
					break;
				case ID_8BIT:
					GrayButtons(hwnd,ID_8BIT);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					dlgRate=GetSampleRate(hwnd);
					dlgChannels=GetRadioButton(hwnd,ID_MONO,ID_STEREO);
					dlgResolution=GetRadioButton(hwnd,ID_8BIT,ID_16BIT);
					dlgByteOrder=GetRadioButton(hwnd,ID_INTEL,ID_MOTOROLA);
					dlgFormat=GetRadioButton(hwnd,ID_SIGNED,ID_A_LAW);
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

BOOL AskRawParams(const char* rfName, const char* rfDataString, DWORD* rate, WORD* channels, WORD* bits, BYTE* flags)
{
    switch (rawParams)
    {
		case ID_PARAMS_ASK:
			if ((rfDataString!=NULL) && (lstrcmpi(rfDataString,"")))
				lstrcpy(dlgTitle,rfDataString);
			else if ((rfName!=NULL) && (lstrcmpi(rfName,"")))
				lstrcpy(dlgTitle,rfName);
			else
				lstrcpy(dlgTitle,"");
			if (DialogBox(Plugin.hDllInst,"AskRawDialog",GetFocus(),AskRawDlgProc))
			{
				*rate=dlgRate;
				*channels=(dlgChannels==ID_STEREO)?2:1;
				*bits=(dlgResolution==ID_16BIT)?16:8;
				*flags=(dlgByteOrder==ID_INTEL)?RAW_INTEL:0;
				(*flags)|=(RAW_SIGNED+(dlgFormat-ID_SIGNED))<<1;
				return TRUE;
			}
			else
				return FALSE;
		case ID_PARAMS_DEFAULTS:
			*rate=defRate;
			*channels=(defChannels==ID_STEREO)?2:1;
			*bits=(defResolution==ID_16BIT)?16:8;
			*flags=(defByteOrder==ID_INTEL)?RAW_INTEL:0;
			(*flags)|=(RAW_SIGNED+(defFormat-ID_SIGNED))<<1;
			return TRUE;
		default: // ???
			return FALSE;
    }
}

BOOL ParseRawParamString
(
	char* str, 
	DWORD *rate, 
	WORD *channels, 
	WORD *bits, 
	BYTE *flags
)
{
	char *end;

	if (str==NULL)
		return FALSE;

	*rate=-1;
	*channels=-1;
	*bits=-1;
	*flags=-1;

	*rate=strtoul(str,&end,0x10);
	while (*end==' ')
		end++;
	*channels=(WORD)strtoul(end,&end,0x10);
	while (*end==' ')
		end++;
	*bits=(WORD)strtoul(end,&end,0x10);
	while (*end==' ')
		end++;
	*flags=(BYTE)strtoul(end,&end,0x10);
	
	return (
			(*rate!=-1) &&
			(*channels!=(WORD)(-1)) &&
			(*bits!=(WORD)(-1)) &&
			(*flags!=(BYTE)(-1))
		   );
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	BYTE flags;

    if (f==NULL)
		return FALSE;

	if (f->node==NULL)
	{
		SetLastError(PRIVEC(IDS_NONODE));
		return FALSE;
	}
    if (!ParseRawParamString(f->node->afDataString,rate,channels,bits,&flags))
    {
		if (!AskRawParams(f->node->rfName,f->node->rfDataString,rate,channels,bits,&flags))
		{
			SetLastError(PRIVEC(IDS_NOPARAMS));
			return FALSE;
		}
    }
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
	if ((flags>>1==RAW_A_LAW) || (flags>>1==RAW_U_LAW))
	{
		LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
		if (LPData(f)->buffer==NULL)
		{
			GlobalFree(f->afData);
			SetLastError(PRIVEC(IDS_NOBUFFER));
			return FALSE;
		}
	}
    Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    LPData(f)->align=(*channels)*((*bits)/8);
    LPData(f)->flags=flags;
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

    return (
			(GlobalFree(LPData(f)->buffer)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

#define USI(x)	      ((unsigned short)(x))
#define SWAPWORD(x)   (((USI(x)&0xFF)<<8)|(USI(x)>>8))
#define SIGNBIT8(x)   (((x)&0x80)?(~(x)):((x)^0x80))
#define SIGNBIT16(x)  (((x)&0x8000)?((~(x))|0x8000):(x))
#define UNSIGNED16(x) ((x)^0x8000)
#define SIGNED8(x)    ((x)^0x80)

short alaw2pcm(unsigned char a_val)
{
	short t;
	BYTE  seg;

	a_val^=0x55;
	t=(a_val & QUANT_MASK)<<4;
	seg=((unsigned)a_val & SEG_MASK)>>SEG_SHIFT;
	switch (seg)
	{
		case 0:
			t+=8;
			break;
		case 1:
			t+=0x108;
			break;
		default:
			t+=0x108;
			t<<=seg-1;
	}

	return ((a_val & SIGN_BIT)?t:-t);
}

#define BIAS  (0x84)  /* Bias for linear code. */

short ulaw2pcm(unsigned char u_val)
{
	short t;

	u_val=~u_val;

	t=((u_val & QUANT_MASK)<<3)+BIAS;
	t<<=((unsigned)u_val & SEG_MASK)>>SEG_SHIFT;

	return ((u_val & SIGN_BIT)?(BIAS-t):(t-BIAS));
}

DWORD DecodeBuffer(FSHandle* f, PVOID chunk, DWORD size, PVOID buff)
{
    DWORD   outsize,i;
    signed short *srcpcm16,*dstpcm16;
    signed char  *srcpcm8,*dstpcm8;

    if (f==NULL)
		return 0;
    if (LPData(f)==NULL)
		return 0;

	outsize=0;
    switch ((LPData(f)->flags)>>1)
    {
		case RAW_A_LAW:
			srcpcm8=(signed char*)chunk;
			dstpcm16=(signed short*)buff;
			for (i=0;i<size;i++,srcpcm8++,dstpcm16++)
				*dstpcm16=alaw2pcm((BYTE)(*srcpcm8));
			outsize=2*size;
			break;
		case RAW_U_LAW:
			srcpcm8=(signed char*)chunk;
			dstpcm16=(signed short*)buff;
			for (i=0;i<size;i++,srcpcm8++,dstpcm16++)
				*dstpcm16=ulaw2pcm((BYTE)(*srcpcm8));
			outsize=2*size;
			break;
		case RAW_SIGNBIT:
			if (LPData(f)->bits==16)
			{
				srcpcm16=(signed short*)chunk;
				dstpcm16=(signed short*)buff;
				for (i=0;i<size;i+=2,srcpcm16++,dstpcm16++)
				{
					if (LPData(f)->flags & RAW_INTEL)
						*dstpcm16=SIGNBIT16(*srcpcm16);
					else
						*dstpcm16=SIGNBIT16(SWAPWORD(*srcpcm16));
				}
			}
			else
			{
				srcpcm8=(signed char*)chunk;
				dstpcm8=(signed char*)buff;
				for (i=0;i<size;i++,srcpcm8++,dstpcm8++)
					*dstpcm8=SIGNBIT8(*srcpcm8);
			}
			outsize=size;
			break;
		case RAW_SIGNED:
			if (LPData(f)->bits==16)
			{
				if (LPData(f)->flags & RAW_INTEL)
				{
					if (chunk==buff)
					{
						outsize=size;
						break;
					}
				}
				srcpcm16=(signed short*)chunk;
				dstpcm16=(signed short*)buff;
				for (i=0;i<size;i+=2,srcpcm16++,dstpcm16++)
				{
					if (LPData(f)->flags & RAW_INTEL)
						*dstpcm16=*srcpcm16;
					else
						*dstpcm16=SWAPWORD(*srcpcm16);
				}
			}
			else
			{
				srcpcm8=(signed char*)chunk;
				dstpcm8=(signed char*)buff;
				for (i=0;i<size;i++,srcpcm8++,dstpcm8++)
					*dstpcm8=SIGNED8(*srcpcm8);
			}
			outsize=size;
			break;
		case RAW_UNSIGNED:
			if (LPData(f)->bits==16)
			{
				srcpcm16=(signed short*)chunk;
				dstpcm16=(signed short*)buff;
				for (i=0;i<size;i+=2,srcpcm16++,dstpcm16++)
				{
					if (LPData(f)->flags & RAW_INTEL)
						*dstpcm16=UNSIGNED16(*srcpcm16);
					else
						*dstpcm16=UNSIGNED16(SWAPWORD(*srcpcm16));
				}
			}
			else
			{
				if (chunk==buff)
				{
					outsize=size;
					break;
				}
				srcpcm8=(signed char*)chunk;
				dstpcm8=(signed char*)buff;
				for (i=0;i<size;i++,srcpcm8++,dstpcm8++)
					*dstpcm8=*srcpcm8;
			}
			outsize=size;
			break;
		default: // ???
			outsize=0;
    }
    return outsize;
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

BOOL GetRawParams(const char* rfName, const char* rfDataString, DWORD* rate, WORD* channels, WORD* bits, BYTE* flags)
{
    char *ext,*defext,*filename;

    switch (addFiles)
	{
		case ID_ADD_NEVER:
			return FALSE;
		case ID_ADD_EXT:
			if ((rfDataString!=NULL) && (lstrcmpi(rfDataString,"")))
				filename=(char*)rfDataString;
			else if ((rfName!=NULL) && (lstrcmpi(rfName,"")))
				filename=(char*)rfName;
			else
				filename=NULL;
			ext=GetFileExtension(filename);
			defext=Plugin.afExtensions;
			if (defext==NULL)
				return FALSE;
			while (defext[0]!='\0')
			{
				defext+=lstrlen(defext)+2;
				if (!lstrcmpi(ext,defext))
					return AskRawParams(rfName,rfDataString,rate,channels,bits,flags);
				defext+=lstrlen(defext)+1;
			}
			return FALSE;
		case ID_ADD_ALWAYS:
			return AskRawParams(rfName,rfDataString,rate,channels,bits,flags);
		default: // ???
			return FALSE;
	}
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate;
	WORD   channels,bits;
    BYTE   flags;

    if (f==NULL)
		return NULL;

	if (!GetRawParams(rfName,rfDataString,&rate,&channels,&bits,&flags))
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    wsprintf(node->afDataString,"%lX %X %X %X",rate,channels,bits,flags);
	lstrcpy(node->afName,"");

    return node;
}

DWORD __stdcall GetTime(AFile* node)
{
    FSHandle* f;
    DWORD rate,filesize;
	WORD  channels,bits;
    BYTE  flags,factor;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    filesize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);
	if (!ParseRawParamString(node->afDataString,&rate,&channels,&bits,&flags))
		return -1;
	factor=(((flags>>1)==RAW_A_LAW)||((flags>>1)==RAW_U_LAW))?2:1;

    return MulDiv(8000,filesize*factor,channels*bits*rate);
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

	pcmSize=0;
	switch ((LPData(file)->flags)>>1)
	{
		case RAW_SIGNED:
		case RAW_UNSIGNED:
		case RAW_SIGNBIT:
			toRead=CORRALIGN(buffsize,LPData(file)->align);
			Plugin.fsh->ReadFile(file,buffer,toRead,&read);
			if (read>0)
				pcmSize=DecodeBuffer(file,buffer,read,buffer);
			else
				pcmSize=0L;
			break;
		case RAW_U_LAW:
		case RAW_A_LAW:
			toRead=CORRALIGN(buffsize,LPData(file)->align)/2;
			while ((toRead>0) && (!(Plugin.fsh->EndOfFile(file))))
			{
				Plugin.fsh->ReadFile(file,LPData(file)->buffer,min(toRead,BUFFERSIZE),&read);
				if (read>0)
					decoded=DecodeBuffer(file,LPData(file)->buffer,read,buffer+pcmSize);
				else
					break;
				pcmSize+=decoded;
				toRead-=read;
			}
			break;
		default: // ???
			break;
	}
	*buffpos=-1;

	return pcmSize;
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

void SetDefaultParams(HWND hwnd)
{
    CheckRadioButton(hwnd,ID_ADD_ALWAYS,ID_ADD_NEVER,ID_ADD_EXT);
    CheckRadioButton(hwnd,ID_PARAMS_ASK,ID_PARAMS_DEFAULTS,ID_PARAMS_ASK);
    CheckRadioButton(hwnd,ID_MONO,ID_STEREO,ID_MONO);
    CheckRadioButton(hwnd,ID_8BIT,ID_16BIT,ID_16BIT);
    CheckRadioButton(hwnd,ID_INTEL,ID_MOTOROLA,ID_INTEL);
    CheckRadioButton(hwnd,ID_SIGNED,ID_A_LAW,ID_SIGNED);
    GrayButtons(hwnd,ID_16BIT);
    SetSampleRate(hwnd,22050);
    SetRateList(hwnd,22050);
}

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int cursel;

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			for (cursel=0;cursel<RATENUM;cursel++)
				SendDlgItemMessage(hwnd,ID_RATELIST,LB_ADDSTRING,0,(LPARAM)ratestr[cursel]);
			CheckRadioButton(hwnd,ID_ADD_ALWAYS,ID_ADD_NEVER,addFiles);
			CheckRadioButton(hwnd,ID_PARAMS_ASK,ID_PARAMS_DEFAULTS,rawParams);
			SetDefaults(hwnd);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_RATELIST:
					if (HIWORD(wParam)==LBN_SELCHANGE)
					{
						cursel=(int)SendDlgItemMessage(hwnd,ID_RATELIST,LB_GETCURSEL,0,0L);
						SetSampleRate(hwnd,ratelist[cursel]);
					}
					break;
				case ID_DEFAULTS:
					SetDefaultParams(hwnd);
					break;
				case ID_16BIT:
					GrayButtons(hwnd,ID_16BIT);
					break;
				case ID_8BIT:
					GrayButtons(hwnd,ID_8BIT);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					addFiles=GetRadioButton(hwnd,ID_ADD_ALWAYS,ID_ADD_NEVER);
					rawParams=GetRadioButton(hwnd,ID_PARAMS_ASK,ID_PARAMS_DEFAULTS);
					dlgRate=defRate=GetSampleRate(hwnd);
					dlgChannels=defChannels=GetRadioButton(hwnd,ID_MONO,ID_STEREO);
					dlgResolution=defResolution=GetRadioButton(hwnd,ID_8BIT,ID_16BIT);
					dlgByteOrder=defByteOrder=GetRadioButton(hwnd,ID_INTEL,ID_MOTOROLA);
					dlgFormat=defFormat=GetRadioButton(hwnd,ID_SIGNED,ID_A_LAW);
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

void __stdcall About(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"About",hwnd,AboutDlgProc);
}

AFile* ibNode;
DWORD  ibSize,ibRate,ibTime;
WORD   ibChannels,ibBits;
BYTE   ibFlags;

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
    char str[512],fstr[100];
	BYTE format=ibFlags>>1;

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
			switch (format)
			{
				case RAW_SIGNED:
					lstrcpy(fstr,"Signed");
					break;
				case RAW_UNSIGNED:
					lstrcpy(fstr,"Unsigned");
					break;
				case RAW_SIGNBIT:
					lstrcpy(fstr,"Sign bit");
					break;
				case RAW_U_LAW:
					lstrcpy(fstr,"G.711 u-Law");
					break;
				case RAW_A_LAW:
					lstrcpy(fstr,"G.711 A-Law");
					break;
				default: // ???
					lstrcpy(fstr,"Unknown");
			}
			wsprintf(str,"Format: %s\r\n"
						 "Channels: %u %s\r\n"
						 "Sample rate: %lu Hz\r\n"
						 "Resolution: %u-bit\r\n"
						 "Byte order: %s",
						 fstr,
						 ibChannels,
						 (ibChannels==4)?"(quadro)":((ibChannels==2)?"(stereo)":((ibChannels==1)?"(mono)":"")),
						 ibRate,
						 ibBits,
						 ((ibBits==16)&&(format!=RAW_A_LAW)&&(format!=RAW_U_LAW))?((ibFlags & RAW_INTEL)?"Little-endian":"Big-endian"):"None"
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
    FSHandle* f;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    ibSize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);
	if (!ParseRawParamString(node->afDataString,&ibRate,&ibChannels,&ibBits,&ibFlags))
    {
		MessageBox(hwnd,"File info not available.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
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
	if (
		((LPData(f)->flags>>1)==RAW_A_LAW) ||
		((LPData(f)->flags>>1)==RAW_U_LAW)
	   )
		filepos/=2;
    Plugin.fsh->SetFilePointer(f,filepos,FILE_BEGIN);
}

AFPlugin Plugin=
{
	AFP_VER,
    0,
    0,
	0,
    NULL,
    "ANX RAW Audio File Plug-In",
    "v1.01",
    "RAW Audio Files (*.RAW)\0*.raw\0"
	"SND Audio Files (*.SND)\0*.snd\0",
    "RAW",
    0L,
    NULL,
    NULL,
    Config,
    About,
    Init,
    Quit,
    InfoBox,
    InitPlayback,
    ShutdownPlayback,
    FillPCMBuffer,
    NULL,
    CreateNodeForFile,
    GetTime,
    Seek
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
