/*
 * Game Audio Player source code: adding files
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
#include <commdlg.h>
#include <shlobj.h>

#include "..\Plugins\ResourceFile\RFPlugin.h"
#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Misc.h"
#include "Playlist_NodeOps.h"
#include "Progress.h"
#include "FSHandler.h"
#include "Plugins.h"
#include "Errors.h"
#include "Adding.h"

BOOL opAllowMultipleAFPlugins,
     opAllowMultipleRFPlugins;

BOOL AddAFNodeResFile(HWND hwnd, PluginNode* pnode, FSHandle* file, const char* rfName, const char* rfDataString)
{
	AFile *node;

	if (AFNODE(pnode)->CreateNodeForFile!=NULL)
	{
		if ((node=AFNODE(pnode)->CreateNodeForFile(file,rfName,rfDataString))!=NULL)
		{
			lstrcpy(node->afID,AFNODE(pnode)->afID);
			if (!lstrcmpi(node->afName,""))
				lstrcpy(node->afName,GetFileTitleEx((!lstrcmpi(rfDataString,""))?rfName:rfDataString));
			lstrcpy(node->rfID,RFPLUGIN(file->rf)->rfID);
			lstrcpy(node->rfName,rfName);
			lstrcpy(node->rfDataString,rfDataString);
			node->dwData=0L;
			AddNode(node);
			return TRUE;
		}
		else
			return FALSE;
    }
    else
		return FALSE;
}

void AddResFile(HWND hwnd, RFPlugin* plugin, const char* rfName, const char* rfDataString)
{
    BOOL        afUsed=FALSE;
    PluginNode *pnode;
	FSHandle   *file;

    if (plugin==NULL)
		return;

	file=FSOpenFileAsSection(plugin,rfName,rfDataString,0,0xFFFFFFFF);
	if (file==NULL)
		return;
    pnode=afFirstPlugin;
    while (pnode!=NULL)
    {
		if ((AFNODE(pnode)->afFlags) & AFF_VERIFIABLE)
		{
			afUsed=AddAFNodeResFile(hwnd,pnode,file,rfName,rfDataString);
			if ((!opAllowMultipleAFPlugins) && (afUsed))
				break;
		}
		pnode=pnode->next;
    }
    if ((opAllowMultipleAFPlugins) || (!afUsed))
    {
		pnode=afFirstPlugin;
		while (pnode!=NULL)
		{
			if (!((AFNODE(pnode)->afFlags) & AFF_VERIFIABLE))
				AddAFNodeResFile(hwnd,pnode,file,rfName,rfDataString);
			pnode=pnode->next;
		}
    }
	FSCloseFile(file);
}

BOOL AddRFNode(HWND hwnd, PluginNode* pnode, const char* rfName)
{
    DWORD  k,num;
    RFile *reslist,*rnode;

    reslist=RFNODE(pnode)->GetFileList(rfName);
    if (reslist!=NULL)
    {
		num=0;
		rnode=reslist;
		while (rnode!=NULL)
		{
			num++;
			rnode=rnode->next;
		}
		k=0;
		if (num>1)
			OpenProgress(hwnd,PDT_SINGLE,"Adding files from resource");
		ShowProgressHeaderMsg(rfName);
		ShowProgress(k,num);
		rnode=reslist;
		while (rnode!=NULL)
		{
			if (IsCancelled())
				break;
			ShowProgressStateMsg(rnode->rfDataString);
			AddResFile(hwnd,RFNODE(pnode),rfName,rnode->rfDataString);
			rnode=rnode->next;
			ShowProgress(++k,num);
		}
		RFNODE(pnode)->FreeFileList(reslist);
		return TRUE;
    }
    else
		return FALSE;
}

DWORD AddFile(HWND hwnd, const char* rfName)
{
    PluginNode *pnode;
    BOOL        rfUsed=FALSE;

    pnode=rfFirstPlugin;
    while (pnode!=NULL)
    {
		if ((RFNODE(pnode)->rfFlags) & RFF_VERIFIABLE)
		{
			rfUsed=AddRFNode(hwnd,pnode,rfName);
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
			if (!((RFNODE(pnode)->rfFlags) & RFF_VERIFIABLE))
				AddRFNode(hwnd,pnode,rfName);
			pnode=pnode->next;
		}
    }

	return 1;
}

void Add(HWND hwnd)
{
    OPENFILENAME ofn={0};
    char szDirName[MAX_PATH];
    char *szFile;
    char *szFilter;
    char fileToOpen[MAX_PATH];
    char szTitle[]="Select file(s) to add to playlist";
    long ldir,curfile,numfiles;
    PluginNode *pnode;

	szFile=(char*)LocalAlloc(LPTR,MAX_FILE);
	szFilter=(char*)LocalAlloc(LPTR,MAX_FILTER);
	lstrcpy(szFilter,"");
	StrCatEx(szFilter,"All Files (*.*)\0*.*\0");
    pnode=afFirstPlugin;
    while (pnode!=NULL)
    {
		StrCatEx(szFilter,AFNODE(pnode)->afExtensions);
		pnode=pnode->next;
    }
    pnode=rfFirstPlugin;
    while (pnode!=NULL)
    {
		StrCatEx(szFilter,RFNODE(pnode)->rfExtensions);
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
		HWND   pwnd;

		lstrcpy(fileToOpen,ofn.lpstrFile);
		if (FileExists(fileToOpen))
			AddFile(hwnd,fileToOpen);
		else
		{
			numfiles=CountFiles(ofn.lpstrFile);
			curfile=0;
			pwnd=OpenProgress(hwnd,PDT_DOUBLE,"File verification");
			ShowMasterProgress(curfile,numfiles);
			ShowMasterProgressHeaderMsg("Verified files");
			CorrectDirString(fileToOpen);
			ldir=lstrlen(fileToOpen);
			ofn.lpstrFile+=lstrlen(ofn.lpstrFile)+1;
			while (ofn.lpstrFile[0]!='\0')
			{
				if (IsAllCancelled())
					break;
				lstrcpy(fileToOpen+ldir,ofn.lpstrFile);
				ofn.lpstrFile+=lstrlen(ofn.lpstrFile)+1;
				ResetCancelFlag();
				AddFile(pwnd,fileToOpen);
				ShowMasterProgress(++curfile,numfiles);
			}
		}
		CloseProgress();
    }
	LocalFree(szFile);
	LocalFree(szFilter);
}

void AddDir(HWND hwnd)
{
	HWND	     pwnd;
	BROWSEINFO   bi;
	LPITEMIDLIST lpidl;
	char	     lpszTitle[]="Select directory to add files from";

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
		pwnd=OpenProgress(hwnd,PDT_SIMPLE,"Verifying files");
		PerformActionForDir(pwnd,AddFile,dirRecursive);
		CloseProgress();
	}
}
