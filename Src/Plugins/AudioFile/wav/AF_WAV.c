/*
 * WAV plug-in source code
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
#include <mmreg.h>
#include <msacm.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_WAV.h"
#include "resource.h"

typedef struct tagData
{
    WORD  tag,channels,bits,align;
    DWORD rate;
    DWORD dataStart,
		  dataLength,
		  dataRemained,
		  outSize;

	HACMSTREAM hACMStream;

	char* buffer;
	DWORD bufferSize;
} Data;

#define LPData(f) ((Data*)((f)->afData))

#define BUFFERSIZE (64000ul)

AFPlugin Plugin;

void AlignFilePointer2(FSHandle* file)
{
	if ((Plugin.fsh->GetFilePointer(file))%2)
		Plugin.fsh->SetFilePointer(file,1,FILE_CURRENT);
}

BOOL ReadFMTChunk(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits, WORD* tag, WORD* align, LPWAVEFORMATEX* ppwfex)
{
    DWORD read;
    RIFFHeader riffhdr;
    ChunkHeader chunkhdr;
    WAVEFORMATEX wfex,*pwfex;

    Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(f,&riffhdr,sizeof(RIFFHeader),&read);
    if (read<sizeof(RIFFHeader))
		return FALSE;
    if (
		(memcmp(riffhdr.riffid,IDSTR_RIFF,4)) ||
		(memcmp(riffhdr.rifftype,IDSTR_WAVE,4))
       )
		return FALSE;
	AlignFilePointer2(f);
    Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(ChunkHeader),&read);
	if (read<sizeof(ChunkHeader))
		return FALSE;
    while ((!(Plugin.fsh->EndOfFile(f))) && (memcmp(chunkhdr.id,IDSTR_fmt,4)))
    {
		Plugin.fsh->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
		AlignFilePointer2(f);
		Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(ChunkHeader),&read);
		if (read<sizeof(ChunkHeader))
			return FALSE;
    }
    if (memcmp(chunkhdr.id,IDSTR_fmt,4))
		return FALSE;
	if (ppwfex!=NULL)
	{
		*ppwfex=(LPWAVEFORMATEX)LocalAlloc(LPTR,chunkhdr.size);
		Plugin.fsh->ReadFile(f,*ppwfex,chunkhdr.size,&read);
		if (read<chunkhdr.size)
		{
			LocalFree(*ppwfex);
			return FALSE;
		}
		pwfex=*ppwfex;
	}
	else
	{
		Plugin.fsh->ReadFile(f,&wfex,min(chunkhdr.size,sizeof(wfex)),&read);
		if (read<min(chunkhdr.size,sizeof(wfex)))
			return FALSE;
		pwfex=&wfex;
	}
    *rate=pwfex->nSamplesPerSec;
    *channels=pwfex->nChannels;
    *bits=pwfex->wBitsPerSample;
    *tag=pwfex->wFormatTag;
	*align=pwfex->nBlockAlign;

    return TRUE;
}

DWORD ReadFactChunk(FSHandle* f)
{
    DWORD read,outsize;
    ChunkHeader chunkhdr={0};

    Plugin.fsh->SetFilePointer(f,sizeof(RIFFHeader),FILE_BEGIN);
	AlignFilePointer2(f);
    Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(ChunkHeader),&read);
	if (read<sizeof(ChunkHeader))
		return -1;
    while ((!(Plugin.fsh->EndOfFile(f))) && (memcmp(chunkhdr.id,IDSTR_fact,4)))
    {
		Plugin.fsh->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
		AlignFilePointer2(f);
		Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(ChunkHeader),&read);
		if (read<sizeof(ChunkHeader))
			return -1;
    }
    if (memcmp(chunkhdr.id,IDSTR_fact,4))
		return -1;
    Plugin.fsh->ReadFile(f,&outsize,sizeof(DWORD),&read);
	if (read<sizeof(DWORD))
		return -1;
	else
		return outsize;
}

BOOL FindDataChunk(FSHandle* f, DWORD* size)
{
	DWORD read;
	ChunkHeader chunkhdr;

	Plugin.fsh->SetFilePointer(f,sizeof(RIFFHeader),FILE_BEGIN);
	AlignFilePointer2(f);
    Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(ChunkHeader),&read);
	if (read<sizeof(ChunkHeader))
		return FALSE;
    while ((!(Plugin.fsh->EndOfFile(f))) && (memcmp(chunkhdr.id,IDSTR_data,4)))
    {
		Plugin.fsh->SetFilePointer(f,chunkhdr.size,FILE_CURRENT);
		AlignFilePointer2(f);
		Plugin.fsh->ReadFile(f,&chunkhdr,sizeof(ChunkHeader),&read);
		if (read<sizeof(ChunkHeader))
			return FALSE;
    }
    if (memcmp(chunkhdr.id,IDSTR_data,4))
		return FALSE;
	else
	{
		*size=chunkhdr.size;
		return TRUE;
	}
}

void __stdcall Seek(FSHandle* f, DWORD pos);

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
    WORD  tag,align;
	DWORD datasize,outsize;
	HACMSTREAM hACMStream;
	LPWAVEFORMATEX pwfex=NULL;
	WAVEFORMATEX wfexPCM;

    if (f==NULL)
		return FALSE;

    if (!ReadFMTChunk(f,rate,channels,bits,&tag,&align,&pwfex))
    {
		LocalFree(pwfex);
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
    switch (tag)
    {
		case WAVE_FORMAT_PCM:
			hACMStream=NULL;
			break;
		default:
			wfexPCM.wFormatTag=WAVE_FORMAT_PCM;
			if (acmFormatSuggest(NULL,pwfex,&wfexPCM,sizeof(wfexPCM),ACM_FORMATSUGGESTF_WFORMATTAG)!=MMSYSERR_NOERROR)
			{
				LocalFree(pwfex);
				SetLastError(PRIVEC(IDS_NODECODER));
				return FALSE;
			}
			*rate=wfexPCM.nSamplesPerSec;
			*channels=wfexPCM.nChannels;
			*bits=wfexPCM.wBitsPerSample;
			if (acmStreamOpen(&hACMStream,NULL,pwfex,&wfexPCM,NULL,0,0,0)!=MMSYSERR_NOERROR)
			{
				LocalFree(pwfex);
				SetLastError(PRIVEC(IDS_ACMOPENFAILED));
				return FALSE;
			}
    }
	LocalFree(pwfex);
	switch (tag)
	{
		case WAVE_FORMAT_PCM:
			Plugin.Seek=Seek;
			outsize=0;
			break;
		default:
			outsize=ReadFactChunk(f);
			if (outsize==-1)
				Plugin.Seek=NULL;
			else
				Plugin.Seek=Seek;
	}
    if (!FindDataChunk(f,&datasize))
	{
		acmStreamClose(hACMStream,0);
		SetLastError(PRIVEC(IDS_NODATACHUNK));
		return FALSE;
	}
    f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		acmStreamClose(hACMStream,0);
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
	switch (tag)
	{
		case WAVE_FORMAT_PCM:
			LPData(f)->buffer=NULL;
			LPData(f)->bufferSize=0;
			break;
		default:
			LPData(f)->bufferSize=CORRALIGN(BUFFERSIZE,align);
			if (LPData(f)->bufferSize==0)
				LPData(f)->bufferSize=align;
			LPData(f)->buffer=(char*)GlobalAlloc(GPTR,LPData(f)->bufferSize);
			if (LPData(f)->buffer==NULL)
			{
				GlobalFree(f->afData);
				acmStreamClose(hACMStream,0);
				SetLastError(PRIVEC(IDS_NOBUFFER));
				return FALSE;
			}
	}
	LPData(f)->outSize=outsize;
	LPData(f)->hACMStream=hACMStream;
    LPData(f)->dataStart=Plugin.fsh->GetFilePointer(f);
    LPData(f)->dataLength=datasize;
    LPData(f)->dataRemained=datasize;
	LPData(f)->tag=tag;
    LPData(f)->align=align;
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
			(acmStreamClose(LPData(f)->hACMStream,0)==MMSYSERR_NOERROR) &&
			(GlobalFree(LPData(f)->buffer)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  rate;
	WORD   channels,bits,tag,align;

    if (f==NULL)
		return NULL;

    if (!ReadFMTChunk(f,&rate,&channels,&bits,&tag,&align,NULL))
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
    DWORD rate,filesize,outsize,time;
    WORD  channels,bits,tag,align;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

    if (!ReadFMTChunk(f,&rate,&channels,&bits,&tag,&align,NULL))
    {
		Plugin.fsh->CloseFile(f);
		return -1;
    }
    switch (tag)
	{
		case WAVE_FORMAT_PCM:
			filesize=Plugin.fsh->GetFileSize(f);
			time=MulDiv(1000,filesize-sizeof(RIFFHeader)-sizeof(ChunkHeader)-sizeof(FMTChunk),align*rate);
			break;
		default:
			outsize=ReadFactChunk(f);
			if (outsize!=-1)
				time=MulDiv(1000,outsize,rate);
			else
				time=-1;
	}
	Plugin.fsh->CloseFile(f);

	return time;
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD pcmSize,toRead,read,toDecode;
	ACMSTREAMHEADER acmshdr={0};

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	pcmSize=0;
    switch (LPData(file)->tag)
    {
		case WAVE_FORMAT_PCM:
			toRead=min(buffsize,LPData(file)->dataRemained);
			toRead=CORRALIGN(toRead,LPData(file)->align);
			Plugin.fsh->ReadFile(file,buffer,toRead,&pcmSize);
			LPData(file)->dataRemained-=pcmSize;
			break;
		default:
			while (1)
			{
				acmStreamSize(LPData(file)->hACMStream,buffsize,&toDecode,ACM_STREAMSIZEF_DESTINATION);
				toRead=min(min(LPData(file)->bufferSize,LPData(file)->dataRemained),toDecode);
				if (toRead==0)
					break;
				Plugin.fsh->ReadFile(file,LPData(file)->buffer,toRead,&read);
				if (read==0L)
					break; // ???
				LPData(file)->dataRemained-=read;
				acmshdr.cbStruct=sizeof(ACMSTREAMHEADER);
				acmshdr.fdwStatus=0;
				acmshdr.dwUser=0;
				acmshdr.pbSrc=LPData(file)->buffer;
				acmshdr.cbSrcLength=read;
				acmshdr.cbSrcLengthUsed=0;
				acmshdr.dwSrcUser=0;
				acmshdr.pbDst=buffer+pcmSize;
				acmshdr.cbDstLength=buffsize;
				acmshdr.cbDstLengthUsed=0;
				acmshdr.dwDstUser=0;
				if (acmStreamPrepareHeader(LPData(file)->hACMStream,&acmshdr,0L)!=MMSYSERR_NOERROR)
					break;
				if (acmStreamConvert(LPData(file)->hACMStream,&acmshdr,ACM_STREAMCONVERTF_BLOCKALIGN)!=MMSYSERR_NOERROR)
				{
					acmStreamUnprepareHeader(LPData(file)->hACMStream,&acmshdr,0L);
					break;
				}
				if (acmshdr.cbSrcLengthUsed<acmshdr.cbSrcLength)
				{
					acmshdr.fdwStatus^=ACMSTREAMHEADER_STATUSF_DONE;
					acmStreamConvert(LPData(file)->hACMStream,&acmshdr,0L);
				}
				acmStreamUnprepareHeader(LPData(file)->hACMStream,&acmshdr,0L);
				pcmSize+=acmshdr.cbDstLengthUsed;
				buffsize-=acmshdr.cbDstLengthUsed;
				if (buffsize==0)
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
DWORD  ibSize,ibRate,ibTime,ibOutRate;
WORD   ibChannels,ibBits,ibTag,ibAlign,ibOutChannels,ibOutBits,ibOutTag;

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
    char str[1024];
	ACMFORMATTAGDETAILS aftd={0};

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
			aftd.cbStruct=sizeof(aftd);
			aftd.dwFormatTag=ibTag;
			if (acmFormatTagDetails(NULL,&aftd,ACM_FORMATTAGDETAILSF_FORMATTAG)!=MMSYSERR_NOERROR)
				lstrcpy(aftd.szFormatTag,"Unknown");
			wsprintf(str,"Format: %s (%u)\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit\r\n"
						 "Block Align: %u",
						 aftd.szFormatTag,
						 ibTag,
						 ibChannels,
						 (ibChannels==4)?"(quadro)":((ibChannels==2)?"(stereo)":((ibChannels==1)?"(mono)":"")),
						 ibRate,
						 ibBits,
						 ibAlign
					);
			SetDlgItemText(hwnd,ID_DATAFMT,str);
			if (
				(ibOutTag==0) ||
				(ibOutChannels==0) ||
				(ibOutRate==0) ||
				(ibOutBits==0)
			   )
				lstrcpy(str,"None suggested.\r\nPlayback impossible.");
			else
				wsprintf(str,"Format: PCM (%u)\r\n"
							 "Channels: %u %s\r\n"
							 "Sample Rate: %lu Hz\r\n"
							 "Resolution: %u-bit",
							 ibOutTag,
							 ibOutChannels,
							 (ibOutChannels==4)?"(quadro)":((ibOutChannels==2)?"(stereo)":((ibOutChannels==1)?"(mono)":"")),
							 ibOutRate,
							 ibOutBits
						);
			SetDlgItemText(hwnd,ID_OUTFMT,str);
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
	LPWAVEFORMATEX pwfex=NULL;
	WAVEFORMATEX wfexPCM;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadFMTChunk(f,&ibRate,&ibChannels,&ibBits,&ibTag,&ibAlign,&pwfex))
    {
		LocalFree(pwfex);
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
	wfexPCM.wFormatTag=WAVE_FORMAT_PCM;
	if (acmFormatSuggest(NULL,pwfex,&wfexPCM,sizeof(wfexPCM),ACM_FORMATSUGGESTF_WFORMATTAG)!=MMSYSERR_NOERROR)
	{
		ibOutTag=0;
		ibOutRate=0;
		ibOutChannels=0;
		ibOutBits=0;
	}
	else
	{
		ibOutTag=WAVE_FORMAT_PCM;
		ibOutRate=wfexPCM.nSamplesPerSec;
		ibOutChannels=wfexPCM.nChannels;
		ibOutBits=wfexPCM.wBitsPerSample;
	}
	LocalFree(pwfex);
    DialogBox(Plugin.hDllInst,"InfoBox",hwnd,InfoBoxDlgProc);
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    RIFFHeader riffhdr;
    DWORD      read,rate;
    WORD       channels,bits,tag,align;
    AFile     *node;
    FSHandle   file;

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying WAV file...");
    RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
    RFPLUGIN(f)->ReadFile(f,&riffhdr,sizeof(RIFFHeader),&read);
	if (read<sizeof(RIFFHeader))
		return NULL;
    file.rf=f;
    file.start=pos;
    file.length=riffhdr.riffsize+8;
    file.node=NULL;
    file.afData=NULL;
    if (!ReadFMTChunk(&file,&rate,&channels,&bits,&tag,&align,NULL))
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=riffhdr.riffsize+8;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"WAV file correct.");

    return node;
}

void __stdcall Seek(FSHandle* f, DWORD pos)
{
    DWORD filepos;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

	switch (LPData(f)->tag)
	{
		case WAVE_FORMAT_PCM:
			filepos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
			break;
		default:
			filepos=MulDiv(pos,(LPData(f)->rate),1000);
			filepos=MulDiv(LPData(f)->dataLength,filepos,LPData(f)->outSize);
			acmStreamReset(LPData(f)->hACMStream,0); // ???
	}
	Plugin.fsh->SetFilePointer(f,LPData(f)->dataStart+CORRALIGN(filepos,LPData(f)->align),FILE_BEGIN);
	if (filepos<=LPData(f)->dataLength)
		LPData(f)->dataRemained=LPData(f)->dataLength-filepos;
	else
		LPData(f)->dataRemained=0;
}

void __stdcall Init(void)
{
	DisableThreadLibraryCalls(Plugin.hDllInst);
}

SearchPattern patterns[]=
{
	{4,IDSTR_RIFF}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX WAV Audio File Plug-In",
    "v1.01",
    "Microsoft Waveform Audio Files (*.WAV)\0*.wav\0",
    "WAV",
    1,
    patterns,
    NULL,
    NULL,
    About,
    Init,
    NULL,
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
