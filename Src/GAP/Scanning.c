/*
 * Game Audio Player source code: scanning of files
 *
 * Copyright (C) 1998-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * Knuth-Morris-Pratt algorithm implementation (used for pattern search) 
 * is based on the one given in the book 
 * "Algorithms and Programming: Theorems and Problems" Birkhauser, 1997 by 
 * Alexander Shen (shen@mccme.ru)
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
#include <commdlg.h>
#include <shlobj.h>

#include "..\Plugins\ResourceFile\RFPlugin.h"
#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Misc.h"
#include "Playlist_NodeOps.h"
#include "Progress.h"
#include "Plugins.h"
#include "Errors.h"
#include "Adding.h"
#include "Scanning.h"

#define NO_VALUE   (0xFFFFFFFF)

void BuildKMPTable(DWORD* table, char* string, DWORD size)
{
	DWORD i,len;

	if ((table==NULL) || (string==NULL) || (size==0L))
		return;

	table[0]=0L;
	for (i=1;i<size;i++)
	{
		len=table[i-1];
		while ((string[len]!=string[i]) && (len>0))
			len=table[len];
		if (string[len]==string[i])
			table[i]=len+1;
		else
			table[i]=0L;
	}
}

void InitSearchState(SearchState* lpSearchState, SearchPattern* lpPattern)
{
	if (
		(lpSearchState==NULL) ||
		(lpPattern==NULL) ||
		(lpPattern->fileIDsize==0L) ||
		(lpPattern->fileID==NULL)
	   )
	{
		lpSearchState->lpPattern=NULL;
		return;
	}

	lpSearchState->lpPattern=lpPattern;
	lpSearchState->curLength=0L;
	lpSearchState->table=(DWORD*)GlobalAlloc(GPTR,sizeof(DWORD)*(lpPattern->fileIDsize));
	BuildKMPTable(lpSearchState->table,lpPattern->fileID,lpPattern->fileIDsize);
}

void ShutdownSearchState(SearchState* lpSearchState)
{
	if (lpSearchState==NULL)
		return;

	GlobalFree(lpSearchState->table);
	lpSearchState->lpPattern=NULL;
}

DWORD FindID(HWND hwnd, char* buffer, DWORD buffsize, SearchState* lpss)
{
    DWORD i;

    for (i=0;i<buffsize;i++)
	{
		while (
			   (lpss->lpPattern->fileID[lpss->curLength]!=buffer[i]) && 
			   (lpss->curLength>0)
			  )
			lpss->curLength=lpss->table[lpss->curLength];
		if (lpss->lpPattern->fileID[lpss->curLength]==buffer[i])
		{
			lpss->curLength++;
			if (lpss->curLength==lpss->lpPattern->fileIDsize)
			{
				ShowProgressStateMsg("Found ID string");
				lpss->curLength=0L;
				return (i-(lpss->lpPattern->fileIDsize)+1);
			}
		}
	}

    return NO_VALUE;
}

long ScanResFile(HWND hwnd, RFPlugin* plugin, const char* rfName, const char* rfDataString)
{
    RFHandle*	 rf;
    DWORD		 i,num=0,curpos,found,newpos,maxcurpos,bufferStart,read;
    AFile       *node;
    PluginNode  *pnode;
	SearchState *pcss;
    char		 str[MAX_PATH+400],*loadBuffer;

    if (plugin==NULL)
		return 0;

    rf=plugin->OpenFile(rfName,rfDataString);
    if (rf==NULL)
    {
		wsprintf(str,"Cannot open resource file: %s %s%s%s",rfName,
					 (lstrcmp(rfDataString,""))?"(":"",
					 rfDataString,
					 (lstrcmp(rfDataString,""))?")":""
				);
		ReportError(hwnd,str,plugin->hDllInst);
		return 0;
    }
	loadBuffer=(char*)GlobalAlloc(GPTR,BUFFERSIZE);
	plugin->SetFilePointer(rf,0,FILE_BEGIN);
	ShowProgressHeaderMsg((lstrcmpi(rfDataString,""))?rfDataString:rfName);
	ShowFileProgress(rf);
	while (!(plugin->EndOfFile(rf)))
	{
		bufferStart=plugin->GetFilePointer(rf);
		ShowProgressStateMsg("Loading block...");
		plugin->ReadFile(rf,loadBuffer,BUFFERSIZE,&read);
		if (read==0L)
			break;
		ShowFileProgress(rf);
		maxcurpos=0;
		pnode=afFirstPlugin;
		while (pnode!=NULL)
		{
			pcss=PSSTATE(pnode->data);
			for (i=0;i<AFNODE(pnode)->numPatterns;i++,pcss++)
			{
				if (
					(pcss->lpPattern==NULL) ||
					(pcss->lpPattern->fileIDsize==0L)
				   )
					continue;
				wsprintf(str,"Searching for %s file ID string %lu...",AFNODE(pnode)->afID,i);
				ShowProgressStateMsg(str);
				curpos=0;
				while (curpos<read)
				{
					found=FindID(hwnd,loadBuffer+curpos,read-curpos,pcss);
					if (found!=NO_VALUE)
					{
						if ((node=AFNODE(pnode)->CreateNodeForID(hwnd,rf,i,bufferStart+curpos+found,&newpos))!=NULL)
						{
							lstrcpy(node->afID,AFNODE(pnode)->afID);
							if (!lstrcmpi(node->afName,""))
								lstrcpy(node->afName,NO_AFNAME);
							lstrcpy(node->rfID,plugin->rfID);
							lstrcpy(node->rfName,rfName);
							lstrcpy(node->rfDataString,rfDataString);
							node->dwData=0L;
							AddNode(node);
							num++;
							wsprintf(str,"%s (%lu)",rfName,num);
							ShowProgressHeaderMsg(str);
							curpos=newpos-bufferStart;
						}
						else
							curpos+=found+(pcss->lpPattern->fileIDsize);
					}
					else
						curpos=read;
					if (IsCancelled())
						break;
				}
				if (IsCancelled())
					break;
				if (maxcurpos<curpos)
					maxcurpos=curpos;
			}
			if (IsCancelled())
				break;
			pnode=pnode->next;
		}
		if (IsCancelled())
			break;
		if (maxcurpos>read)
		{
			pnode=afFirstPlugin;
			while (pnode!=NULL)
			{
				pcss=PSSTATE(pnode->data);
				for (i=0;i<AFNODE(pnode)->numPatterns;i++,pcss++)
					pcss->curLength=0L;
				pnode=pnode->next;
			}
		}
		plugin->SetFilePointer(rf,bufferStart+maxcurpos,FILE_BEGIN);
	}
	plugin->CloseFile(rf);
	GlobalFree(loadBuffer);

    return num;
}

BOOL ScanRFNode(HWND hwnd, PluginNode* pnode, const char* rfName, DWORD* num)
{
    RFile *reslist,*rnode;

    if (pnode==NULL)
		return FALSE;
    reslist=RFNODE(pnode)->GetFileList(rfName);
    if (reslist!=NULL)
    {
		rnode=reslist;
		while (rnode!=NULL)
		{
			if (IsCancelled())
				break;
			(*num)+=ScanResFile(hwnd,RFNODE(pnode),rfName,rnode->rfDataString);
			rnode=rnode->next;
		}
		RFNODE(pnode)->FreeFileList(reslist);
		return TRUE;
    }
    else
		return FALSE;
}

DWORD Scan(HWND hwnd, const char* rfName)
{
    PluginNode *pnode;
    DWORD       num=0;
    BOOL        rfUsed=FALSE;

    pnode=rfFirstPlugin;
    while (pnode!=NULL)
    {
		if ((RFNODE(pnode)->rfFlags) & RFF_VERIFIABLE)
		{
			if (IsCancelled())
				break;
			rfUsed=ScanRFNode(hwnd,pnode,rfName,&num);
			if ((!opAllowMultipleRFPlugins) && (rfUsed))
				break;
		}
		pnode=pnode->next;
    }
    if ((opAllowMultipleRFPlugins) || (!rfUsed))
    {
		pnode=rfFirstPlugin;
		while (pnode!=NULL)
		{
			if (IsCancelled())
				break;
			if (!((RFNODE(pnode)->rfFlags) & RFF_VERIFIABLE))
				ScanRFNode(hwnd,pnode,rfName,&num);
			pnode=pnode->next;
		}
    }

    return num;
}

void Find(HWND hwnd)
{
    OPENFILENAME ofn={0};
    char   szDirName[MAX_PATH];
    char  *szFile;
    char  *szFilter;
    char   fileToOpen[MAX_PATH];
    char   szTitle[]="Select file(s) to search for audio files within";
    int    ldir;
    DWORD  num=0,curfile,numfiles;
    PluginNode *pnode;

	szFile=(char*)LocalAlloc(LPTR,MAX_FILE);
	szFilter=(char*)LocalAlloc(LPTR,MAX_FILTER);
	lstrcpy(szFilter,"");
	StrCatEx(szFilter,"All Files (*.*)\0*.*\0");
    pnode=rfFirstPlugin;
    while (pnode!=NULL)
    {
		StrCatEx(szFilter,RFNODE(pnode)->rfExtensions);
		pnode=pnode->next;
    }
    pnode=afFirstPlugin;
    while (pnode!=NULL)
    {
		StrCatEx(szFilter,AFNODE(pnode)->afExtensions);
		pnode=pnode->next;
    }
    GetCurrentDirectory(sizeof(szDirName), szDirName);
    szFile[0]='\0';
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.lpstrTitle=szTitle;
    ofn.lpstrDefExt=NULL;
    ofn.nMaxFile = MAX_FILE;
    ofn.lpstrFileTitle = NULL;
    ofn.lpstrInitialDir = szDirName;
    ofn.Flags = OFN_ALLOWMULTISELECT |
				OFN_NODEREFERENCELINKS |
				OFN_SHAREAWARE |
				OFN_PATHMUSTEXIST |
				OFN_FILEMUSTEXIST |
				OFN_EXPLORER |
				OFN_READONLY;

    if (GetOpenFileName(&ofn))
    {
		char str[50];
		HWND pwnd;

		lstrcpy(fileToOpen,ofn.lpstrFile);
		if (FileExists(fileToOpen))
		{
			wsprintf(str,"File search: %s",GetFileTitleEx(fileToOpen));
			pwnd=OpenProgress(hwnd,PDT_SINGLE,str);
			num=Scan(pwnd,fileToOpen);
		}
		else
		{
			numfiles=CountFiles(ofn.lpstrFile);
			curfile=0;
			pwnd=OpenProgress(hwnd,PDT_DOUBLE,"File search");
			ShowMasterProgress(curfile,numfiles);
			CorrectDirString(fileToOpen);
			ldir=lstrlen(fileToOpen);
			ofn.lpstrFile+=lstrlen(ofn.lpstrFile)+1;
			while (ofn.lpstrFile[0]!='\0')
			{
				if (IsAllCancelled())
					break;
				lstrcpy(fileToOpen+ldir,ofn.lpstrFile);
				ofn.lpstrFile+=lstrlen(ofn.lpstrFile)+1;
				wsprintf(str,"Resource: %s",GetFileTitleEx(fileToOpen));
				ShowMasterProgressHeaderMsg(str);
				ResetCancelFlag();
				num+=Scan(pwnd,fileToOpen);
				ShowMasterProgress(++curfile,numfiles);
			}
		}
		CloseProgress();
		if (num>0)
			wsprintf(str,"Found %lu files.",num);
		else
			wsprintf(str,"Found no files.");
		MessageBox(GetFocus(),str,szAppName,MB_OK | MB_ICONINFORMATION);
    }
	LocalFree(szFile);
	LocalFree(szFilter);
}

void ScanDir(HWND hwnd)
{
	HWND	     pwnd;
	BROWSEINFO   bi;
	LPITEMIDLIST lpidl;
	DWORD	     num;
	char	     lpszTitle[]="Select directory to scan files from";
	char	     str[30];

	lstrcpy(pafDir,"");
	bi.hwndOwner=hwnd;
	bi.pidlRoot=NULL;
	bi.pszDisplayName=pafDir;
	bi.lpszTitle=lpszTitle;
	bi.ulFlags=BIF_RETURNONLYFSDIRS;
	bi.lpfn=BrowseRecursiveProc;
	bi.lParam=(LPARAM)lpszTitle;
	if ((lpidl=SHBrowseForFolder(&bi))!=NULL)
	{
		SHGetPathFromIDList(lpidl,pafDir);
		CorrectDirString(pafDir);
		pwnd=OpenProgress(hwnd,PDT_SINGLE2,"File search");
		num=PerformActionForDir(pwnd,Scan,dirRecursive);
		CloseProgress();
		if (num>0)
			wsprintf(str,"Found %lu files.",num);
		else
			wsprintf(str,"Found no files.");
		MessageBox(hwnd,str,szAppName,MB_OK | MB_ICONINFORMATION);
	}
}
