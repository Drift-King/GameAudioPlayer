/*
 * Game Audio Player source code: saving/loading playlists
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

#include <stdio.h>
#include <string.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Misc.h"
#include "Progress.h"
#include "ListView.h"
#include "Errors.h"
#include "Playlist_NodeOps.h"
#include "Playlist_SaveLoad.h"

BOOL opTreatCD,opPutCD;

char* ExtractString(char* str, char* xstr)
{
    int   i,brackets;
    char *start;

    start=strchr(str,'<');
    if (start!=NULL)
    {
		brackets=1;
		i=0;
		while ((brackets!=0) && (i<lstrlen(start)))
		{
			i++;
			switch (start[i])
			{
				case '<': brackets++; break;
				case '>': brackets--; break;
				default: break;
			}
		}
		if (brackets==0)
		{
			lstrcpyn(xstr,start+1,i);
			return (start+i+1);
		}
		else
			return str;
    }
    else
		return str;
}

DWORD LoadPlaylist(HWND hwnd, const char *fname)
{
    FILE    *apl;
    char    *data,str[MAX_PATH+512],afName[256],rfName[MAX_PATH],afID[10],rfID[10],afDataString[256],rfDataString[256];
    DWORD    fsStart,fsLength,num,filesize;
    AFile   *node;
    BOOL     wasEmpty=(list_size==0);

    if ((apl=fopen(fname,"rt"))!=NULL)
    {
		fseek(apl,0,SEEK_SET);
		wsprintf(str,"Loading %s",GetFileTitleEx(fname));
		ShowProgressHeaderMsg(str);
		num=0;
		filesize=fsize(apl);
		ShowProgress(ftell(apl),filesize);
		while (1)
		{
			if (IsCancelled())
				break;
			lstrcpy(str,"");
			fgets(str,sizeof(str),apl);
			if (lstrlen(str)==0)
				break;
			node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
			lstrcpy(afID,"");
			lstrcpy(rfID,"");
			lstrcpy(rfName,"");
			lstrcpy(afName,NO_AFNAME);
			lstrcpy(afDataString,"");
			lstrcpy(rfDataString,"");
			data=str;
			data=ExtractString(data,afName);
			data=ExtractString(data,afID);
			data=ExtractString(data,afDataString);
			data=ExtractString(data,rfName);
			data=ExtractString(data,rfID);
			data=ExtractString(data,rfDataString);
			sscanf(data,"%lX %lX",&fsStart,&fsLength);
			lstrcpy(node->afName,afName);
			lstrcpy(node->afID,afID);
			lstrcpy(node->afDataString,afDataString);
			if (opTreatCD)
				RefineResName(node->rfName,rfName);
			else
				lstrcpy(node->rfName,rfName);
			lstrcpy(node->rfID,rfID);
			lstrcpy(node->rfDataString,rfDataString);
			node->fsStart=fsStart;
			node->fsLength=fsLength;
			node->dwData=0L;
			AddNode(node);
			num++;
			wsprintf(str,"Playlist nodes added: %lu",num);
			ShowProgressStateMsg(str);
			ShowProgress(ftell(apl),filesize);
		}
		fclose(apl);
		if (!lstrcmpi(list_filename,""))
		{
			if (wasEmpty)
			{
				lstrcpy(list_filename,fname);
				list_changed=FALSE;
			}
		}
		else
			list_changed=TRUE;
    }
    else
    {
		wsprintf(str,"Error loading playlist: %s",fname);
		ReportError(hwnd,str,NULL);
    }
    UpdateWholeWindow(playList);

	return 1;
}

void Load(HWND hwnd)
{
    OPENFILENAME ofn={0};
    char  szDirName[MAX_PATH];
    char *szFile;
    char  szFilter[]="ANX Playlists (*.APL)\0*.apl\0All Files (*.*)\0*.*\0";
    char  fileToOpen[MAX_PATH];
    char  szTitle[]="Select playlist(s) to load";
    int   ldir,curfile,numfiles;

	szFile=(char*)LocalAlloc(LPTR,MAX_FILE);
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
    ofn.Flags = OFN_NODEREFERENCELINKS |
				OFN_SHAREAWARE |
				OFN_PATHMUSTEXIST |
				OFN_FILEMUSTEXIST |
				OFN_EXPLORER |
				OFN_READONLY |
				OFN_ALLOWMULTISELECT;

    if (GetOpenFileName(&ofn))
    {
		HWND   pwnd;

		lstrcpy(fileToOpen,ofn.lpstrFile);
		if (FileExists(fileToOpen))
		{
			pwnd=OpenProgress(hwnd,PDT_SINGLE,"Playlist loading");
			LoadPlaylist(pwnd,fileToOpen);
		}
		else
		{
			numfiles=CountFiles(ofn.lpstrFile);
			curfile=0;
			pwnd=OpenProgress(hwnd,PDT_DOUBLE,"Playlist loading");
			ShowMasterProgress(curfile,numfiles);
			ShowMasterProgressHeaderMsg("Loaded playlists");
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
				LoadPlaylist(pwnd,fileToOpen);
				ShowMasterProgress(++curfile,numfiles);
			}
		}
		CloseProgress();
    }
	LocalFree(szFile);
}

void SavePlaylist(HWND hwnd, const char *fname)
{
    FILE  *apl;
    AFile *node=list_start;
    char   str[MAX_PATH+50];
    DWORD  num;

    if ((apl=fopen(fname,"wt"))!=NULL)
    {
		fseek(apl,0,SEEK_SET);
		OpenProgress(hwnd,PDT_SINGLE,"Playlist saving");
		wsprintf(str,"Saving %s",GetFileTitleEx(fname));
		ShowProgressHeaderMsg(str);
		num=0;
		ShowProgress(num,(DWORD)list_size);
		while (node!=NULL)
		{
			if (IsCancelled())
				break;
			if (!lstrcmpi(node->afName,""))
				lstrcpy(node->afName,NO_AFNAME);
			fprintf(apl,"<%s> <%s> <%s> <%s%s> <%s> <%s> %lX %lX\n",node->afName,
									node->afID,
								    node->afDataString,
								    ((opPutCD) && (IsCDPath(node->rfName)))?CD_SIG:"",
								    ((opPutCD) && (IsCDPath(node->rfName)))?(node->rfName+2):(node->rfName),
								    node->rfID,
								    node->rfDataString,
								    node->fsStart,
								    node->fsLength
				   );
			node=node->next;
			num++;
			wsprintf(str,"Playlist nodes saved: %lu",num);
			ShowProgressStateMsg(str);
			ShowProgress(num,list_size);
		}
		fclose(apl);
		CloseProgress();
		lstrcpy(list_filename,fname);
		list_changed=FALSE;
    }
    else
    {
		wsprintf(str,"Error saving playlist: %s",fname);
		ReportError(hwnd,str,NULL);
    }
    UpdateWholeWindow(playList);
}

void Save(HWND hwnd)
{
    OPENFILENAME ofn={0};
    char szDirName[MAX_PATH];
    char szFile[2000];
    char szFilter[]="ANX Playlists (*.APL)\0*.apl\0All Files (*.*)\0*.*\0";
    char szTitle[]="Save playlist";

    GetCurrentDirectory(sizeof(szDirName), szDirName);
    szFile[0]='\0';
	lstrcpy(szFile,GetFileTitleEx(list_filename));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.lpstrTitle=szTitle;
    ofn.lpstrDefExt=NULL;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFileTitle = NULL;
    ofn.lpstrInitialDir = szDirName;
    ofn.Flags = OFN_NODEREFERENCELINKS |
				OFN_PATHMUSTEXIST |
				OFN_SHAREAWARE |
				OFN_OVERWRITEPROMPT |
				OFN_EXPLORER |
				OFN_HIDEREADONLY;

    if (GetSaveFileName(&ofn))
	{
		AppendDefExt(szFile,szFilter);
		SavePlaylist(hwnd,szFile);
	}
}
