/*
 * Game Audio Player source code: options dialog box, options saving/loading
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
#include <prsht.h>

#include "Globals.h"
#include "Misc.h"
#include "Options.h"
#include "Options_Playback.h"
#include "Options_Playlist.h"
#include "Options_Saving.h"
#include "Options_AFPlugins.h"
#include "Options_RFPlugins.h"
#include "PlayDialog.h"
#include "Plugins.h"
#include "Player.h"
#include "Playlist.h"
#include "Adding.h"
#include "Playlist_NodeOps.h"
#include "Playlist_SaveLoad.h"
#include "Playlist_MultiOps.h"
#include "resource.h"

IntOption repeatTypeOptions[]=
{
	{"off", ID_REPEAT},
	{"one", ID_REPEAT_ONE},
	{"all", ID_REPEAT_ALL}
};

IntOption dropSupportOptions[]=
{
	{"off",  ID_DROP_OFF},
	{"ask",  ID_DROP_ASK},
	{"add",  ID_DROP_ADD},
	{"scan", ID_DROP_SCAN},
	{"load", ID_DROP_LOAD}
};

IntOption playlistTypeOptions[]=
{
	{"dialog", ID_PLAYLIST_DIALOG},
	{"window", ID_PLAYLIST_WINDOW}
};

IntOption multiSaveFileNameOptions[]=
{
	{"ask_filename", ID_ASKFILENAME},
	{"ask_template", ID_ASKTEMPLATE},
	{"use_template", ID_USETEMPLATE}
};

// internal
int propPage=0;

int CreateOptionsDlg(HWND hwnd)
{
    PROPSHEETPAGE   psp[5];
    PROPSHEETHEADER psh;

    psp[0].dwSize = sizeof(PROPSHEETPAGE);
    psp[0].dwFlags = PSP_USETITLE;
    psp[0].hInstance = hInst;
    psp[0].pszTemplate = "PlaybackOptionsPage";
    psp[0].pfnDlgProc = PlaybackOptionsProc;
    psp[0].pszTitle = "Playback";
    psp[0].lParam = 0;

    psp[1].dwSize = sizeof(PROPSHEETPAGE);
    psp[1].dwFlags = PSP_USETITLE;
    psp[1].hInstance = hInst;
    psp[1].pszTemplate = "PlaylistOptionsPage";
    psp[1].pfnDlgProc = PlaylistOptionsProc;
    psp[1].pszTitle = "Playlist";
    psp[1].lParam = 0;

	psp[2].dwSize = sizeof(PROPSHEETPAGE);
    psp[2].dwFlags = PSP_USETITLE;
    psp[2].hInstance = hInst;
    psp[2].pszTemplate = "SavingOptionsPage";
    psp[2].pfnDlgProc = SavingOptionsProc;
    psp[2].pszTitle = "Saving";
    psp[2].lParam = 0;

    psp[3].dwSize = sizeof(PROPSHEETPAGE);
    psp[3].dwFlags = PSP_USETITLE;
    psp[3].hInstance = hInst;
    psp[3].pszTemplate = "ARFPluginsOptionsPage";
    psp[3].pfnDlgProc = AFPluginsOptionsProc;
    psp[3].pszTitle = "AF Plug-Ins";
    psp[3].lParam = 0;

    psp[4].dwSize = sizeof(PROPSHEETPAGE);
    psp[4].dwFlags = PSP_USETITLE;
    psp[4].hInstance = hInst;
    psp[4].pszTemplate = "ARFPluginsOptionsPage";
    psp[4].pfnDlgProc = RFPluginsOptionsProc;
    psp[4].pszTitle = "RF Plug-Ins";
    psp[4].lParam = 0;

    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_PROPSHEETPAGE;
    psh.hwndParent = hwnd;
    psh.hInstance = hInst;
    psh.pszCaption = (LPSTR)"GAP Options";
    psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
    psh.nStartPage=propPage;
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;

    return PropertySheet(&psh);
}

void EnableApply(HWND hwnd)
{
    PropSheet_Changed(GetParent(hwnd),hwnd);
}

void DisableApply(HWND hwnd)
{
    PropSheet_UnChanged(GetParent(hwnd),hwnd);
}

void TrackPropPage(HWND hwnd, int index)
{
    if (PropSheet_GetCurrentPageHwnd(GetParent(hwnd))==hwnd)
		propPage=index;
}

void WriteIntOption(const char* section, const char* key, IntOption* pio, int num, int value)
{
    int i;

    for (i=0;i<num;i++)
		if (pio[i].value==value)
		{
			WritePrivateProfileString(section,key,pio[i].str,szINIFileName);
			return;
		}
}

#define SaveOptionBool(b) ((b)?"on":"off")

void SaveOptions(void)
{
    char str[40];

    wsprintf(str,"%d",waveDeviceID);
    WritePrivateProfileString("Playback","WaveDeviceID",str,szINIFileName);
    wsprintf(str,"%lu",bufferSize/1024);
    WritePrivateProfileString("Playback","BufferSize",str,szINIFileName);
	WritePrivateProfileString("Playlist","PlayNext",SaveOptionBool(opPlayNext),szINIFileName);
    WriteIntOption("Playlist","Repeat",repeatTypeOptions,sizeof(repeatTypeOptions)/sizeof(IntOption),opRepeatType);
    WritePrivateProfileString("Playlist","Shuffle",SaveOptionBool(opShuffle),szINIFileName);
    WritePrivateProfileString("Playlist","IntroScan",SaveOptionBool(opIntroScan),szINIFileName);
    wsprintf(str,"%u",introScanLength/1000);
    WritePrivateProfileString("Playlist","IntroScanLength",str,szINIFileName);
    WritePrivateProfileString("Playlist","AutoLoad",SaveOptionBool(opAutoLoad),szINIFileName);
    WritePrivateProfileString("Playlist","AutoPlay",SaveOptionBool(opAutoPlay),szINIFileName);
	WritePrivateProfileString("PlayerWindow","TimeMode",(opTimeElapsed)?"elapsed":"remaining",szINIFileName);
	WritePrivateProfileString("PlayerWindow","EasyMove",SaveOptionBool(opEasyMove),szINIFileName);
	WritePrivateProfileString("Playlist","NoTime",SaveOptionBool(opNoTime),szINIFileName);
    WritePrivateProfileString("Playlist","TreatCD",SaveOptionBool(opTreatCD),szINIFileName);
    WritePrivateProfileString("Playlist","PutCD",SaveOptionBool(opPutCD),szINIFileName);
    wsprintf(str,"%d",CDDrive);
    WritePrivateProfileString("Playlist","CDDrive",str,szINIFileName);
	WriteIntOption("Playlist","DropSupport",dropSupportOptions,sizeof(dropSupportOptions)/sizeof(IntOption),opDropSupport);
	WriteIntOption("Playlist","PlaylistType",playlistTypeOptions,sizeof(playlistTypeOptions)/sizeof(IntOption),opPlaylistType);
    wsprintf(str,"%lu",GetPriorityClass(GetCurrentProcess()));
    WritePrivateProfileString("Priority","PriorityClass",str,szINIFileName);
    wsprintf(str,"%d",GetThreadPriority(GetCurrentThread()));
    WritePrivateProfileString("Priority","MainPriority",str,szINIFileName);
	wsprintf(str,"%d",playbackPriority);
    WritePrivateProfileString("Priority","PlaybackPriority",str,szINIFileName);
    WritePrivateProfileString("AllowMultiplePlugins","AFPlugins",SaveOptionBool(opAllowMultipleAFPlugins),szINIFileName);
    WritePrivateProfileString("AllowMultiplePlugins","RFPlugins",SaveOptionBool(opAllowMultipleRFPlugins),szINIFileName);
	WriteIntOption("Saving","MultiSaveFileName",multiSaveFileNameOptions,sizeof(multiSaveFileNameOptions)/sizeof(IntOption),multiSaveFileName);
	CorrectDirString(defTemplateDir);
	CorrectExtString(defTemplateFileExt);
	WritePrivateProfileString("Saving","DefTemplateDir",defTemplateDir,szINIFileName);
	WritePrivateProfileString("Saving","DefTemplateFileTitle",defTemplateFileTitle,szINIFileName);
	WritePrivateProfileString("Saving","DefTemplateFileExt",defTemplateFileExt,szINIFileName);
	wsprintf(str,"%u",waveTag);
	WritePrivateProfileString("Saving","MultiConvertWaveTag",str,szINIFileName);
	CorrectDirString(afPluginDir);
	CorrectDirString(rfPluginDir);
	WritePrivateProfileString("PluginDirectories","AFPlugins",afPluginDir,szINIFileName);
	WritePrivateProfileString("PluginDirectories","RFPlugins",rfPluginDir,szINIFileName);
}

void CorrectCDDriveLetter(int *cddrive)
{
	char root[3],drive;
	int  i;

	root[1]=':';
    root[2]=0;
	i=0;
    for (drive='A';drive<='Z';drive++)
    {
		root[0]=drive;
		if (GetDriveType(root)==DRIVE_CDROM)
		{
			if (*cddrive==i)
				return;
			else
				i++;
		}
    }
	*cddrive=0;
}

int GetIntOption(const char* section, const char* key, IntOption* pio, int num, int def)
{
    int i;
    char str[256];

    GetPrivateProfileString(section,key,pio[def].str,str,sizeof(str),szINIFileName);
    for (i=0;i<num;i++)
		if (!lstrcmp(pio[i].str,str))
			return pio[i].value;

    return pio[def].value;
}

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void LoadOptions(void)
{
    char str[40],appDir[MAX_PATH];

    waveDeviceID=(UINT)GetPrivateProfileInt("Playback","WaveDeviceID",DEF_WAVEDEV,szINIFileName);
    bufferSize=1024*(DWORD)GetPrivateProfileInt("Playback","BufferSize",DEF_BUFFERSIZE,szINIFileName);
	GetPrivateProfileString("Playlist","PlayNext","on",str,sizeof(str),szINIFileName);
    opPlayNext=LoadOptionBool(str);
    opRepeatType=GetIntOption("Playlist","Repeat",repeatTypeOptions,sizeof(repeatTypeOptions)/sizeof(IntOption),2);
    GetPrivateProfileString("Playlist","Shuffle","off",str,sizeof(str),szINIFileName);
    opShuffle=LoadOptionBool(str);
    GetPrivateProfileString("Playlist","IntroScan","off",str,sizeof(str),szINIFileName);
    opIntroScan=LoadOptionBool(str);
    introScanLength=(DWORD)1000*GetPrivateProfileInt("Playlist","IntroScanLength",DEF_INTROSCANLENGTH,szINIFileName);
    GetPrivateProfileString("Playlist","AutoLoad","on",str,sizeof(str),szINIFileName);
    opAutoLoad=LoadOptionBool(str);
    GetPrivateProfileString("Playlist","AutoPlay","off",str,sizeof(str),szINIFileName);
    opAutoPlay=LoadOptionBool(str);
	GetPrivateProfileString("PlayerWindow","TimeMode","elapsed",str,sizeof(str),szINIFileName);
    opTimeElapsed=(BOOL)(!lstrcmpi(str,"elapsed"));
	GetPrivateProfileString("PlayerWindow","EasyMove","on",str,sizeof(str),szINIFileName);
    opEasyMove=LoadOptionBool(str);
	GetPrivateProfileString("Playlist","NoTime","on",str,sizeof(str),szINIFileName);
    opNoTime=LoadOptionBool(str);
    GetPrivateProfileString("Playlist","TreatCD","on",str,sizeof(str),szINIFileName);
    opTreatCD=LoadOptionBool(str);
    GetPrivateProfileString("Playlist","PutCD","on",str,sizeof(str),szINIFileName);
    opPutCD=LoadOptionBool(str);
    CDDrive=(int)GetPrivateProfileInt("Playlist","CDDrive",DEF_CDDRIVE,szINIFileName);
	CorrectCDDriveLetter(&CDDrive);
	opDropSupport=GetIntOption("Playlist","DropSupport",dropSupportOptions,sizeof(dropSupportOptions)/sizeof(IntOption),1);
	opPlaylistType=GetIntOption("Playlist","PlaylistType",playlistTypeOptions,sizeof(playlistTypeOptions)/sizeof(IntOption),1);
    SetPriorityClass(GetCurrentProcess(),GetPrivateProfileInt("Priority","PriorityClass",DEF_PRIORITYCLASS,szINIFileName));
    SetThreadPriority(GetCurrentThread(),GetPrivateProfileInt("Priority","MainPriority",DEF_MAINPRIORITY,szINIFileName));
	SetPlaybackPriority(GetPrivateProfileInt("Priority","PlaybackPriority",DEF_PLAYBACKPRIORITY,szINIFileName));
    GetPrivateProfileString("AllowMultiplePlugins","AFPlugins","off",str,sizeof(str),szINIFileName);
    opAllowMultipleAFPlugins=LoadOptionBool(str);
    GetPrivateProfileString("AllowMultiplePlugins","RFPlugins","off",str,sizeof(str),szINIFileName);
    opAllowMultipleRFPlugins=LoadOptionBool(str);
	multiSaveFileName=GetIntOption("Saving","MultiSaveFileName",multiSaveFileNameOptions,sizeof(multiSaveFileNameOptions)/sizeof(IntOption),1);
	GetPrivateProfileString("Saving","DefTemplateDir",DEF_TEMPLATEDIR,defTemplateDir,sizeof(defTemplateDir),szINIFileName);
	GetPrivateProfileString("Saving","DefTemplateFileTitle",DEF_TEMPLATEFILETITLE,defTemplateFileTitle,sizeof(defTemplateFileTitle),szINIFileName);
	GetPrivateProfileString("Saving","DefTemplateFileExt",DEF_TEMPLATEFILEEXT,defTemplateFileExt,sizeof(defTemplateFileExt),szINIFileName);
	CorrectDirString(defTemplateDir);
	CorrectExtString(defTemplateFileExt);
	lstrcpy(dlgTemplateDir,defTemplateDir);
	lstrcpy(dlgTemplateFileTitle,defTemplateFileTitle);
	lstrcpy(dlgTemplateFileExt,defTemplateFileExt);
	dlgWaveTag=waveTag=(WORD)GetPrivateProfileInt("Saving","MultiConvertWaveTag",DEF_WAVETAG,szINIFileName);
	GetHomeDirectory(appDir,sizeof(appDir));
	CorrectDirString(appDir);
	lstrcat(appDir,"Plugins\\");
	GetPrivateProfileString("PluginDirectories","AFPlugins",appDir,afPluginDir,sizeof(afPluginDir),szINIFileName);
	GetPrivateProfileString("PluginDirectories","RFPlugins",appDir,rfPluginDir,sizeof(rfPluginDir),szINIFileName);
	CorrectDirString(afPluginDir);
	CorrectDirString(rfPluginDir);
}
