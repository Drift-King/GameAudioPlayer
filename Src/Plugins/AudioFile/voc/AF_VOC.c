/*
 * VOC plug-in source code
 *
 * Copyright (C) 1999-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This code is based on the VOC file format description by:
 * galt@dsd.es.com
 * and uses parts of VOC-dealing SOX source code which is
 * Copyright 1991 Lance Norskog and Sundry Contributors
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

#include "AF_VOC.h"
#include "resource.h"

#define BUFFERSIZE (64000ul)

typedef struct tagVOCBlockNode
{
	BYTE   type;
	WORD   align;
	DWORD  start,length,time,rate;
	struct tagVOCBlockNode *next;
} VOCBlockNode;

typedef struct tagData
{
	DWORD blockRest;
	DWORD rateCurrent;
	BOOL  isSilence;

	VOCBlockNode *chain,*curnode;
	char *buffer;
    WORD  channels,bits,align;
    DWORD rateGlobal;
	BYTE  ct;
} Data;

typedef struct tagVOCInfo
{
	DWORD rate,time;
	WORD  align,channels,bits;
	BYTE  ct;
} VOCInfo;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

void FreeVOCBlockChain(VOCBlockNode *chain)
{
	VOCBlockNode *walker=chain,*tmp;

	while (walker!=NULL)
	{
		tmp=walker->next;
		GlobalFree(walker);
		walker=tmp;
	}
}

VOCBlockNode* ReadVOCBlockChain(FSHandle* f, VOCInfo* vi)
{
    DWORD read,currate,time;
	WORD  curalign,curchannels,curbits;
	BYTE  curct;
	BOOL  found;
    VOCHeader vochdr;
	VOCBlockHeader vocblockhdr;
	VOCDataHeader vocdatahdr;
	VOCNewDataHeader vocndatahdr;
	VOCSilenceHeader vocsilencehdr;
	VOCBlockNode *chain,**pcurnode;

    Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(f,&vochdr,sizeof(VOCHeader),&read);
    if (read<sizeof(VOCHeader))
		return NULL;
    if (memcmp(vochdr.id,IDSTR_VOC,20))
		return NULL;
    Plugin.fsh->SetFilePointer(f,vochdr.datastart,FILE_BEGIN);
	chain=NULL;
	found=FALSE;
	pcurnode=&chain;
	if (vi!=NULL)
	{
		time=0;
		vi->time=0;
		vi->rate=0;
		vi->channels=0;
		vi->bits=0;
		vi->ct=0;
		vi->align=0;
	}
	while (!(Plugin.fsh->EndOfFile(f)))
	{
		Plugin.fsh->ReadFile(f,&vocblockhdr,sizeof(vocblockhdr),&read);
		if (read==0L)
			break; // ???
		if (vocblockhdr.type==VOC_TERM)
			break;
		if (read<sizeof(vocblockhdr))
			break; // ???
		(*pcurnode)=(VOCBlockNode*)GlobalAlloc(GPTR,sizeof(VOCBlockNode));
		(*pcurnode)->next=NULL;
		(*pcurnode)->type=vocblockhdr.type;
		(*pcurnode)->start=Plugin.fsh->GetFilePointer(f);
		(*pcurnode)->length=VOCBLOCKSIZE(vocblockhdr);
		switch (vocblockhdr.type)
		{
			case VOC_DATA:
				Plugin.fsh->ReadFile(f,&vocdatahdr,sizeof(vocdatahdr),&read);
				Plugin.fsh->SetFilePointer(f,-(LONG)sizeof(vocdatahdr),FILE_CURRENT);
				if (read<sizeof(vocdatahdr))
				{
					FreeVOCBlockChain(chain);
					SetLastError(PRIVEC(IDS_INCORRECTFILE));
					return NULL;
				}
				currate=(1000000/(256-vocdatahdr.sr));
				curchannels=1; // ???
				curbits=8; // ???
				curct=vocdatahdr.ct;
				if (vocdatahdr.ct==VOC_CT_8BIT)
					curalign=curchannels*(curbits/8);
				else
					curalign=0;
				if (!found)
				{
					if (vi!=NULL)
					{
						vi->rate=currate;
						vi->channels=curchannels;
						vi->bits=curbits;
						vi->ct=curct;
						vi->align=curalign;
					}
					found=TRUE;
				}
				if ((curalign!=0) && (currate!=0))
				{
					(*pcurnode)->time=MulDiv(1000,VOCBLOCKSIZE(vocblockhdr)-sizeof(VOCDataHeader),currate*curalign);
					time+=(*pcurnode)->time;
				}
				(*pcurnode)->rate=currate;
				(*pcurnode)->align=curalign;
				break;
			case VOC_NEWDATA:
				Plugin.fsh->ReadFile(f,&vocndatahdr,sizeof(vocndatahdr),&read);
				Plugin.fsh->SetFilePointer(f,-(LONG)sizeof(vocndatahdr),FILE_CURRENT);
				if (read<sizeof(vocndatahdr))
				{
					FreeVOCBlockChain(chain);
					SetLastError(PRIVEC(IDS_INCORRECTFILE));
					return NULL;
				}
				currate=vocndatahdr.rate;
				curbits=vocndatahdr.bits;
				curchannels=vocndatahdr.channels;
				curct=VOC_CT_8BIT; // ???
				curalign=curchannels*(curbits/8);
				if (!found)
				{
					if (vi!=NULL)
					{
						vi->rate=currate;
						vi->channels=curchannels;
						vi->bits=curbits;
						vi->ct=curct;
						vi->align=curalign;
					}
					found=TRUE;
				}
				if ((curalign!=0) && (currate!=0))
				{
					(*pcurnode)->time=MulDiv(1000,VOCBLOCKSIZE(vocblockhdr)-sizeof(VOCNewDataHeader),currate*curalign);
					time+=(*pcurnode)->time;
				}
				(*pcurnode)->rate=currate;
				(*pcurnode)->align=curalign;
				break;
			case VOC_CONT:
				if ((curalign!=0) && (currate!=0))
				{
					(*pcurnode)->time=MulDiv(1000,VOCBLOCKSIZE(vocblockhdr),currate*curalign);
					time+=(*pcurnode)->time;
				}
				(*pcurnode)->rate=currate;
				(*pcurnode)->align=curalign;
				break;
			case VOC_SILENCE:
				if ((curalign!=0) && (currate!=0))
				{
					Plugin.fsh->ReadFile(f,&vocsilencehdr,sizeof(vocsilencehdr),&read);
					if (read<sizeof(vocsilencehdr))
					{
						FreeVOCBlockChain(chain);
						SetLastError(PRIVEC(IDS_INCORRECTFILE));
						return NULL;
					}
					Plugin.fsh->SetFilePointer(f,-(LONG)sizeof(vocsilencehdr),FILE_CURRENT);
					currate=(1000000/(256-vocsilencehdr.sr));
					(*pcurnode)->time=MulDiv(1000,vocsilencehdr.length+1,currate*curalign);
					time+=(*pcurnode)->time;
				}
				(*pcurnode)->rate=currate;
				(*pcurnode)->align=curalign;
				break;
			case VOC_MARKER:
			case VOC_TEXT:
			case VOC_LOOP:
			case VOC_LOOPEND:
			case VOC_EXTENDED:
				(*pcurnode)->time=0;
				(*pcurnode)->rate=0;
				(*pcurnode)->align=0;
				break;
			default:
				FreeVOCBlockChain(chain);
				SetLastError(PRIVEC(IDS_UNKNOWNBLOCK));
				return NULL;
		}
		pcurnode=&((*pcurnode)->next);
		Plugin.fsh->SetFilePointer(f,VOCBLOCKSIZE(vocblockhdr),FILE_CURRENT);
    }
	if (!found)
	{
		FreeVOCBlockChain(chain);
		SetLastError(PRIVEC(IDS_NODATA));
		return NULL;
	}
	if (vi!=NULL)
		vi->time=time;
	Plugin.fsh->SetFilePointer(f,vochdr.datastart,FILE_BEGIN);

	return chain;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	VOCInfo vi;
	VOCBlockNode *chain;

    if (f==NULL)
		return FALSE;

    chain=ReadVOCBlockChain(f,&vi);
	if (chain==NULL)
		return FALSE;
    switch (vi.ct)
    {
		case VOC_CT_8BIT:
			break;
		case VOC_CT_4BIT:
		case VOC_CT_26BIT:
		case VOC_CT_2BIT:
			FreeVOCBlockChain(chain);
			SetLastError(PRIVEC(IDS_NODECODER));
			return FALSE;
		default: // ???
			return FALSE;
    }
	*rate=vi.rate;
	*channels=vi.channels;
	*bits=vi.bits;
	f->afData=GlobalAlloc(GPTR,sizeof(Data));
    if (f->afData==NULL)
    {
		FreeVOCBlockChain(chain);
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
    }
	LPData(f)->buffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
	if (LPData(f)->buffer==NULL)
    {
		GlobalFree(f->afData);
		FreeVOCBlockChain(chain);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
    }
	LPData(f)->chain=chain;
	LPData(f)->curnode=chain;
    LPData(f)->blockRest=0;
	LPData(f)->isSilence=FALSE;
    LPData(f)->align=vi.align;
    LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->ct=vi.ct;
    LPData(f)->rateGlobal=*rate;
	LPData(f)->rateCurrent=*rate;

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

	FreeVOCBlockChain(LPData(f)->chain);
    return (
			(GlobalFree(LPData(f)->buffer)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
    DWORD  read;
	VOCHeader vochdr;

    if (f==NULL)
		return NULL;

	Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(f,&vochdr,sizeof(VOCHeader),&read);
    if (read<sizeof(VOCHeader))
		return NULL;
    if (memcmp(vochdr.id,IDSTR_VOC,20))
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
    FSHandle *f;
	VOCInfo vi;
	VOCBlockNode *chain;

    if (node==NULL)
		return -1;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;

	if ((chain=ReadVOCBlockChain(f,&vi))==NULL)
	{
		Plugin.fsh->CloseFile(f);
		return -1;
	}
	FreeVOCBlockChain(chain);
    Plugin.fsh->CloseFile(f);

    return (DWORD)(vi.time);
}

// dirty stuff
DWORD ResampleBuffer(char* inbuf, char* outbuf, DWORD insize, DWORD align, DWORD inrate, DWORD outrate)
{
	DWORD i,innum,outnum;

	innum=insize/align;
	outnum=MulDiv(innum,outrate,inrate);
	for (i=0;i<outnum;i++)
		memcpy(outbuf+i*align,inbuf+MulDiv(i,inrate,outrate)*align,align); // ???

	return outnum*align;
}

DWORD __stdcall FillPCMBuffer(FSHandle* f, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    BOOL	         found;
	DWORD		     read,toRead;
	VOCDataHeader	 vocdatahdr;
	VOCNewDataHeader vocndatahdr;
	VOCSilenceHeader vocsilencehdr;

    if (f==NULL)
		return 0;
    if (LPData(f)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(f))
		return 0;

    switch (LPData(f)->ct)
    {
		case VOC_CT_8BIT:
			if (LPData(f)->blockRest==0)
			{
				found=FALSE;
				while ((LPData(f)->curnode!=NULL) && (!found))
				{
					switch (LPData(f)->curnode->type)
					{
						case VOC_TERM: // should not happen
							return 0;
						case VOC_DATA:
							Plugin.fsh->SetFilePointer(f,LPData(f)->curnode->start,FILE_BEGIN);
							Plugin.fsh->ReadFile(f,&vocdatahdr,sizeof(vocdatahdr),&read);
							if (read<sizeof(vocdatahdr))
								break; // ???
							LPData(f)->rateCurrent=(1000000/(256-vocdatahdr.sr));
							LPData(f)->blockRest=(LPData(f)->curnode->length)-sizeof(VOCDataHeader);
							LPData(f)->isSilence=FALSE;
							found=TRUE;
							break;
						case VOC_NEWDATA:
							Plugin.fsh->SetFilePointer(f,LPData(f)->curnode->start,FILE_BEGIN);
							Plugin.fsh->ReadFile(f,&vocndatahdr,sizeof(vocndatahdr),&read);
							if (read<sizeof(vocndatahdr))
								break; // ???
							LPData(f)->rateCurrent=vocndatahdr.rate;
							LPData(f)->blockRest=(LPData(f)->curnode->length)-sizeof(VOCNewDataHeader);
							LPData(f)->isSilence=FALSE;
							found=TRUE;
							break;
						case VOC_SILENCE:
							Plugin.fsh->SetFilePointer(f,LPData(f)->curnode->start,FILE_BEGIN);
							Plugin.fsh->ReadFile(f,&vocsilencehdr,sizeof(vocsilencehdr),&read);
							if (read<sizeof(vocsilencehdr))
								break; // ???
							LPData(f)->rateCurrent=(1000000/(256-vocsilencehdr.sr));
							LPData(f)->blockRest=MulDiv(vocsilencehdr.length+1,LPData(f)->rateGlobal,LPData(f)->rateCurrent);
							LPData(f)->isSilence=TRUE;
							found=TRUE;
							break;
						case VOC_CONT:
							Plugin.fsh->SetFilePointer(f,LPData(f)->curnode->start,FILE_BEGIN);
							LPData(f)->blockRest=(LPData(f)->curnode->length);
							LPData(f)->isSilence=FALSE;
							found=TRUE;
							break;
						case VOC_MARKER:
						case VOC_TEXT:
						case VOC_LOOP:
						case VOC_LOOPEND:
						case VOC_EXTENDED:
							break;
						default: // ???
							break;
					}
					if (!found)
						LPData(f)->curnode=LPData(f)->curnode->next;
				}
				if (!found)
					return 0;
			}
			if (LPData(f)->isSilence)
			{
				toRead=min(buffsize,LPData(f)->blockRest);
				toRead=CORRALIGN(toRead,LPData(f)->align);
				if (LPData(f)->bits==16)
					memset(buffer,0x00,toRead);
				else
					memset(buffer,0x80,toRead);
				LPData(f)->blockRest-=toRead;
				read=toRead;
			}
			else if (LPData(f)->rateCurrent==LPData(f)->rateGlobal)
			{
				toRead=min(buffsize,LPData(f)->blockRest);
				toRead=CORRALIGN(toRead,LPData(f)->align);
				Plugin.fsh->ReadFile(f,buffer,toRead,&read);
				LPData(f)->blockRest-=read;
			}
			else // this stuff is dirty !!! hope this'll never happen...
			{
				toRead=min(BUFFERSIZE,min((DWORD)MulDiv(buffsize,LPData(f)->rateCurrent,LPData(f)->rateGlobal),LPData(f)->blockRest));
				toRead=CORRALIGN(toRead,LPData(f)->align);
				Plugin.fsh->ReadFile(f,LPData(f)->buffer,toRead,&read);
				if (read==0L)
					break; // ???
				LPData(f)->blockRest-=read;
				read=ResampleBuffer(LPData(f)->buffer,buffer,read,LPData(f)->align,LPData(f)->rateCurrent,LPData(f)->rateGlobal);
			}
			if ((LPData(f)->blockRest==0) && (LPData(f)->curnode!=NULL))
				LPData(f)->curnode=LPData(f)->curnode->next;
			break;
		case VOC_CT_4BIT:
		case VOC_CT_26BIT:
		case VOC_CT_2BIT:
			// don't know what to do...
			read=0;
			break;
		default:
			read=0;
			break;
	}
	*buffpos=-1;

	return read;
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
BYTE   ibCT,ibVerMinor,ibVerMajor;
WORD   ibChannels,ibBits;

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
    char str[512],format[100];

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
			switch (ibCT)
			{
				case VOC_CT_8BIT:
					lstrcpy(format,"PCM");
					break;
				case VOC_CT_4BIT:
					lstrcpy(format,"4-bit compressed");
					break;
				case VOC_CT_26BIT:
					lstrcpy(format,"2.6-bit compressed");
					break;
				case VOC_CT_2BIT:
					lstrcpy(format,"2-bit compressed");
					break;
				default:
					lstrcpy(format,"Unknown");
			}
			wsprintf(str,"Format: %s (%u)\r\n"
						 "Version: %u.%u\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit",
						 format,
						 ibCT,
						 ibVerMajor,ibVerMinor,
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
	DWORD read;
	VOCInfo vi;
	VOCHeader vochdr;
	VOCBlockNode *chain;

    if (node==NULL)
		return;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
	Plugin.fsh->SetFilePointer(f,0,FILE_BEGIN);
    Plugin.fsh->ReadFile(f,&vochdr,sizeof(VOCHeader),&read);
    if (
		(read<sizeof(VOCHeader)) ||
		(memcmp(vochdr.id,IDSTR_VOC,20))
	   )
	{
		wsprintf(str,"This is not %s file.",Plugin.afID);
		MessageBox(hwnd,str,Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		Plugin.fsh->CloseFile(f);
		return;
	}
	ibVerMajor=vochdr.ver_major;
	ibVerMinor=vochdr.ver_minor;
	chain=ReadVOCBlockChain(f,&vi);
	if (chain==NULL)
	{
		wsprintf(str,"Incorrect %s file.",Plugin.afID);
		MessageBox(hwnd,str,Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		Plugin.fsh->CloseFile(f);
		return;
	}
	ibRate=vi.rate;
	ibChannels=vi.channels;
	ibBits=vi.bits;
	ibCT=vi.ct;
	FreeVOCBlockChain(chain);
    ibSize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);
    ibNode=node;
    if (node->afTime!=-1)
		ibTime=node->afTime;
    else
		ibTime=vi.time;
    DialogBox(Plugin.hDllInst,"InfoBox",hwnd,InfoBoxDlgProc);
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
	VOCHeader      vochdr;
	VOCBlockHeader vocblockhdr;
    DWORD	   read,length;
    AFile	  *node;

    if (f->rfPlugin==NULL)
		return NULL;

    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying VOC blocks chain...");
    RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
    RFPLUGIN(f)->ReadFile(f,&vochdr,sizeof(vochdr),&read);
	if (read<sizeof(vochdr))
		return NULL;
	RFPLUGIN(f)->SetFilePointer(f,pos+vochdr.datastart,FILE_BEGIN);
	length=vochdr.datastart;
	while (!(RFPLUGIN(f)->EndOfFile(f)))
	{
		if ((BOOL)SendMessage(hwnd,WM_GAP_ISCANCELLED,0,0))
			return NULL;
		RFPLUGIN(f)->ReadFile(f,&vocblockhdr,sizeof(vocblockhdr),&read);
		if (read==0L)
			return NULL;
		if (vocblockhdr.type==VOC_TERM)
		{
			length+=1;
			break;
		}
		if (read<sizeof(vocblockhdr))
			return NULL;
		switch (vocblockhdr.type)
		{
			case VOC_DATA:
			case VOC_CONT:
			case VOC_SILENCE:
			case VOC_MARKER:
			case VOC_TEXT:
			case VOC_LOOP:
			case VOC_LOOPEND:
			case VOC_EXTENDED:
			case VOC_NEWDATA:
				break;
			default: // ???
				return NULL; // ???
		}
		RFPLUGIN(f)->SetFilePointer(f,VOCBLOCKSIZE(vocblockhdr),FILE_CURRENT);
		length+=sizeof(vocblockhdr)+VOCBLOCKSIZE(vocblockhdr);
		SendMessage(hwnd,WM_GAP_SHOWFILEPROGRESS,0,(LPARAM)f);
    }
	if (vocblockhdr.type!=VOC_TERM)
	{
		SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"No VOC terminator.");
		return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=length;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,"");
    SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"VOC file correct.");

    return node;
}

void __stdcall Seek(FSHandle* f, DWORD pos)
{
    DWORD         filepos;
	VOCBlockNode *walker;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

	walker=LPData(f)->chain;
	while (walker!=NULL)
	{
		if (walker->time>pos)
			break;
		pos-=walker->time;
		walker=walker->next;
	}
	if (walker==NULL)
		return;
	LPData(f)->curnode=walker;
    filepos=MulDiv(pos,(walker->rate)*(walker->align),1000);
	LPData(f)->isSilence=FALSE;
	switch (walker->type)
	{
		case VOC_DATA:
			filepos+=sizeof(VOCDataHeader);
			break;
		case VOC_NEWDATA:
			filepos+=sizeof(VOCNewDataHeader);
			break;
		case VOC_CONT:
			// nothing to do...
			break;
		case VOC_SILENCE:
			filepos=0;
			LPData(f)->isSilence=TRUE;
			break;
		default: // ???
			return;
	}
	filepos=CORRALIGN(filepos,walker->align);
    Plugin.fsh->SetFilePointer(f,walker->start+filepos,FILE_BEGIN);
	LPData(f)->rateCurrent=walker->rate;
	if (walker->type!=VOC_SILENCE)
		LPData(f)->blockRest=(walker->length)-filepos;
	else
		LPData(f)->blockRest=CORRALIGN(MulDiv((walker->time)-pos,(LPData(f)->rateGlobal)*(LPData(f)->align),1000),LPData(f)->align); // ???
}

void __stdcall Init(void)
{
	DisableThreadLibraryCalls(Plugin.hDllInst);
}

SearchPattern patterns[]=
{
	{20,IDSTR_VOC}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX VOC Audio File Plug-In",
    "v0.91",
    "Creative Labs Voice Files (*.VOC)\0*.voc\0",
    "VOC",
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
