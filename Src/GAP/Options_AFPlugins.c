/*
 * Game Audio Player source code: AF plug-ins options
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
#include <shlobj.h>

#include "Globals.h"
#include "Misc.h"
#include "Options.h"
#include "Options_AFPlugins.h"
#include "Plugins.h"
#include "Errors.h"
#include "Adding.h"
#include "resource.h"

void EnableButtons(HWND hwnd, BOOL cstate, BOOL astate)
{
    EnableWindow(GetDlgItem(hwnd,ID_PLUGINCONFIG),cstate);
    EnableWindow(GetDlgItem(hwnd,ID_PLUGINABOUT),astate);
}

LONG GetCurPlugin(HWND hwnd)
{
    int index;

    index=SendDlgItemMessage(hwnd,ID_PLUGINS,LB_GETCURSEL,(WPARAM)0,(LPARAM)0L);
    if (index!=LB_ERR)
		return SendDlgItemMessage(hwnd,ID_PLUGINS,LB_GETITEMDATA,(WPARAM)index,(LPARAM)0);
    else
		return (LONG)NULL;
}

void CheckCurAFSel(HWND hwnd)
{
    AFPlugin* plugin;

    if ((plugin=(AFPlugin*)GetCurPlugin(hwnd))==NULL)
		EnableButtons(hwnd,FALSE,FALSE);
    else
		EnableButtons(hwnd,(plugin->Config!=NULL),(plugin->About!=NULL));
}

LRESULT CALLBACK AFPluginsOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    char		 str[MAX_PATH+256],dir[MAX_PATH];
	char	     lpszTitle[]="Select audio file plug-in directory:";
    int 		 index;
    PluginNode	*pnode;
    AFPlugin	*plugin;
    LPNMHDR		 lpnmhdr;
	BROWSEINFO   bi;
	LPITEMIDLIST lpidl;

    switch (umsg)
    {
		case WM_INITDIALOG:
			SetDlgItemText(hwnd,ID_PLUGINDIR,"Audio file plug-in &directory (takes effect on startup)");
			SetDlgItemText(hwnd,ID_PLUGINLIST,"Audio file &plug-ins");
			pnode=afFirstPlugin;
			while (pnode!=NULL)
			{
				wsprintf(str,"%s %s (%s)",AFNODE(pnode)->Description,AFNODE(pnode)->Version,pnode->pluginFileName);
				index=(int)SendDlgItemMessage(hwnd,ID_PLUGINS,LB_ADDSTRING,0,(LPARAM)str);
				SendDlgItemMessage(hwnd,ID_PLUGINS,LB_SETITEMDATA,(WPARAM)index,(LPARAM)(AFNODE(pnode)));
				pnode=pnode->next;
			}
			SetCheckBox(hwnd,ID_PLUGINALLOW,opAllowMultipleAFPlugins);
			CorrectDirString(afPluginDir);
			SetDlgItemText(hwnd,ID_DIR,afPluginDir);

			CheckCurAFSel(hwnd);
			DisableApply(hwnd);

			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
					TrackPropPage(hwnd,3);
					opAllowMultipleAFPlugins=GetCheckBox(hwnd,ID_PLUGINALLOW);
					GetDlgItemText(hwnd,ID_DIR,afPluginDir,sizeof(afPluginDir));
					CorrectDirString(afPluginDir);
					DisableApply(hwnd);
					SetWindowLong(hwnd,DWL_MSGRESULT,PSNRET_NOERROR);
					break;
				case PSN_RESET:
					DisableApply(hwnd);
					TrackPropPage(hwnd,3);
					break;
				default:
					break;
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_DIR:
					GetDlgItemText(hwnd,ID_DIR,dir,sizeof(dir));
					if (!DirectoryExists(dir))
						CreateDirectoryRecursive(dir);
					bi.hwndOwner=hwnd;
					bi.pidlRoot=NULL;
					bi.pszDisplayName=dir;
					bi.lpszTitle=lpszTitle;
					bi.ulFlags=BIF_RETURNONLYFSDIRS;
					bi.lpfn=BrowseProc;
					if (dir[lstrlen(dir)-1]=='\\')
						dir[lstrlen(dir)-1]=0;
					bi.lParam=(LPARAM)dir;
					if ((lpidl=SHBrowseForFolder(&bi))!=NULL)
					{
						SHGetPathFromIDList(lpidl,dir);
						CorrectDirString(dir);
						SetDlgItemText(hwnd,ID_DIR,dir);
						EnableApply(hwnd);
					}
					break;
				case ID_PLUGINS:
					switch (HIWORD(wparm))
					{
						case LBN_DBLCLK:
							CheckCurAFSel(hwnd);
							plugin=(AFPlugin*)GetCurPlugin(hwnd);
							if ((plugin!=NULL) && (plugin->Config!=NULL))
								plugin->Config(hwnd);
							break;
						case LBN_KILLFOCUS:
						case LBN_SETFOCUS:
						case LBN_SELCANCEL:
						case LBN_SELCHANGE:
							CheckCurAFSel(hwnd);
							break;
						default:
							break;
					}
					break;
				case ID_PLUGINCONFIG:
					plugin=(AFPlugin*)GetCurPlugin(hwnd);
					if ((plugin!=NULL) && (plugin->Config!=NULL))
						plugin->Config(hwnd);
					break;
				case ID_PLUGINABOUT:
					plugin=(AFPlugin*)GetCurPlugin(hwnd);
					if ((plugin!=NULL) && (plugin->About!=NULL))
						plugin->About(hwnd);
					break;
				case ID_PLUGINALLOW:
					EnableApply(hwnd);
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
