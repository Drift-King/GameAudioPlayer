/*
 * AUD/VQA plug-in source code
 * (Westwood Studios music/sfx/speech/video)
 *
 * Copyright (C) 1998-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code is based on the specs written by Vladan Bato:
 * E-mail: bat22@geocities.com
 * http://www.geocities.com/SiliconValley/8682
 * And also on the VQA specs written by Aaron Glover:
 * E-mail: arn@ibm.net
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

#include <windows.h>
#include <commctrl.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_AUD.h"
#include "resource.h"

#define WM_GAP_WAITINGTOY (WM_USER+1)

#define VQA_SND0  (0)
#define VQA_SND1  (1)
#define VQA_SND2  (2)

#define AUD_VER_1 VQA_VER_1 // ???
#define AUD_VER_2 VQA_VER_2 // ???

#define BUFFERSIZE (64000ul)

typedef struct tagIntOption 
{
    char *str;
    int   value;
} IntOption;

typedef struct tagData
{
    BYTE   type;
	BOOL   bAUD;
	WORD   channels,bits,align;
	DWORD  rate;
    DWORD  curChunkSize,curChunkOutSize;
	DWORD  sndStart,size1,size2;
	WORD   numFrames,version;
    long   Index1,Index2;
    long   Cur_Sample1,Cur_Sample2;
    char  *buffer;
	DWORD *finf;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

int audTimeCalcMethod;
BOOL opNoWalk,
	 opUseClipping,
	 opUseEnhancement;

IntOption audTimeCalcMethodOptions[]=
{
	{"header_outsize",ID_AH_OUTSIZE},
	{"header_size",   ID_AH_SIZE},
	{"filesize",	  ID_FILE_SIZE},
	{"walk_outsize",  ID_CH_OUTSIZE},
	{"walk_size",	  ID_CH_SIZE}
};

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

char WSTable2bit[]=
{
	-2,
	-1,
	0,
	1
};

char WSTable4bit[]=
{
	-9,
	-8,
	-6,
	-5,
	-4,
	-3,
	-2,
	-1,
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	8
};

#define NIBBLE(b,n) ( (n) ? (((b)&0xF0)>>4) : ((b)&0x0F) )

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
		audTimeCalcMethod=ID_AH_OUTSIZE;
		opNoWalk=FALSE;
		opUseClipping=TRUE;
		opUseEnhancement=FALSE;
		return;
    }
    audTimeCalcMethod=GetIntOption("TimeCalculationMethod",audTimeCalcMethodOptions,sizeof(audTimeCalcMethodOptions)/sizeof(IntOption),0);
	GetPrivateProfileString(Plugin.Description,"NoWalk","off",str,40,Plugin.szINIFileName);
    opNoWalk=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseEnhancement","off",str,40,Plugin.szINIFileName);
    opUseEnhancement=LoadOptionBool(str);
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

    WriteIntOption("TimeCalculationMethod",audTimeCalcMethodOptions,sizeof(audTimeCalcMethodOptions)/sizeof(IntOption),audTimeCalcMethod);
	WritePrivateProfileString(Plugin.Description,"NoWalk",SaveOptionBool(opNoWalk),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseEnhancement",SaveOptionBool(opUseEnhancement),Plugin.szINIFileName);
}

void AlignFilePointer2(FSHandle* file)
{
	if ((Plugin.fsh->GetFilePointer(file))%2)
		Plugin.fsh->SetFilePointer(file,1,FILE_CURRENT);
}

BOOL ReadAUDHeader
(
	FSHandle* file, 
	DWORD* rate, 
	WORD*  channels, 
	WORD*  bits, 
	BYTE*  type, 
	WORD*  version,
	DWORD* size,
	DWORD* outsize
)
{
    AUDHeader	audhdr;
    AUDHeaderOld audhdrold;
    AUDChunkHeader audchunkhdr;
    DWORD	read,shift;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(file,&audhdr,sizeof(AUDHeader),&read);
	if (read<sizeof(AUDHeader))
		return FALSE; // ???
    Plugin.fsh->ReadFile(file,&audchunkhdr,sizeof(AUDChunkHeader),&read);
	if (read<sizeof(AUDChunkHeader))
		return FALSE; // ???
    if (audchunkhdr.id!=ID_DEAF)
    {
		Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);
		Plugin.fsh->ReadFile(file,&audhdrold,sizeof(AUDHeaderOld),&read);
		if (read<sizeof(AUDHeaderOld))
			return FALSE; // ???
		Plugin.fsh->ReadFile(file,&audchunkhdr,sizeof(AUDChunkHeader),&read);
		if (read<sizeof(AUDChunkHeader))
			return FALSE; // ???
		if (audchunkhdr.id!=ID_DEAF)
			return FALSE;
		*version=AUD_VER_1;
		*type=audhdrold.type;
		*channels=(audhdrold.flags & AUD_STEREO)?2:1;
		*bits=(audhdrold.flags & AUD_16BIT)?16:8;
		*rate=audhdrold.rate;
		*size=audhdrold.size;
		*outsize=-1;
    }
    else
    {
		*version=AUD_VER_2;
		*type=audhdr.type;
		*channels=(audhdr.flags & AUD_STEREO)?2:1;
		*bits=(audhdr.flags & AUD_16BIT)?16:8;
		*rate=audhdr.rate;
		*size=audhdr.size;
		*outsize=audhdr.outsize;
    }
	switch (*version)
	{
		case AUD_VER_1:
			shift=sizeof(AUDHeaderOld);
			break;
		default: // ???
		case AUD_VER_2:
			shift=sizeof(AUDHeader);
			break;
	}
	Plugin.fsh->SetFilePointer(file,shift,FILE_BEGIN);

    return TRUE;
}

DWORD FindFORMChunk(FSHandle* file, char* id, DWORD idsize)
{
	DWORD	        read;
	FORMChunkHeader chunkhdr;

	AlignFilePointer2(file);
	Plugin.fsh->ReadFile(file,&chunkhdr,sizeof(FORMChunkHeader),&read);
	while (
		   (memcmp(chunkhdr.szID,id,idsize)) && 
		   (!(Plugin.fsh->EndOfFile(file)))
		  )
	{
		Plugin.fsh->SetFilePointer(file,SWAPDWORD(chunkhdr.size),FILE_CURRENT);
		AlignFilePointer2(file);
		Plugin.fsh->ReadFile(file,&chunkhdr,sizeof(FORMChunkHeader),&read);
		if (read<sizeof(FORMChunkHeader))
			break; // ???
	}
	if (memcmp(chunkhdr.szID,id,idsize))
		return -1;
	else
		return SWAPDWORD(chunkhdr.size);
}

BOOL ReadVQAHeader
(
	FSHandle* file, 
	DWORD*  rate, 
	WORD*   channels, 
	WORD*   bits, 
	BYTE*   type, 
	WORD*   version,
	WORD*   numframes,
	DWORD*  size1,
	DWORD*  size2,
	DWORD*  sndStart,
	DWORD** finf
)
{
	WORD       size;
	DWORD	   read,chunkSize;
	FORMHeader formhdr;
    VQAHeader  vqahdr;
	FORMChunkHeader chunkhdr;

    if (file==NULL)
		return FALSE;

	Plugin.fsh->SetFilePointer(file,0L,FILE_BEGIN);
	Plugin.fsh->ReadFile(file,&formhdr,sizeof(FORMHeader),&read);
	if (
		(read<sizeof(FORMHeader)) ||
		(memcmp(formhdr.szID,IDSTR_FORM,4)) ||
		(memcmp(formhdr.szType,IDSTR_WVQA,4))
	   )
		return FALSE;

	if (FindFORMChunk(file,IDSTR_VQHD,4)==-1)
		return FALSE;
	Plugin.fsh->ReadFile(file,&vqahdr,sizeof(VQAHeader),&read);
	if (read<sizeof(VQAHeader))
		return FALSE;
	*rate=(vqahdr.rate!=0)?(vqahdr.rate):22050;
	*channels=(WORD)((vqahdr.channels!=0)?(vqahdr.channels):1);
	*bits=(WORD)((vqahdr.bits!=0)?(vqahdr.bits):8);
	*numframes=vqahdr.numFrames;
	*version=vqahdr.version;

	chunkSize=FindFORMChunk(file,IDSTR_FINF,4);
	if (chunkSize!=-1)
	{
		if (finf!=NULL)
		{
			*finf=GlobalAlloc(GPTR,chunkSize);
			Plugin.fsh->ReadFile(file,*finf,chunkSize,&read);
			if (read<chunkSize)
			{
				GlobalFree(*finf);
				*finf=NULL;
			}
		}
		else
			Plugin.fsh->SetFilePointer(file,chunkSize,FILE_CURRENT);
	}
	else
	{
		if (finf!=NULL)
			*finf=NULL;
	}

	AlignFilePointer2(file);
	Plugin.fsh->ReadFile(file,&chunkhdr,sizeof(FORMChunkHeader),&read);
	while (
		   (memcmp(chunkhdr.szID,IDSTR_SND0,3)) && 
		   (!(Plugin.fsh->EndOfFile(file)))
		  )
	{
		Plugin.fsh->SetFilePointer(file,SWAPDWORD(chunkhdr.size),FILE_CURRENT);
		AlignFilePointer2(file);
		Plugin.fsh->ReadFile(file,&chunkhdr,sizeof(FORMChunkHeader),&read);
		if (read<sizeof(FORMChunkHeader))
			break; // ???
		if (!memcmp(chunkhdr.szID,IDSTR_VQFR,3)) // ???
			return FALSE;
	}
	if (!memcmp(chunkhdr.szID,IDSTR_SND0,4))
		*type=VQA_SND0;
	else if (!memcmp(chunkhdr.szID,IDSTR_SND1,4))
		*type=VQA_SND1;
	else if (!memcmp(chunkhdr.szID,IDSTR_SND2,4))
		*type=VQA_SND2;
	else
		return FALSE;
	*sndStart=Plugin.fsh->GetFilePointer(file)-sizeof(FORMChunkHeader);

	if (*type!=VQA_SND1)
		*size1=SWAPDWORD(chunkhdr.size);
	else
	{
		Plugin.fsh->ReadFile(file,&size,sizeof(WORD),&read);
		Plugin.fsh->SetFilePointer(file,-(LONG)read,FILE_CURRENT);
		*size1=(DWORD)size;
	}
	Plugin.fsh->SetFilePointer(file,SWAPDWORD(chunkhdr.size),FILE_CURRENT);

	chunkSize=FindFORMChunk(file,IDSTR_SND0,3);
	if (chunkSize!=-1)
	{
		if (*type!=VQA_SND1)
			*size2=chunkSize;
		else
		{
			Plugin.fsh->ReadFile(file,&size,sizeof(WORD),&read);
			*size2=(DWORD)size;
		}
	}
	else
		return FALSE;

	Plugin.fsh->SetFilePointer(file,*sndStart,FILE_BEGIN);

    return TRUE;
}

void __stdcall Seek_AUD_WS_ADPCM(FSHandle* f, DWORD pos);
void __stdcall Seek_VQA(FSHandle* f, DWORD pos);

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	BYTE   type;
	BOOL   bAUD;
	WORD   nframes,version;
	DWORD  size1,size2,sndStart;
	DWORD *finf;

    if (f==NULL)
		return FALSE;

	finf=NULL;
	sndStart=size1=size2=0L;
	nframes=version=0;
    if (
		(!(bAUD=ReadAUDHeader(f,rate,channels,bits,&type,&version,&size1,&size2))) &&
		(!ReadVQAHeader(f,rate,channels,bits,&type,&version,&nframes,&size1,&size2,&sndStart,&finf))
	   )
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	if (bAUD)
	{
		switch (type)
		{
			case AUD_IMA_ADPCM:
				Plugin.Seek=NULL;
				break;
			case AUD_WS_ADPCM:
				Plugin.Seek=Seek_AUD_WS_ADPCM;
				if (*channels==2)
					MessageBox(GetFocus(),"Stereo WS ADPCM compressed AUD!\nIt will most likely be played back incorrectly.",Plugin.Description,MB_OK | MB_ICONWARNING);
				break;
			default:
				SetLastError(PRIVEC(IDS_UNKNOWNTYPE));
				return FALSE;
		}
	}
	else
	{
		switch (type)
		{
			case VQA_SND0:
			case VQA_SND1:
				Plugin.Seek=Seek_VQA;
				break;
			case VQA_SND2:
				Plugin.Seek=NULL;
				break;
			default:
				SetLastError(PRIVEC(IDS_UNKNOWNTYPE));
				return FALSE;
		}
	}
	f->afData=GlobalAlloc(GPTR,sizeof(Data));
	if (f->afData==NULL)
	{
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
	}
	LPData(f)->sndStart=sndStart;
	LPData(f)->size1=size1;
	LPData(f)->size2=size2;
	LPData(f)->numFrames=nframes;
	LPData(f)->finf=finf;
    LPData(f)->curChunkSize=0;
	LPData(f)->curChunkOutSize=0;
	LPData(f)->Index1=0L;
	LPData(f)->Index2=0L;
	LPData(f)->Cur_Sample1=0L;
	LPData(f)->Cur_Sample2=0L;
	if ((bAUD) || (type!=VQA_SND0))
	{
		LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
		if (LPData(f)->buffer==NULL)
		{
			GlobalFree(f->afData);
			SetLastError(PRIVEC(IDS_NOBUFFER));
			return FALSE;
		}
	}
	else
		LPData(f)->buffer=NULL;
	LPData(f)->align=(*channels)*((*bits)/8);
    LPData(f)->type=type;
	LPData(f)->bAUD=bAUD;
	LPData(f)->version=version;
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

	Plugin.Seek=NULL;
    return (
			(GlobalFree(LPData(f)->finf)==NULL) &&
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

DWORD Decode_IMA_ADPCM(FSHandle* f, char* chunk, DWORD size, char* buff)
{
    DWORD  i,pcmsize,halfsize;
	BYTE   Code;
    short *pcmbuff;
	long  *pindex,*psample;

    if (f==NULL)
		return 0;

	pcmbuff=(short*)buff;
    pcmsize=0;
	if (LPData(f)->channels==2)
	{
		switch (LPData(f)->version)
		{
			case VQA_VER_1: // AUD_VER_1
			case VQA_VER_2: // AUD_VER_2
				for (i=0;i<size;i+=2)
				{
					pindex=&(LPData(f)->Index1);
					psample=&(LPData(f)->Cur_Sample1);
					Code=NIBBLE(chunk[i],0);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;

					pindex=&(LPData(f)->Index2);
					psample=&(LPData(f)->Cur_Sample2);
					Code=NIBBLE(chunk[i+1],0);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;

					pindex=&(LPData(f)->Index1);
					psample=&(LPData(f)->Cur_Sample1);
					Code=NIBBLE(chunk[i],1);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;

					pindex=&(LPData(f)->Index2);
					psample=&(LPData(f)->Cur_Sample2);
					Code=NIBBLE(chunk[i+1],1);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;
				}				
				break;
			case VQA_VER_3:
				halfsize=size/2;
				for (i=0;i<halfsize;i++)
				{
					pindex=&(LPData(f)->Index1);
					psample=&(LPData(f)->Cur_Sample1);
					Code=NIBBLE(chunk[i],0);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;

					pindex=&(LPData(f)->Index2);
					psample=&(LPData(f)->Cur_Sample2);
					Code=NIBBLE(chunk[i+halfsize],0);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;

					pindex=&(LPData(f)->Index1);
					psample=&(LPData(f)->Cur_Sample1);
					Code=NIBBLE(chunk[i],1);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;

					pindex=&(LPData(f)->Index2);
					psample=&(LPData(f)->Cur_Sample2);
					Code=NIBBLE(chunk[i+halfsize],1);
					Step_IMA_ADPCM(Code,pindex,psample);
					if (opUseClipping)
						Clip16BitSample(psample);
					if (opUseEnhancement)
						Enhance16BitSample(psample);
					*pcmbuff++=(short)(*psample);
					pcmsize+=2;
				}
				break;
			default: // ???
				break;
		}
	}
	else
	{
		pindex=&(LPData(f)->Index1);
		psample=&(LPData(f)->Cur_Sample1);
		for (i=0;i<size;i++)
		{
			Code=NIBBLE(chunk[i],0);
			Step_IMA_ADPCM(Code,pindex,psample);
			if (opUseClipping)
				Clip16BitSample(psample);
			if (opUseEnhancement)
				Enhance16BitSample(psample);
			*pcmbuff++=(short)(*psample);
			pcmsize+=2;

			Code=NIBBLE(chunk[i],1);
			Step_IMA_ADPCM(Code,pindex,psample);
			if (opUseClipping)
				Clip16BitSample(psample);
			if (opUseEnhancement)
				Enhance16BitSample(psample);
			*pcmbuff++=(short)(*psample);
			pcmsize+=2;
		}
	}

    return pcmsize;
}

void Clip8BitSample(short *sample)
{
	if (*sample>0xFF)
		*sample=0xFF;
	if (*sample<0)
		*sample=0;
}

DWORD Decode_WS_ADPCM(FSHandle *f, char *chunk, char *buff, DWORD size, DWORD outsize)
{
	BYTE *pIn,*pOut,code;
	char  count;
	DWORD read;
	WORD  dIn;
	short dOut;

	if (
		(f==NULL) ||
		(LPData(f)==NULL) ||
		(chunk==NULL) ||
		(buff==NULL) ||
		(outsize==0)
	   )
	   return 0;

	read=0;
	pIn=(BYTE*)chunk;
	pOut=(BYTE*)buff;
	dOut=0x80;
	if (size==outsize)
	{
		for (;outsize>0;outsize--)
			*pOut++=*pIn++;
		return size;
	}
	while (outsize>0)
	{
		dIn=(*pIn++);
		read++;
		dIn<<=2;
		code=HIBYTE(dIn);
		count=LOBYTE(dIn)>>2;
		switch (code)
		{
			case 2:
				if (count & 0x20)
				{
					count<<=3;
					dOut+=count>>3;
					*pOut++=(BYTE)dOut;
					outsize--;
				}
				else
				{
					for (count++;count>0;count--,outsize--,read++)
						*pOut++=*pIn++;
					dOut=pOut[-1];
				}
				break;
			case 1:
				for (count++;count>0;count--)
				{
					code=*pIn++;
					read++;
					dOut+=WSTable4bit[(code & 0x0F)];
					Clip8BitSample(&dOut);
					*pOut++=(BYTE)dOut;
					dOut+=WSTable4bit[(code>>4)];
					Clip8BitSample(&dOut);
					*pOut++=(BYTE)dOut;
					outsize-=2;
				}
				break;
			case 0:
				for (count++;count>0;count--)
				{
					code=*pIn++;
					read++;
					dOut+=WSTable2bit[(code & 0x03)];
					Clip8BitSample(&dOut);
					*pOut++=(BYTE)dOut;
					dOut+=WSTable2bit[((code>>2) & 0x03)];
					Clip8BitSample(&dOut);
					*pOut++=(BYTE)dOut;
					dOut+=WSTable2bit[((code>>4) & 0x03)];
					Clip8BitSample(&dOut);
					*pOut++=(BYTE)dOut;
					dOut+=WSTable2bit[((code>>6) & 0x03)];
					Clip8BitSample(&dOut);
					*pOut++=(BYTE)dOut;
					outsize-=4;
				}
				break;
			default:
				for (count++;count>0;count--,outsize--)
					*pOut++=(BYTE)dOut;
				break;
		}
	}
	return read;
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    AUDHeader	    audhdr;
    AUDHeaderOld    audhdrold;
    AUDChunkHeader  chunkhdr;
	FORMHeader      formhdr;
    DWORD	        datapos,curpos,size,outsize,read,start,length,walksize;
    AFile*	        node;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // DEAF
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying AUD file...");
			RFPLUGIN(f)->SetFilePointer(f,pos-sizeof(AUDChunkHeader)+4-6,FILE_BEGIN);
			RFPLUGIN(f)->ReadFile(f,&outsize,sizeof(DWORD),&read);
			if (read<sizeof(DWORD))
				return NULL; // ???
			RFPLUGIN(f)->SetFilePointer(f,-8,FILE_CURRENT);
			RFPLUGIN(f)->ReadFile(f,&size,sizeof(DWORD),&read);
			if (read<sizeof(DWORD))
				return NULL; // ???
			RFPLUGIN(f)->SetFilePointer(f,pos-sizeof(AUDChunkHeader)+4,FILE_BEGIN);
			curpos=datapos=RFPLUGIN(f)->GetFilePointer(f);
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying AUD chunks chain...");
			while (!(RFPLUGIN(f)->EndOfFile(f)))
			{
				if ((BOOL)SendMessage(hwnd,WM_GAP_ISCANCELLED,0,0))
					return NULL;
				chunkhdr.id=0L;
				chunkhdr.size=0L;
				RFPLUGIN(f)->ReadFile(f,&chunkhdr,sizeof(AUDChunkHeader),&read);
				if (read<sizeof(AUDChunkHeader))
					break; // ???
				if (chunkhdr.id!=ID_DEAF)
					break;
				curpos=RFPLUGIN(f)->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
				SendMessage(hwnd,WM_GAP_SHOWFILEPROGRESS,0,(LPARAM)f);
			}
			walksize=curpos-datapos;
			if (size==walksize)
			{
				start=pos-sizeof(AUDChunkHeader)+4-sizeof(AUDHeader);
				RFPLUGIN(f)->SetFilePointer(f,start,FILE_BEGIN);
				RFPLUGIN(f)->ReadFile(f,&audhdr,sizeof(AUDHeader),&read);
				if (read<sizeof(AUDHeader))
					return NULL;
				length=audhdr.size+sizeof(AUDHeader);
			}
			else if (outsize==walksize)
			{
				start=pos-sizeof(AUDChunkHeader)+4-sizeof(AUDHeaderOld);
				RFPLUGIN(f)->SetFilePointer(f,start,FILE_BEGIN);
				RFPLUGIN(f)->ReadFile(f,&audhdrold,sizeof(AUDHeaderOld),&read);
				if (read<sizeof(AUDHeaderOld))
					return NULL;
				length=audhdrold.size+sizeof(AUDHeaderOld);
			}
			else
				return NULL;
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"AUD file correct.");
			break;
		case 1: // FORM
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying VQA file...");
			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			RFPLUGIN(f)->ReadFile(f,&formhdr,sizeof(FORMHeader),&read);
			if (
				(read<sizeof(FORMHeader)) ||
				(memcmp(formhdr.szID,IDSTR_FORM,4)) ||
				(memcmp(formhdr.szType,IDSTR_WVQA,4))
			   )
			   return NULL;
			length=SWAPDWORD(formhdr.size)+8;
			start=pos;
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"VQA file correct.");
			break;
		default: // ???
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=start;
    node->fsLength=length;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate,size1,size2,sndStart;
    WORD   channels,bits,nframes,version;
	BYTE   type;

    if (f==NULL)
		return NULL;

    if (
		(!ReadAUDHeader(f,&rate,&channels,&bits,&type,&version,&size1,&size2)) &&
		(!ReadVQAHeader(f,&rate,&channels,&bits,&type,&version,&nframes,&size1,&size2,&sndStart,NULL))
	   )
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");

    return node;
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

DWORD GetAUDTime
(
	FSHandle* f, 
	DWORD rate, 
	WORD  channels, 
	WORD  bits, 
	BYTE  type, 
	WORD  version,
	DWORD size,
	DWORD outsize
)
{
	HWND  pwnd,hwnd;
	AUDChunkHeader chunkhdr;
	DWORD fchunks,lastchunk,datasize,hdrsize,filesize,read,time;

	switch (audTimeCalcMethod)
	{
		case ID_AH_OUTSIZE:
			if (version==AUD_VER_2)
				return MulDiv(8000,outsize,channels*bits*rate);
		case ID_FILE_SIZE:
			switch (type)
			{
				case AUD_IMA_ADPCM:
					filesize=Plugin.fsh->GetFileSize(f);
					if (version==AUD_VER_2)
						hdrsize=sizeof(AUDHeader);
					else
						hdrsize=sizeof(AUDHeaderOld);
					fchunks=(filesize-hdrsize)/(512+sizeof(AUDChunkHeader));
					lastchunk=(filesize-hdrsize)%(512+sizeof(AUDChunkHeader));
					datasize=(DWORD)512*fchunks+((lastchunk==0)?0:(lastchunk-sizeof(AUDChunkHeader)));
					return MulDiv(2000,datasize,channels*rate);
				default: // ???
					break;
			}
		case ID_AH_SIZE:
			switch (type)
			{
				case AUD_IMA_ADPCM:
					fchunks=size/(512+sizeof(AUDChunkHeader));
					lastchunk=size%(512+sizeof(AUDChunkHeader));
					datasize=(DWORD)512*fchunks+((lastchunk==0)?0:(lastchunk-sizeof(AUDChunkHeader)));
					return MulDiv(2000,datasize,channels*rate);
				default: // ???
					break;
			}
			break;
		default:
			break;
	}
	if (
		(audTimeCalcMethod!=ID_CH_SIZE) &&
		(audTimeCalcMethod!=ID_CH_OUTSIZE) &&
		(opNoWalk)
	   )
		return -1;
	hwnd=GetFocus();
	pwnd=CreateDialog(Plugin.hDllInst,"Walking",Plugin.hMainWindow,(DLGPROC)WalkingDlgProc);
	SetWindowText(pwnd,"Calculating time");
	SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETRANGE,0,MAKELPARAM(0,100));
    SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,0,0L);
	time=0;
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(AUDChunkHeader),&read);
		if (read<sizeof(AUDChunkHeader))
			break; // ???
		Plugin.fsh->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
		SendDlgItemMessage(pwnd,ID_WALKING,PBM_SETPOS,MulDiv(100,Plugin.fsh->GetFilePointer(f),Plugin.fsh->GetFileSize(f)),0L);
		SendMessage(pwnd,WM_GAP_WAITINGTOY,0,0);
		switch (audTimeCalcMethod)
		{
			case ID_CH_SIZE:
				switch (type)
				{
					case AUD_IMA_ADPCM:
						time+=MulDiv(2000,chunkhdr.size,channels*rate);
						break;
					default: // ???
						break;
				}
			default:
				time+=MulDiv(8000,chunkhdr.outsize,channels*bits*rate);
				break;
		}
	}
	SetFocus(hwnd);
    DestroyWindow(pwnd);
	UpdateWindow(hwnd);

	return time;
}

DWORD GetVQATime
(
	FSHandle* f, 
	DWORD rate, 
	WORD  channels, 
	WORD  bits, 
	BYTE  type, 
	WORD  version,
	WORD  numframes,
	DWORD size1,
	DWORD size2
)
{
	DWORD time,factor=1,size;

	switch (version)
	{
		default: // ???
		case VQA_VER_1:
		case VQA_VER_3:
			size=size1+size2*(numframes-1);
			break;
		case VQA_VER_2:
			size=size1+size2*numframes;
			break;
	}
	switch (type)
	{
		case VQA_SND2:
			factor=bits/4;
		case VQA_SND0:
		case VQA_SND1:
			return MulDiv(8000,size*factor,channels*bits*rate);
		default: // ???
			return -1;
	}

	return time;
}

DWORD __stdcall GetTime(AFile* node)
{
    FSHandle* f;
    DWORD rate,time,size1,size2,sndStart;
    WORD  channels,bits,nframes,version;
	BYTE  type;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
	time=-1;
    if (ReadAUDHeader(f,&rate,&channels,&bits,&type,&version,&size1,&size2))
		time=GetAUDTime(f,rate,channels,bits,type,version,size1,size2);
	else if (ReadVQAHeader(f,&rate,&channels,&bits,&type,&version,&nframes,&size1,&size2,&sndStart,NULL))
		time=GetVQATime(f,rate,channels,bits,type,version,nframes,size1,size2);
	Plugin.fsh->CloseFile(f);

	return time;
}

DWORD FillPCMBufferForAUD(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
	DWORD read,pcmSize,decoded;
    AUDChunkHeader chunkhdr;

	if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
	if (!(LPData(file)->bAUD))
		return 0;

	pcmSize=0L;
	*buffpos=-1;
	while (!(Plugin.fsh->EndOfFile(file)))
    {
		if (LPData(file)->curChunkSize==0)
		{
			Plugin.fsh->ReadFile(file,&chunkhdr,sizeof(AUDChunkHeader),&read);
			if (
				(read<sizeof(AUDChunkHeader)) ||
				(chunkhdr.id!=ID_DEAF)
			   )
				break; // ???
			LPData(file)->curChunkSize=chunkhdr.size;
			LPData(file)->curChunkOutSize=chunkhdr.outsize;
		}
		if (LPData(file)->curChunkOutSize<=CORRALIGN(buffsize,LPData(file)->align))
		{
			Plugin.fsh->ReadFile(file,LPData(file)->buffer,LPData(file)->curChunkSize,&read);
			switch (LPData(file)->type)
			{
				case AUD_IMA_ADPCM:
					decoded=Decode_IMA_ADPCM(file,LPData(file)->buffer,read,buffer+pcmSize);
					break;
				case AUD_WS_ADPCM:
					Decode_WS_ADPCM(file,LPData(file)->buffer,buffer+pcmSize,LPData(file)->curChunkSize,LPData(file)->curChunkOutSize);
					decoded=LPData(file)->curChunkOutSize;
					break;
				default: // ???
					return 0;
			}
			pcmSize+=decoded;
			buffsize-=decoded;
			LPData(file)->curChunkSize=0; // ???
		}
		else
			break;
    }

	return pcmSize;
}

DWORD FillPCMBufferForVQA(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
	DWORD      read,pcmSize,decoded,chunkSize;
	SND1Header sndhdr;

	if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
	if (LPData(file)->bAUD)
		return 0L;

	pcmSize=0L;
	*buffpos=-1;
	while (!(Plugin.fsh->EndOfFile(file)))
    {
		if (LPData(file)->curChunkSize==0)
		{
			chunkSize=FindFORMChunk(file,IDSTR_SND0,3);
			if (chunkSize==-1)
				return pcmSize;
			switch (LPData(file)->type)
			{
				case VQA_SND0:
					LPData(file)->curChunkSize=chunkSize;
					LPData(file)->curChunkOutSize=LPData(file)->curChunkSize;
					break;
				case VQA_SND1:
					Plugin.fsh->ReadFile(file,&sndhdr,sizeof(SND1Header),&read);
					LPData(file)->curChunkSize=sndhdr.size;
					LPData(file)->curChunkOutSize=sndhdr.outSize;
					break;
				case VQA_SND2:
					LPData(file)->curChunkSize=chunkSize;
					LPData(file)->curChunkOutSize=(LPData(file)->bits/4)*LPData(file)->curChunkSize;
					break;
				default: // ???
					return 0L;
			}
		}
		if (LPData(file)->curChunkOutSize<=CORRALIGN(buffsize,LPData(file)->align))
		{
			switch (LPData(file)->type)
			{
				case VQA_SND0:
					Plugin.fsh->ReadFile(file,buffer+pcmSize,LPData(file)->curChunkSize,&decoded);
					break;
				case VQA_SND1:
					Plugin.fsh->ReadFile(file,LPData(file)->buffer,LPData(file)->curChunkSize,&read);
					Decode_WS_ADPCM(file,LPData(file)->buffer,buffer+pcmSize,LPData(file)->curChunkSize,LPData(file)->curChunkOutSize);
					decoded=LPData(file)->curChunkOutSize;
					break;
				case VQA_SND2:
					Plugin.fsh->ReadFile(file,LPData(file)->buffer,LPData(file)->curChunkSize,&read);
					decoded=Decode_IMA_ADPCM(file,LPData(file)->buffer,read,buffer+pcmSize);
					break;
				default: // ???
					return 0L;
			}
			pcmSize+=decoded;
			buffsize-=decoded;
			LPData(file)->curChunkSize=0; // ???
		}
		else
			break;
    }

	return pcmSize;
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD pcmSize;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	if (LPData(file)->bAUD)
		pcmSize=FillPCMBufferForAUD(file,buffer,buffsize,buffpos);
	else
		pcmSize=FillPCMBufferForVQA(file,buffer,buffsize,buffpos);

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
BYTE   ibType;
WORD   ibChannels,ibBits,ibVersion;
BOOL   ibAUD;

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
    char str[512],type[20],ver[20];

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
			if (ibAUD)
			{
				switch (ibType)
				{
					case AUD_IMA_ADPCM:
						lstrcpy(type,"IMA ADPCM");
						break;
					case AUD_WS_ADPCM:
						lstrcpy(type,"WS ADPCM");
						break;
					default:
						lstrcpy(type,"Unknown");
						break;
				}
				lstrcpy(ver,(ibVersion==AUD_VER_2)?"new":"old");
			}
			else
			{
				switch (ibType)
				{
					case VQA_SND0:
						lstrcpy(type,"None");
						break;
					case VQA_SND1:
						lstrcpy(type,"WS ADPCM");
						break;
					case VQA_SND2:
						lstrcpy(type,"IMA ADPCM");
						break;
					default:
						lstrcpy(type,"Unknown");
						break;
				}
				wsprintf(ver,"version %u",ibVersion);
			}
			wsprintf(str,"Format: %s (%s)\r\n"
						 "Compression: %s (%u)\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 (ibAUD)?"AUD":"VQA",
						 ver,
						 type,
						 ibType,
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

void GrayButtons(HWND hwnd, int method)
{
	if (
		(method!=ID_WALK) && // when dialog is active
		(method!=ID_CH_OUTSIZE) && // when initializing dialog
		(method!=ID_CH_SIZE)	   // --//--
	   )
	{
		EnableWindow(GetDlgItem(hwnd,ID_CH_OUTSIZE),FALSE);
		EnableWindow(GetDlgItem(hwnd,ID_CH_SIZE),FALSE);
		EnableWindow(GetDlgItem(hwnd,ID_WALKFRAME),FALSE);
		EnableWindow(GetDlgItem(hwnd,ID_NOWALK),TRUE);
	}
	else
	{
		EnableWindow(GetDlgItem(hwnd,ID_CH_OUTSIZE),TRUE);
		EnableWindow(GetDlgItem(hwnd,ID_CH_SIZE),TRUE);
		EnableWindow(GetDlgItem(hwnd,ID_WALKFRAME),TRUE);
		EnableWindow(GetDlgItem(hwnd,ID_NOWALK),FALSE);
		SetCheckBox(hwnd,ID_NOWALK,FALSE);
	}
}

void SetDefaults(HWND hwnd)
{
	CheckRadioButton(hwnd,ID_AH_OUTSIZE,ID_WALK,ID_AH_OUTSIZE);
	CheckRadioButton(hwnd,ID_CH_OUTSIZE,ID_CH_SIZE,ID_CH_OUTSIZE);
	SetCheckBox(hwnd,ID_NOWALK,FALSE);
	SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
	SetCheckBox(hwnd,ID_USEENHANCEMENT,FALSE);
	GrayButtons(hwnd,ID_AH_OUTSIZE);
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
			if (
				(audTimeCalcMethod>=ID_AH_OUTSIZE) &&
				(audTimeCalcMethod<=ID_FILE_SIZE)
			   )
			{
				CheckRadioButton(hwnd,ID_AH_OUTSIZE,ID_WALK,audTimeCalcMethod);
				CheckRadioButton(hwnd,ID_CH_OUTSIZE,ID_CH_SIZE,ID_CH_OUTSIZE);
			}
			else
			{
				CheckRadioButton(hwnd,ID_AH_OUTSIZE,ID_WALK,ID_WALK);
				CheckRadioButton(hwnd,ID_CH_OUTSIZE,ID_CH_SIZE,audTimeCalcMethod);
			}
			SetCheckBox(hwnd,ID_NOWALK,opNoWalk);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			SetCheckBox(hwnd,ID_USEENHANCEMENT,opUseEnhancement);
			GrayButtons(hwnd,audTimeCalcMethod);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetDefaults(hwnd);
					break;
				case ID_AH_OUTSIZE:
				case ID_AH_SIZE:
				case ID_FILE_SIZE:
				case ID_WALK:
					GrayButtons(hwnd,LOWORD(wParam));
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					audTimeCalcMethod=GetRadioButton(hwnd,ID_AH_OUTSIZE,ID_WALK);
					if (audTimeCalcMethod==ID_WALK)
						audTimeCalcMethod=GetRadioButton(hwnd,ID_CH_OUTSIZE,ID_CH_SIZE);
					opNoWalk=GetCheckBox(hwnd,ID_NOWALK);
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
	DWORD size1,size2,sndStart;
	WORD  nframes;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (
		(!(ibAUD=ReadAUDHeader(f,&ibRate,&ibChannels,&ibBits,&ibType,&ibVersion,&size1,&size2))) &&
		(!ReadVQAHeader(f,&ibRate,&ibChannels,&ibBits,&ibType,&ibVersion,&nframes,&size1,&size2,&sndStart,NULL))
	   )
    {
		wsprintf(str,"This is not %s file or VQA with soundtrack.",Plugin.afID);
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

void __stdcall Seek_AUD_WS_ADPCM(FSHandle* f, DWORD pos)
{
    DWORD datapos,read;
	AUDChunkHeader chunkhdr;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;
	if (!(LPData(f)->bAUD))
		return;

    datapos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
	LPData(f)->curChunkSize=0L;
	LPData(f)->curChunkOutSize=0L;
	Plugin.fsh->SetFilePointer(f,(LPData(f)->version==AUD_VER_2)?sizeof(AUDHeader):sizeof(AUDHeaderOld),FILE_BEGIN);
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(AUDChunkHeader),&read);
		if (read<sizeof(AUDChunkHeader))
			return; // ???
		if (chunkhdr.outsize>datapos)
		{
			LPData(f)->curChunkSize=chunkhdr.size;
			LPData(f)->curChunkOutSize=chunkhdr.outsize;
			break;
		}
		datapos-=chunkhdr.outsize;
		Plugin.fsh->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
	}
}

void __stdcall Seek_VQA(FSHandle* f, DWORD pos)
{
	DWORD datapos,read,index;
	FORMChunkHeader chunkhdr;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;
	if (LPData(f)->bAUD)
		return;
	if (LPData(f)->type==VQA_SND2)
		return;

    datapos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
	LPData(f)->curChunkSize=0L;
	LPData(f)->curChunkOutSize=0L;
	if (datapos<LPData(f)->size1)
	{
		Plugin.fsh->SetFilePointer(f,LPData(f)->sndStart,FILE_BEGIN);
		return;
	}
	if (LPData(f)->finf!=NULL)
	{
		index=(datapos-(LPData(f)->size1))/(LPData(f)->size2);
		if ((LPData(f)->version==VQA_VER_2) && (index==0))
		{
			Plugin.fsh->SetFilePointer(f,LPData(f)->sndStart,FILE_BEGIN);
			Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(FORMChunkHeader),&read);
			Plugin.fsh->SetFilePointer(f,SWAPDWORD(chunkhdr.size),FILE_CURRENT);
			return;
		}
		if (index<LPData(f)->numFrames)
			Plugin.fsh->SetFilePointer(f,2*CORRLARGE(LPData(f)->finf[index]),FILE_BEGIN);
		return;
	}
	Plugin.fsh->SetFilePointer(f,LPData(f)->sndStart,FILE_BEGIN);
	AlignFilePointer2(f);
	Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(FORMChunkHeader),&read);
	Plugin.fsh->SetFilePointer(f,SWAPDWORD(chunkhdr.size),FILE_CURRENT);
	datapos-=LPData(f)->size1;
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		chunkhdr.size=FindFORMChunk(f,IDSTR_SND0,3);
		if (chunkhdr.size==-1)
			break;
		if ((LPData(f)->size2)>datapos)
		{
			Plugin.fsh->SetFilePointer(f,-(LONG)sizeof(FORMChunkHeader),FILE_CURRENT);
			break;
		}
		datapos-=LPData(f)->size2;
		Plugin.fsh->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
	}
}

SearchPattern patterns[]=
{
	{4,IDSTR_DEAF},
	{4,IDSTR_FORM}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX AUD Audio File Decoder",
    "v0.98",
    "Westwood AUD Audio Files (*.AUD)\0*.aud\0"
	"Westwood VQA Video Files (*.VQA)\0*.vqa\0",
    "AUD",
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
