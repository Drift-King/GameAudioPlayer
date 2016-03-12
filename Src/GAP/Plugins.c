/*
 * Game Audio Player source code: plug-ins management
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

#include "..\Plugins\ResourceFile\RFPlugin.h"
#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "PlayDialog.h"
#include "Plugins.h"
#include "Scanning.h"
#include "Misc.h"
#include "FSHandler.h"

char afPluginDir[MAX_PATH];
char rfPluginDir[MAX_PATH];

PluginNode *afFirstPlugin,*afLastPlugin;
PluginNode *rfFirstPlugin,*rfLastPlugin;

AFPlugin* (__stdcall *appGetAFPlugin)(void);
RFPlugin* (__stdcall *appGetRFPlugin)(void);

BOOL AddPlugin(PluginNode** first, PluginNode** last, PluginNode* pnode)
{
    if (pnode==NULL)
		return FALSE;

    if (*first==NULL)
    {
		*first=pnode;
		*last=pnode;
		pnode->prev=NULL;
		pnode->next=NULL;
    }
    else
    {
		(*last)->next=pnode;
		pnode->prev=*last;
		pnode->next=NULL;
		*last=pnode;
    }

    return TRUE;
}

PluginNode* CreateAFPluginNode(char* filename)
{
	DWORD       i;
    AFPlugin   *plugin;
    HINSTANCE   dll;
    PluginNode *pnode=NULL;
	char        path[MAX_PATH];

	lstrcpy(path,afPluginDir);
	lstrcat(path,filename);
    dll=LoadLibrary(path);
    if (dll==NULL)
		return NULL;
	DisableThreadLibraryCalls(dll);
    appGetAFPlugin=(AFPlugin* (__stdcall *)(void))GetProcAddress(dll,"_GetAudioFilePlugin@0");
    if (appGetAFPlugin!=NULL)
    {
		plugin=appGetAFPlugin();
		if (plugin!=NULL)
		{
			if (
				(plugin->afVer==AFP_VER) && // ???
				(plugin->afID!=NULL) &&
				(plugin->InitPlayback!=NULL) &&
				(plugin->ShutdownPlayback!=NULL) &&
				(plugin->FillPCMBuffer!=NULL)
			   )
			{
				pnode=(PluginNode*)GlobalAlloc(GPTR,sizeof(PluginNode));
				pnode->plugin=plugin;
				lstrcpy(pnode->pluginFileName,GetFileTitleEx(filename));
				strupr(pnode->pluginFileName);
				plugin->hDllInst=dll;
				plugin->hMainWindow=playDialog;
				plugin->szINIFileName=szINIFileName;
				plugin->fsh=&appFSHandler;
				if (
					(plugin->numPatterns==0L) || 
					(plugin->patterns==NULL) ||
					(plugin->CreateNodeForID==NULL)
				   )
				{
					pnode->data=NULL;
					plugin->numPatterns=0L;
					plugin->patterns=NULL;
					plugin->CreateNodeForID=NULL;
				}
				else
				{
					pnode->data=(LPVOID)GlobalAlloc(GPTR,sizeof(SearchState)*(plugin->numPatterns));
					for (i=0;i<(plugin->numPatterns);i++)
						InitSearchState(PSSTATE(pnode->data)+i,(plugin->patterns)+i);
				}
				if (plugin->Init!=NULL)
					plugin->Init();
			}
		}
    }
    if (pnode==NULL)
		FreeLibrary(dll);

    return pnode;
}

PluginNode* CreateRFPluginNode(char* filename)
{
    RFPlugin   *plugin;
    HINSTANCE   dll;
    PluginNode *pnode=NULL;
	char        path[MAX_PATH];

	lstrcpy(path,rfPluginDir);
	lstrcat(path,filename);
    dll=LoadLibrary(path);
    if (dll==NULL)
		return NULL;
	DisableThreadLibraryCalls(dll);
    appGetRFPlugin=(RFPlugin* (__stdcall *)(void))GetProcAddress(dll,"_GetResourceFilePlugin@0");
    if (appGetRFPlugin!=NULL)
    {
		plugin=appGetRFPlugin();
		if (plugin!=NULL)
		{
			if (
				(plugin->rfVer==RFP_VER) && // ???
				(plugin->rfID!=NULL) &&
				(plugin->GetFileList!=NULL) &&
				(plugin->FreeFileList!=NULL) &&
				(plugin->OpenFile!=NULL) &&
				(plugin->CloseFile!=NULL) &&
				(plugin->GetFileSize!=NULL) &&
				(plugin->GetFilePointer!=NULL) &&
				(plugin->EndOfFile!=NULL) &&
				(plugin->SetFilePointer!=NULL) &&
				(plugin->ReadFile!=NULL)
			   )
			{
				pnode=(PluginNode*)GlobalAlloc(GPTR,sizeof(PluginNode));
				pnode->plugin=plugin;
				pnode->data=NULL;
				lstrcpy(pnode->pluginFileName,GetFileTitleEx(filename));
				strupr(pnode->pluginFileName);
				plugin->hDllInst=dll;
				plugin->hMainWindow=playDialog;
				plugin->szINIFileName=szINIFileName;
				if (plugin->Init!=NULL)
					plugin->Init();
			}
		}
    }
    if (pnode==NULL)
		FreeLibrary(dll);

    return pnode;
}

void InitAFPlugins(void)
{
	HANDLE sh;
	char   pattern[MAX_PATH];
    WIN32_FIND_DATA w32fd;

    afFirstPlugin=NULL;
    afLastPlugin=NULL;
	lstrcpy(pattern,afPluginDir);
	lstrcat(pattern,"*.dll");
    sh=FindFirstFile(pattern,&w32fd);
    if (sh!=INVALID_HANDLE_VALUE)
    {
		do
			AddPlugin(&afFirstPlugin,&afLastPlugin,CreateAFPluginNode(w32fd.cFileName));
		while (FindNextFile(sh,&w32fd));
		FindClose(sh);
    }
    if (afFirstPlugin==NULL)
		MessageBox(GetFocus(),"No audio file plug-ins found.",szAppName,MB_OK | MB_ICONWARNING);
}

void InitRFPlugins(void)
{
	HANDLE sh;
	char   pattern[MAX_PATH];
	WIN32_FIND_DATA w32fd;

	rfFirstPlugin=NULL;
    rfLastPlugin=NULL;
	lstrcpy(pattern,rfPluginDir);
	lstrcat(pattern,"*.dll");
    sh=FindFirstFile(pattern,&w32fd);
    if (sh!=INVALID_HANDLE_VALUE)
    {
		do
			AddPlugin(&rfFirstPlugin,&rfLastPlugin,CreateRFPluginNode(w32fd.cFileName));
		while (FindNextFile(sh,&w32fd));
		FindClose(sh);
    }
    if (rfFirstPlugin==NULL)
		MessageBox(GetFocus(),"No resource file plug-ins found.",szAppName,MB_OK | MB_ICONWARNING);
}

void InitPlugins(void)
{
	InitAFPlugins();
	InitRFPlugins();
}

void ShutdownPlugins(void)
{
	DWORD       i;
    PluginNode *pnode,*tnode;

    pnode=afFirstPlugin;
    while (pnode!=NULL)
    {
		tnode=pnode->next;
		if (pnode->data!=NULL)
		{
			for (i=0;i<(AFNODE(pnode)->numPatterns);i++)
				ShutdownSearchState(PSSTATE(pnode->data)+i);
			GlobalFree(pnode->data);
		}
		if (AFNODE(pnode)->Quit!=NULL)
			AFNODE(pnode)->Quit();
		FreeLibrary(AFNODE(pnode)->hDllInst);
		GlobalFree(pnode);
		pnode=tnode;
    }
    afFirstPlugin=NULL;
    afLastPlugin=NULL;

    pnode=rfFirstPlugin;
    while (pnode!=NULL)
    {
		tnode=pnode->next;
		if (RFNODE(pnode)->Quit!=NULL)
			RFNODE(pnode)->Quit();
		FreeLibrary(RFNODE(pnode)->hDllInst);
		GlobalFree(pnode);
		pnode=tnode;
    }
    rfFirstPlugin=NULL;
    rfLastPlugin=NULL;
}

AFPlugin* GetAFPlugin(const char* id)
{
    PluginNode *pnode=afFirstPlugin;

    while ((pnode!=NULL) && (lstrcmp(id,AFNODE(pnode)->afID)))
		pnode=pnode->next;
    if (pnode!=NULL)
		return AFNODE(pnode);
    else
		return NULL;
}

RFPlugin* GetRFPlugin(const char* id)
{
    PluginNode *pnode=rfFirstPlugin;

    while ((pnode!=NULL) && (lstrcmp(id,RFNODE(pnode)->rfID)))
		pnode=pnode->next;
    if (pnode!=NULL)
		return RFNODE(pnode);
    else
		return NULL;
}
