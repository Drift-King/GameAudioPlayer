/*
 * Game Audio Player source code: conversion to .WAV
 *
 * Copyright (C) 1998-2000 ANX Software
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

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Progress.h"
#include "Errors.h"
#include "Misc.h"
#include "WAV.h"
#include "FSHandler.h"
#include "Convert.h"

void CorrectOddPos(HANDLE file)
{
	BYTE  pad=0;
	DWORD written;

	if (SetFilePointer(file,0,NULL,FILE_CURRENT)%2)
		WriteFile(file,&pad,1,&written,NULL);
}

void ConvertNode(HWND hwnd, AFile* node, const char* fname, WORD tag)
{
    FSHandle* file;
    HANDLE wavfile;
    RIFFHeader riffhdr;
	ChunkHeader chunkhdr;
    DWORD riffsize,factsize,datasize,written,rate,buffpos,pcmsize,dstsize,sizeToFill,sizeFilled;
	DWORD pos_riffsize,pos_factsize,pos_datasize;
    WORD  channels,bits;
    char  str[MAX_PATH+100],*pcmBuffer=NULL,*dstBuffer=NULL;
	LPWAVEFORMATEX pwfex;
	ACMFORMATTAGDETAILS aftd={0};
	MMRESULT mmr;
	WAVEFORMATEX wfexPCM;
	HACMSTREAM hACMStream;
	ACMSTREAMHEADER acmshdr;

    if ((file=FSOpenForPlayback(hwnd,node,&rate,&channels,&bits))==NULL)
		return;
	wfexPCM.wFormatTag=WAVE_FORMAT_PCM;
	wfexPCM.nChannels=channels;
	wfexPCM.nSamplesPerSec=rate;
	wfexPCM.wBitsPerSample=bits;
	wfexPCM.cbSize=0;
	wfexPCM.nBlockAlign=channels*(bits/8);
	wfexPCM.nAvgBytesPerSec=rate*wfexPCM.nBlockAlign;
	switch (tag)
	{
		case WAVE_FORMAT_PCM:
			pwfex=NULL; // ???
			hACMStream=NULL;
			dstBuffer=NULL; // ???
			pcmBuffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
			break;
		default:
			aftd.cbStruct=sizeof(aftd);
			aftd.dwFormatTag=tag;
			mmr=acmFormatTagDetails(NULL,&aftd,ACM_FORMATTAGDETAILSF_LARGESTSIZE);
			if (mmr!=MMSYSERR_NOERROR)
			{
				AFPLUGIN(node)->ShutdownPlayback(file);
				FSCloseFile(file);
				wsprintf(str,"Failed to get details for wave format tag: 0x%X",tag);
				ReportMMError(hwnd,mmr,str);
				return;
			}
			pwfex=(LPWAVEFORMATEX)LocalAlloc(LPTR,aftd.cbFormatSize);
			pwfex->wFormatTag=tag;
			mmr=acmFormatSuggest(NULL,&wfexPCM,pwfex,aftd.cbFormatSize,ACM_FORMATSUGGESTF_WFORMATTAG);
			if (mmr!=MMSYSERR_NOERROR)
			{
				LocalFree(pwfex);
				AFPLUGIN(node)->ShutdownPlayback(file);
				FSCloseFile(file);
				wsprintf(str,"No format suggested for wave format tag: 0x%X",tag);
				ReportMMError(hwnd,mmr,str);
				return;
			}
			mmr=acmStreamOpen(&hACMStream,NULL,&wfexPCM,pwfex,NULL,0,0,ACM_STREAMOPENF_NONREALTIME);
			if (mmr!=MMSYSERR_NOERROR)
			{
				LocalFree(pwfex);
				AFPLUGIN(node)->ShutdownPlayback(file);
				FSCloseFile(file);
				wsprintf(str,"Failed to open conversion stream for wave format tag: 0x%X",tag);
				ReportMMError(hwnd,mmr,str);
				return;
			}
			if (acmStreamSize(hACMStream,BUFFERSIZE,&dstsize,ACM_STREAMSIZEF_SOURCE)!=MMSYSERR_NOERROR)
				dstsize=BUFFERSIZE;
			pcmBuffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
			dstBuffer=(char*)GlobalAlloc(GPTR,dstsize);
			memset(&acmshdr,0x00,sizeof(acmshdr)); // ???
			acmshdr.cbStruct=sizeof(ACMSTREAMHEADER);
			acmshdr.fdwStatus=0;
			acmshdr.pbSrc=pcmBuffer;
			acmshdr.cbSrcLength=BUFFERSIZE;
			acmshdr.cbSrcLengthUsed=0;
			acmshdr.pbDst=dstBuffer;
			acmshdr.cbDstLength=dstsize;
			acmshdr.cbDstLengthUsed=0;
			mmr=acmStreamPrepareHeader(hACMStream,&acmshdr,0L);
			if (mmr!=MMSYSERR_NOERROR)
			{
				GlobalFree(dstBuffer);
				GlobalFree(pcmBuffer);
				acmStreamClose(hACMStream,0);
				LocalFree(pwfex);
				AFPLUGIN(node)->ShutdownPlayback(file);
				FSCloseFile(file);
				ReportMMError(hwnd,mmr,"Failed to prepare conversion stream header.");
				return;
			}
			acmshdr.cbSrcLength=0;
	}
	if (!EnsureDirPresence(fname))
	{
		if (hACMStream!=NULL)
		{
			acmStreamUnprepareHeader(hACMStream,&acmshdr,0L);
			acmStreamClose(hACMStream,0);
		}
		GlobalFree(dstBuffer);
		GlobalFree(pcmBuffer);
		LocalFree(pwfex);
		AFPLUGIN(node)->ShutdownPlayback(file);
		FSCloseFile(file);
		return;
	}
    wavfile=CreateFile(fname,
					   GENERIC_WRITE,
					   FILE_SHARE_READ,
					   NULL,
					   CREATE_ALWAYS,
					   FILE_ATTRIBUTE_NORMAL,
					   NULL
					  );
    if (wavfile==INVALID_HANDLE_VALUE)
    {
		if (hACMStream!=NULL)
		{
			acmStreamUnprepareHeader(hACMStream,&acmshdr,0L);
			acmStreamClose(hACMStream,0);
		}
		GlobalFree(dstBuffer);
		GlobalFree(pcmBuffer);
		LocalFree(pwfex);
		AFPLUGIN(node)->ShutdownPlayback(file);
		FSCloseFile(file);
		wsprintf(str,"Cannot open WAV file: %s",fname);
		ReportError(hwnd,str,NULL);
		return;
    }
    ShowProgressHeaderMsg(fname);
	datasize=0;
	factsize=0;
    ShowProgressStateMsg("Writing RIFF header...");
    SetFilePointer(wavfile,0,NULL,FILE_BEGIN);
    lstrcpy(riffhdr.riffid,IDSTR_RIFF);
    lstrcpy(riffhdr.rifftype,IDSTR_WAVE);
	riffhdr.riffsize=0;
	WriteFile(wavfile,&riffhdr,sizeof(RIFFHeader),&written,NULL);
	pos_riffsize=SetFilePointer(wavfile,0,NULL,FILE_CURRENT)-sizeof(riffhdr.rifftype)-sizeof(riffhdr.riffsize);
	CorrectOddPos(wavfile);
	ShowProgressStateMsg("Writing fmt chunk...");
    lstrcpy(chunkhdr.id,IDSTR_fmt);
	switch (tag)
	{
		case WAVE_FORMAT_PCM:
			chunkhdr.size=sizeof(wfexPCM);
			WriteFile(wavfile,&chunkhdr,sizeof(chunkhdr),&written,NULL);
			WriteFile(wavfile,&wfexPCM,chunkhdr.size,&written,NULL);
			CorrectOddPos(wavfile);
			break;
		default:
			chunkhdr.size=aftd.cbFormatSize;
			WriteFile(wavfile,&chunkhdr,sizeof(chunkhdr),&written,NULL);
			WriteFile(wavfile,pwfex,chunkhdr.size,&written,NULL);
			CorrectOddPos(wavfile);
			lstrcpy(chunkhdr.id,IDSTR_fact);
			chunkhdr.size=sizeof(factsize);
			WriteFile(wavfile,&chunkhdr,sizeof(chunkhdr),&written,NULL);
			pos_factsize=SetFilePointer(wavfile,0,NULL,FILE_CURRENT);
			WriteFile(wavfile,&factsize,sizeof(factsize),&written,NULL);
			CorrectOddPos(wavfile);
	}
    lstrcpy(chunkhdr.id,IDSTR_data);
	chunkhdr.size=datasize;
	WriteFile(wavfile,&chunkhdr,sizeof(chunkhdr),&written,NULL);
	pos_datasize=SetFilePointer(wavfile,0,NULL,FILE_CURRENT)-sizeof(datasize);
    while (1)
    {
		if (IsCancelled())
			break;
		switch (tag)
		{
			case WAVE_FORMAT_PCM:
				sizeToFill=BUFFERSIZE;
				sizeFilled=0;
				break;
			default:
				if (acmshdr.cbSrcLengthUsed!=0L)
				{
					memmove(pcmBuffer,pcmBuffer+acmshdr.cbSrcLengthUsed,acmshdr.cbSrcLength-acmshdr.cbSrcLengthUsed);
					acmshdr.cbSrcLength-=acmshdr.cbSrcLengthUsed;
				}
				if (acmshdr.cbSrcLength<BUFFERSIZE)
					sizeToFill=BUFFERSIZE-acmshdr.cbSrcLength;
				sizeFilled=acmshdr.cbSrcLength;
		}
		wsprintf(str,"Converting %s data block to PCM...",file->node->afID);
		ShowProgressStateMsg(str);
		pcmsize=0;
		if (sizeToFill>0)
			pcmsize=AFPLUGIN(node)->FillPCMBuffer(file,pcmBuffer+sizeFilled,sizeToFill,&buffpos);
		if (tag==WAVE_FORMAT_PCM)
		{
			if (pcmsize==0L)
				break;
			ShowProgressStateMsg("Writing WAV data block...");
			WriteFile(wavfile,pcmBuffer,pcmsize,&written,NULL);
			if (written!=pcmsize)
			{
				ReportError(hwnd,"Failure writing WAV file.",NULL);
				SetCancelFlag();
				break;
			}
			datasize+=written;
		}
		else
		{
			acmshdr.cbSrcLength+=pcmsize;
			if (acmshdr.cbSrcLength==0L)
				break;
			acmshdr.fdwStatus^=ACMSTREAMHEADER_STATUSF_DONE;
			acmshdr.cbSrcLengthUsed=0;
			acmshdr.cbDstLength=dstsize;
			acmshdr.cbDstLengthUsed=0;
			wsprintf(str,"Compressing PCM data block...");
			ShowProgressStateMsg(str);
			mmr=acmStreamConvert(hACMStream,&acmshdr,ACM_STREAMCONVERTF_BLOCKALIGN);
			if (mmr!=MMSYSERR_NOERROR)
			{
				CloseHandle(wavfile);
				DeleteFile(fname);
				acmStreamUnprepareHeader(hACMStream,&acmshdr,0L);
				acmStreamClose(hACMStream,0);
				GlobalFree(dstBuffer);
				GlobalFree(pcmBuffer);
				LocalFree(pwfex);
				AFPLUGIN(node)->ShutdownPlayback(file);
				FSCloseFile(file);
				ReportMMError(hwnd,mmr,"Error during compression.");
				return;
			}
			if (acmshdr.cbSrcLengthUsed==0L)
			{
				acmshdr.fdwStatus^=ACMSTREAMHEADER_STATUSF_DONE;
				acmStreamConvert(hACMStream,&acmshdr,0L);
			}
			factsize+=acmshdr.cbSrcLengthUsed/wfexPCM.nBlockAlign;
			ShowProgressStateMsg("Writing WAV data block...");
			WriteFile(wavfile,dstBuffer,acmshdr.cbDstLengthUsed,&written,NULL);
			if (written!=acmshdr.cbDstLengthUsed)
			{
				ReportError(hwnd,"Failure writing WAV file.",NULL);
				SetCancelFlag();
				break;
			}
			datasize+=written;
		}
		ShowProgress(FSGetFilePointer(file),FSGetFileSize(file)); // ???
    }
    if (IsCancelled())
    {
		ShowProgressStateMsg("Deleting WAV file...");
		CloseHandle(wavfile);
		DeleteFile(fname);
    }
    else
    {
		CorrectOddPos(wavfile);
		ShowProgressStateMsg("Rewriting WAV header...");
		riffsize=GetFileSize(wavfile,NULL)-8;
		SetFilePointer(wavfile,pos_riffsize,NULL,FILE_BEGIN);
		WriteFile(wavfile,&riffsize,sizeof(riffsize),&written,NULL);
		if (tag!=WAVE_FORMAT_PCM)
		{
			SetFilePointer(wavfile,pos_factsize,NULL,FILE_BEGIN);
			WriteFile(wavfile,&factsize,sizeof(factsize),&written,NULL);
		}
		SetFilePointer(wavfile,pos_datasize,NULL,FILE_BEGIN);
		WriteFile(wavfile,&datasize,sizeof(datasize),&written,NULL);
		CloseHandle(wavfile);
    }
	if (hACMStream!=NULL)
	{
		ShowProgressStateMsg("Closing conversion stream...");
		acmStreamUnprepareHeader(hACMStream,&acmshdr,0L);
		acmStreamClose(hACMStream,0);
	}
	ShowProgressStateMsg("Freeing conversion buffers...");
	GlobalFree(dstBuffer);
	GlobalFree(pcmBuffer);
	LocalFree(pwfex);
	wsprintf(str,"Shutting down %s decoder...",file->node->afID);
    ShowProgressStateMsg(str);
    AFPLUGIN(node)->ShutdownPlayback(file);
	FSCloseFile(file);
}
