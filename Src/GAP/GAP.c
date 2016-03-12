/*
 * Game Audio Player source code: WinMain and message loop
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
#include <commctrl.h>

#include "Globals.h"
#include "PlayDialog.h"
#include "Playlist.h"
#include "Misc.h"
#include "resource.h"

//global variables

HINSTANCE hInst;
HMENU     hMenu;
char	  szAppName[]="Game Audio Player";
char	  szINIFileName[MAX_PATH];
char	  szAppDirectory[MAX_PATH];

int APIENTRY WinMain
(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR	  lpCmdLine,
	int	      nCmdShow
)
{
	MSG    msg;
	HWND   hwnd;
	HACCEL hAccelPlayer;
	HACCEL hAccelPlaylist;

    hInst=hInstance;

	InitCommonControls();

	GetHomeDirectory(szAppDirectory,sizeof(szAppDirectory));
    ReplaceAppFileExtension(szINIFileName,sizeof(szINIFileName),".ini");

	hAccelPlayer=LoadAccelerators(hInst,(LPCTSTR)IDR_PLAYER);
	hAccelPlaylist=LoadAccelerators(hInst,(LPCTSTR)IDR_PLAYLIST);
	hMenu=LoadMenu(hInst,(LPCTSTR)IDR_MAINMENU);

    hwnd=CreateDialog(hInstance, "PlayDialog", GetFocus(), (DLGPROC)PlayDialog);
	while (GetMessage(&msg,NULL,0,0))
	{
		if (inEdit)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (
			    (!TranslateAccelerator(hwnd,hAccelPlayer,&msg)) &&
			    (!TranslateAccelerator(playlistWnd,hAccelPlaylist,&msg))
		       )
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	DestroyWindow(hwnd);

	DestroyMenu(hMenu);
	DestroyAcceleratorTable(hAccelPlaylist);
	DestroyAcceleratorTable(hAccelPlayer);

    return (int)msg.wParam;
}
