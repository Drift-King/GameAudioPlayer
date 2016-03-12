/*
 * fDAT Plug-in source code: RF plug-in interface
 *
 * Copyright (C) 2000 Alexander Belyakov
 * E-mail: abel@krasu.ru
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

#include "FileList.h"
#include "Funcs.h"
#include "resource.h"

#include "..\RFPlugin.h"
#include "..\..\..\Common\TreeSel/TreeSel.h"


LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm) {
	switch (umsg) {
	case WM_INITDIALOG:
		CenterInParent (hwnd);
		break;
	case WM_CLOSE:
		EndDialog (hwnd, 1);
	case WM_COMMAND:
		if (LOWORD(wparm) == IDOK)
			EndDialog (hwnd, 1);
		else
			return FALSE;
	default:
		return FALSE;
	}
	return TRUE;
}

extern RFPlugin Plugin;

void __stdcall PluginAbout (HWND hwnd) {
	DialogBox (Plugin.hDllInst, MAKEINTRESOURCE(ABOUT_DIALOG), hwnd, (DLGPROC)AboutDlgProc);
}

// Not necessary in current version.
//void __stdcall PluginConfig (HWND hwnd) {}
//void __stdcall PluginInit () {}
//void __stdcall PluginQuit () {}

RFPlugin Plugin = {
	RFP_VER,
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
	"Return 0; Fallout DAT Resource File Plug-In",
	"v0.10",
	"Fallout DAT Resource Files (*.DAT)\0*.dat\0",
	"fDAT",
	NULL,
	PluginAbout,
	NULL,
	NULL,
	PluginGetFileList,
    PluginFreeFileList,
    PluginOpenFile,
    PluginCloseFile,
    PluginGetFileSize,
    PluginGetFilePointer,
    PluginEndOfFile,
    PluginSetFilePointer,
    PluginReadFile
};

extern "C" __declspec(dllexport) RFPlugin* __stdcall GetResourceFilePlugin(void)
{
    return &Plugin;
}
