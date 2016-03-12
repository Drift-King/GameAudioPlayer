/*
 * Game Audio Player source code: playlist dialog box or window
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

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Globals.h"
#include "Misc.h"
#include "Playlist.h"
#include "ListView.h"
#include "Progress.h"
#include "PlayDialog.h"
#include "FSHandler.h"
#include "Plugins.h"
#include "Player.h"
#include "Errors.h"
#include "Convert.h"
#include "Options.h"
#include "Info.h"
#include "Playlist_MultiOps.h"
#include "Playlist_SaveLoad.h"
#include "Playlist_NodeOps.h"
#include "Adding.h"
#include "Scanning.h"
#include "resource.h"

int  opPlaylistType;
HWND playlistWnd,playlistToolbar;
BOOL inEdit;

TBBUTTON tbPlaylistButtons[]=
{
    {5, ID_CLEAR,    TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_CLEARLIST,  0},
    {1, ID_LOAD,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_LOADLIST,   0},
    {2, ID_SAVE,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_SAVELIST,   0},
    {0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{13,ID_ADD,      TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_ADDFILE,    0},
	{17,ID_ADDDIR,   TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_ADDDIR,     0},
	{14,ID_FIND,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_SCANFILE,   0},
	{18,ID_SCANDIR,  TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_SCANDIR,    0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{0, ID_PLAY,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_PLAY,       0},
	{20,ID_STOP,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_STOP,       0},
	{21,ID_PREV,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_PREVIOUS,   0},
	{22,ID_NEXT,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_NEXT,       0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{15,ID_SAVEFILE, TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_SAVEFILE,   0},
	{16,ID_CONVERT,  TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_CONVERTFILE,0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{6, ID_MOVEUP,   TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_MOVEUP,     0},
	{7, ID_MOVEDOWN, TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_MOVEDOWN,   0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{3, ID_REMOVE,   TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_REMOVE,     0},
	{9, ID_DUPLICATE,TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_DUPLICATE,  0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{4, ID_FILEINFO, TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_FILEINFO,   0},
	{8, ID_TITLE,    TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_FILETITLE,  0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{11,ID_SELECTALL,TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_SELECTALL,  0},
	{12,ID_INVERTSEL,TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_INVERTSEL,  0},
	{0, 0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,              0},
	{10,ID_SORT,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_SORTLIST,   0}
};

int iSortMessageID[]=
{
	ID_SORT_AFTITLE,
	ID_SORT_AFTIME,
	ID_SORT_AFTYPE,
	ID_SORT_RFNAME,
	ID_SORT_RFTYPE,
	ID_SORT_RFDATA,
	ID_SORT_START,
	ID_SORT_LENGTH
};

LRESULT CALLBACK PlaylistDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm);
LRESULT CALLBACK PlaylistWndProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm);

char szPlaylistClassName[]="ANX Playlist";

void InitPlaylist(void)
{
	WNDCLASSEX wc;

    cursel=NULL;
    curNode=NULL;
    list_start=NULL;
    list_end=NULL;
    list_size=0;
    lstrcpy(list_filename,"");
    list_changed=FALSE;
    playList=NULL;
	playlistWnd=NULL;
	playlistToolbar=NULL;
	inEdit=FALSE;

	wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = (WNDPROC)PlaylistWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(hInst,"PlaylistIcon");
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_PLAYLISTMENU);
    wc.lpszClassName = szPlaylistClassName;
	wc.hIconSm       = LoadImage(hInst,
	                             "PlaylistIcon",
	                             IMAGE_ICON,
                                 GetSystemMetrics(SM_CXSMICON),
                                 GetSystemMetrics(SM_CYSMICON),
	                             0);
    if (!RegisterClassEx(&wc))
        RegisterClass((LPWNDCLASS)&wc.style);
}

void ShowPlaylist(HWND hwnd)
{
    if (playlistWnd==NULL)
	{
		switch (opPlaylistType)
		{
			case ID_PLAYLIST_DIALOG:
				playlistWnd=CreateDialog(hInst,"PlaylistDialog",hwnd,(DLGPROC)PlaylistDlgProc);
				break;
			default: // ???
			case ID_PLAYLIST_WINDOW:
				playlistWnd=CreateWindow(szPlaylistClassName,
										 "GAP Playlist",
										 WS_POPUPWINDOW | WS_CAPTION | WS_MAXIMIZEBOX | 
										 WS_MINIMIZEBOX | WS_SIZEBOX | WS_VISIBLE,
										 CW_USEDEFAULT,
										 CW_USEDEFAULT,
										 500,300,
										 hwnd,
										 NULL,
										 hInst,
										 NULL
										);
				ShowWindow(playlistWnd,SW_SHOWNORMAL);
				UpdateWindow(playlistWnd);
		}
	}
    else
	{
		SetFocus(hwnd); // ???
		DestroyWindow(playlistWnd);
		UpdateWindow(hwnd);
		playlistWnd=NULL;
	}
}

int AskSavePlaylist(HWND hwnd)
{
    char str[MAX_PATH+100];

    if ((list_changed) && (list_size!=0))
    {
		wsprintf(str,"%s has changed.\nWould you like to save it?",(lstrcmpi(list_filename,""))?GetFileTitleEx(list_filename):"Playlist");
		return MessageBox(GetFocus(),str,szAppName,MB_ICONQUESTION | MB_YESNOCANCEL);
    }
    else
		return IDIGNORE;
}

void EnableIfNeeded(HWND hwnd, int id, BOOL state)
{
	if (IsWindowEnabled(GetDlgItem(hwnd,id))!=state)
		EnableWindow(GetDlgItem(hwnd,id),state);
}

#define EnablePlaylistSubMenuItemByID(hwnd,isubmenu,id,b) EnableMenuItem(GetSubMenu(GetMenu(hwnd),isubmenu),id,((b)?MF_ENABLED:MF_GRAYED) | MF_BYCOMMAND)
#define EnablePlaylistMenuItemByPos(hwnd,pos,b) EnableMenuItem(GetMenu(hwnd),pos,((b)?MF_ENABLED:MF_GRAYED) | MF_BYPOSITION)
#define EnableToolbarButton(hwnd,id,b) SendMessage(hwnd,TB_ENABLEBUTTON,(WPARAM)(id),(LPARAM)MAKELONG((b),0))

void Selection(HWND hwnd, int num)
{
	BOOL single=(BOOL)(num==1),
		 selected=(BOOL)(num>0);

	switch (opPlaylistType)
	{
		case ID_PLAYLIST_DIALOG:
			EnableIfNeeded(hwnd,ID_PLAY,single);
			EnableIfNeeded(hwnd,ID_MOVEUP,single);
			EnableIfNeeded(hwnd,ID_MOVEDOWN,single);
			EnableIfNeeded(hwnd,ID_REMOVE,selected);
			EnableIfNeeded(hwnd,ID_DUPLICATE,single);
			EnableIfNeeded(hwnd,ID_FILEINFO,single);
			EnableIfNeeded(hwnd,ID_TITLE,single);
			EnableIfNeeded(hwnd,ID_SAVEFILE,selected);
			EnableIfNeeded(hwnd,ID_CONVERT,selected);
			EnableIfNeeded(hwnd,ID_SELECT,selected); // ???
			EnableIfNeeded(hwnd,ID_SORT,selected); // ???
			break;
		default: // ???
		case ID_PLAYLIST_WINDOW:
			EnablePlaylistSubMenuItemByID(hwnd,0,ID_PLAY,single);
			EnablePlaylistSubMenuItemByID(hwnd,1,ID_MOVEUP,single);
			EnablePlaylistSubMenuItemByID(hwnd,1,ID_MOVEDOWN,single);
			EnablePlaylistSubMenuItemByID(hwnd,1,ID_REMOVE,selected);
			EnablePlaylistSubMenuItemByID(hwnd,1,ID_DUPLICATE,single);
			EnablePlaylistSubMenuItemByID(hwnd,1,ID_FILEINFO,single);
			EnablePlaylistSubMenuItemByID(hwnd,1,ID_TITLE,single);
			EnablePlaylistSubMenuItemByID(hwnd,0,ID_SAVEFILE,selected);
			EnablePlaylistSubMenuItemByID(hwnd,0,ID_CONVERT,selected);
			EnablePlaylistMenuItemByPos(hwnd,2,selected); // ???
			EnablePlaylistMenuItemByPos(hwnd,3,selected); // ???
			DrawMenuBar(hwnd);

			EnableToolbarButton(playlistToolbar,ID_MOVEUP,single);
			EnableToolbarButton(playlistToolbar,ID_MOVEDOWN,single);
			EnableToolbarButton(playlistToolbar,ID_REMOVE,selected);
			EnableToolbarButton(playlistToolbar,ID_DUPLICATE,single);
			EnableToolbarButton(playlistToolbar,ID_FILEINFO,single);
			EnableToolbarButton(playlistToolbar,ID_TITLE,single);
			EnableToolbarButton(playlistToolbar,ID_SAVEFILE,selected);
			EnableToolbarButton(playlistToolbar,ID_CONVERT,selected);
			EnableToolbarButton(playlistToolbar,ID_SELECTALL,selected); // ???
			EnableToolbarButton(playlistToolbar,ID_INVERTSEL,selected); // ???
			EnableToolbarButton(playlistToolbar,ID_SORT,selected); // ???
	}
}

void TrackPlaylistNodeMenu(HWND hwnd)
{
	HMENU hSubMenu;
	POINT point;

	hSubMenu=GetSubMenu(hMenu,1);
	GetCursorPos(&point);
	TrackPopupMenu(hSubMenu,TPM_LEFTBUTTON | TPM_RIGHTBUTTON,point.x,point.y,0,hwnd,NULL);
}

void TrackControlMenu(HWND hwnd, int id, HMENU hmenu)
{
	RECT rect;

	GetWindowRect(GetDlgItem(hwnd,id),&rect);
	TrackPopupMenu(hmenu,TPM_LEFTBUTTON | TPM_RIGHTBUTTON,rect.left,rect.top,0,hwnd,NULL);
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	AFile *node1,*node2;

	node1=(AFile*)lParam1;
	node2=(AFile*)lParam2;
	switch (LOWORD(lParamSort))
	{
		case ID_SORT_AFTITLE:
			return strcmp(node1->afName,node2->afName);
		case ID_SORT_AFTIME:
			return (int)((node1->afTime)-(node2->afTime));
		case ID_SORT_AFTYPE:
			return strcmp(node1->afID,node2->afID);
		case ID_SORT_RFTITLE:
			return strcmp(GetFileTitleEx(node1->rfName),GetFileTitleEx(node2->rfName));
		case ID_SORT_RFNAME:
			return strcmp(node1->rfName,node2->rfName);
		case ID_SORT_RFTYPE:
			return strcmp(node1->rfID,node2->rfID);
		case ID_SORT_START:
			return (int)((node1->fsStart)-(node2->fsStart));
		case ID_SORT_LENGTH:
			return (int)((node1->fsLength)-(node2->fsLength));
		case ID_SORT_AFDATA:
			return strcmp(node1->afDataString,node2->afDataString);
		case ID_SORT_RFDATA:
			return strcmp(node1->rfDataString,node2->rfDataString);
		case ID_SORT_REVERSE:
		case ID_SORT_RANDOMIZE:
			return (int)((node2->dwData)-(node1->dwData));
		default:
			return 0;
	}
}

LRESULT CALLBACK PlaylistDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    int          index,posx,posy,count;
    AFile       *node;
	POINT        point;
	RECT         rect;
	HWND         waitwnd;

	static BOOL  bIsDragging=FALSE;
	static POINT dragPoint;
	static HWND  oldCapture;

    switch (umsg)
    {
		case WM_INITDIALOG:
			LoadWndPos("PlaylistWindow",&posx,&posy);
			if ((posx!=DEF_POS) && (posy!=DEF_POS))
				SetWindowPos(hwnd,NULL,posx,posy,0,0,SWP_NOSIZE | SWP_NOZORDER);
			else
				CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			playList=GetDlgItem(hwnd,ID_PLAYLIST);
			playlistToolbar=NULL;
			InitListView();
			Selection(hwnd,ListView_GetSelectedCount(playList));
			DragAcceptFiles(hwnd,(BOOL)(opDropSupport!=ID_DROP_OFF));
			return TRUE;
		case WM_DROPFILES:
			SetForegroundWindow(hwnd);
			OnDropFiles(hwnd,(HDROP)wparm);
			break;
		case WM_RBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
			TrackMainMenu(hwnd);
			return TRUE;
		case WM_NCLBUTTONDOWN:
			if (
				((INT)wparm==HTCLOSE) ||
				((INT)wparm==HTERROR) ||
				((INT)wparm==HTSIZE) ||
				((INT)wparm==HTHELP) ||
				((INT)wparm==HTMENU) ||
				((INT)wparm==HTMAXBUTTON) ||
				((INT)wparm==HTMINBUTTON) ||
				((INT)wparm==HTREDUCE) ||
				((INT)wparm==HTZOOM) ||
				((INT)wparm==HTSYSMENU)
			   )
				break;
		case WM_LBUTTONDOWN:
			if (opEasyMove)
			{
				GetWindowRect(hwnd,&rect);
				dragPoint.x=(LONG)LOWORD(lparm);
				dragPoint.y=(LONG)HIWORD(lparm);
				if (umsg==WM_LBUTTONDOWN)
					ClientToScreen(hwnd,&dragPoint);
				dragPoint.x-=rect.left;
				dragPoint.y-=rect.top;
				oldCapture=SetCapture(hwnd);
				bIsDragging=TRUE;
			}
			break;
		case WM_NCLBUTTONUP:
		case WM_LBUTTONUP:
			if (opEasyMove)
			{
				bIsDragging=FALSE;
				if (oldCapture==NULL)
					ReleaseCapture();
				else
					SetCapture(oldCapture);
			}
			break;
		case WM_MOUSEMOVE:
			if ((opEasyMove) && (bIsDragging))
			{
				point.x=(LONG)LOWORD(lparm);
				point.y=(LONG)HIWORD(lparm);
				ClientToScreen(hwnd,&point);
				point.x-=dragPoint.x;
				point.y-=dragPoint.y;
				SetWindowPos(hwnd,NULL,point.x,point.y,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
			}
			break;
		case WM_DESTROY:
			SaveWndPos(hwnd,"PlaylistWindow",-1);
			playList=NULL;
			playlistToolbar=NULL;
			break;
		case WM_CLOSE:
			if (inEdit)
				break;
			SetFocus(GetParent(hwnd));
			DestroyWindow(hwnd);
			UpdateWindow(GetParent(hwnd));
			playlistWnd=NULL;
			cursel=NULL;
			break;
		case WM_NOTIFY:
			if (wparm==ID_PLAYLIST)
			{
				LPNMHDR 	 hdr=(LPNMHDR)lparm;
				LV_DISPINFO *lvdi=(LV_DISPINFO*)lparm;

				switch (hdr->code)
				{
					case LVN_COLUMNCLICK:
						PostMessage(hwnd,WM_COMMAND,(WPARAM)iSortMessageID[((NM_LISTVIEW*)lparm)->iSubItem],0L);
						break;
					case LVN_ITEMCHANGED:
					case LVN_KEYDOWN:
						if (!inEdit)
							Selection(hwnd,ListView_GetSelectedCount(playList));
						break;
					case NM_RCLICK:
					case NM_RDBLCLK:
						EnsureSelection(TRUE);
						TrackPlaylistNodeMenu(hwnd);
						break;
					case NM_CLICK:
						EnsureSelection(TRUE);
						break;
					case NM_DBLCLK:
						EnsureSelection(TRUE);
						Play(GetListViewNode(GetCurrentSelection()));
						break;
					case LVN_BEGINLABELEDIT:
						inEdit=TRUE;
						break;
					case LVN_ENDLABELEDIT:
						inEdit=FALSE;
						if (lvdi->item.pszText!=NULL)
						{
							node=GetListViewNode(lvdi->item.iItem);
							if (node!=NULL)
							{
								lstrcpy(node->afName,lvdi->item.pszText);
								ListView_SetItemText(playList,lvdi->item.iItem,0,node->afName);
								list_changed=TRUE;
							}
							ShowStats();
						}
						break;
					default:
						break;
				}
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_PLAY:
					Play(GetListViewNode(GetCurrentSelection()));
					break;
				case ID_FILEINFO:
					node=GetListViewNode(GetCurrentSelection());
					if (
						(node!=NULL) &&
						(AFPLUGIN(node)!=NULL) &&
						(AFPLUGIN(node)->InfoBox!=NULL)
					   )
						AFPLUGIN(node)->InfoBox(node,hwnd);
					break;
				case ID_LOAD:
					Load(hwnd);
					break;
				case ID_SAVE:
					if (ListView_GetItemCount(playList)>0)
						Save(hwnd);
					break;
				case ID_FIND:
					Find(hwnd);
					break;
				case ID_SCANDIR:
					ScanDir(hwnd);
					break;
				case ID_ADD:
					Add(hwnd);
					break;
				case ID_ADDDIR:
					AddDir(hwnd);
					break;
				case ID_CLEAR:
					switch (AskSavePlaylist(hwnd))
					{
						case IDYES:
							Save(hwnd);
						case IDNO:
						case IDIGNORE:
							Clear(hwnd);
						case IDCANCEL:
							break;
					}
					break;
				case ID_REMOVE:
					Remover(hwnd);
					break;
				case ID_DUPLICATE:
					DuplicateNode(GetCurrentSelection());
					break;
				case ID_MOVEUP:
					MoveNodeUp(GetCurrentSelection());
					break;
				case ID_MOVEDOWN:
					MoveNodeDown(GetCurrentSelection());
					break;
				case ID_TITLE:
					index=GetCurrentSelection();
					if (index!=NO_SELECTION)
					{
						SetFocus(playList);
						ListView_EditLabel(playList,index);
					}
					break;
				case ID_SAVEFILE:
					Saver(hwnd);
					break;
				case ID_CONVERT:
					Converter(hwnd);
					break;
				case ID_SELECT:
					TrackControlMenu(hwnd,LOWORD(wparm),GetSubMenu(hMenu,2));
					break;
				case ID_SELECTALL:
					count=ListView_GetItemCount(playList);
					for (index=0;index<count;index++)
						ListView_SetItemState(playList,index,LVIS_SELECTED,LVIS_SELECTED);
					break;
				case ID_INVERTSEL:
					count=ListView_GetItemCount(playList);
					for (index=0;index<count;index++)
					{
						if (ListView_GetItemState(playList,index,LVIS_SELECTED) & LVIS_SELECTED)
						{
							ListView_SetItemState(playList,index,0,LVIS_SELECTED);
						}
						else
							ListView_SetItemState(playList,index,LVIS_SELECTED,LVIS_SELECTED);
					}
					EnsureSelection(FALSE);
					break;
				case ID_SORT:
					TrackControlMenu(hwnd,LOWORD(wparm),GetSubMenu(GetSubMenu(hMenu,1),8));
					break;
				case ID_SORT_REVERSE:
				case ID_SORT_RANDOMIZE:
					count=ListView_GetItemCount(playList);
					if (count<=0)
						break;
					srand((UINT)GetTickCount());
					waitwnd=ShowWaitWindow(hwnd,"Sorting list","Walking the playlist items. Please wait...");
					for (index=0;index<count;index++)
					{
						node=GetListViewNode(index);
						if (node!=NULL)
							node->dwData=(DWORD)((LOWORD(wparm)==ID_SORT_RANDOMIZE)?(rand()):index);
					}
					CloseWaitWindow(waitwnd);
				case ID_SORT_AFTITLE:
				case ID_SORT_AFTIME:
				case ID_SORT_AFTYPE:
				case ID_SORT_RFTITLE:
				case ID_SORT_RFNAME:
				case ID_SORT_RFTYPE:
				case ID_SORT_START:
				case ID_SORT_LENGTH:
				case ID_SORT_AFDATA:
				case ID_SORT_RFDATA:
					count=ListView_GetItemCount(playList);
					if (count<=0)
						break;
					waitwnd=ShowWaitWindow(hwnd,"Sorting list","Sorting the playlist. Please wait...");
					ListView_SortItems(playList,(PFNLVCOMPARE)CompareFunc,(LPARAM)LOWORD(wparm));
					BuildNodeChain(playList);
					ListView_EnsureVisible(playList,GetCurrentSelection(),FALSE);
					CloseWaitWindow(waitwnd);
					break;
				case IDOK:
				case ID_LIST:
					if (inEdit)
						break;
					SetFocus(playDialog);
					DestroyWindow(hwnd);
					UpdateWindow(playDialog);
					playlistWnd=NULL;
					cursel=NULL;
					break;
				case ID_INFO:
				case ID_OPTIONS:
				case ID_EQUALIZER:
				case ID_TIME_TOGGLE:
				case ID_TIME_ELAPSED:
				case ID_TIME_REMAINING:
				case ID_ONTOP:
				case ID_EASYMOVE:
				case ID_PREV:
				case ID_NEXT:
				case ID_STOP:
				case ID_PLAYNEXT:
				case ID_SHUFFLE:
				case ID_INTROSCAN:
				case ID_REPEAT:
				case ID_REPEAT_ONE:
				case ID_REPEAT_ALL:
				case IDQUIT:
					PostMessage(playDialog,WM_COMMAND,LOWORD(wparm),0L);
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


void InitPlaylistToolbarTooltips(void)
{
    tbPlaylistButtons[0].dwData= IDS_CLEARLIST;
    tbPlaylistButtons[1].dwData= IDS_LOADLIST;
    tbPlaylistButtons[2].dwData= IDS_SAVELIST;
    tbPlaylistButtons[3].dwData= 0;
	tbPlaylistButtons[4].dwData= IDS_ADDFILE;
	tbPlaylistButtons[5].dwData= IDS_ADDDIR;
	tbPlaylistButtons[6].dwData= IDS_SCANFILE;
	tbPlaylistButtons[7].dwData= IDS_SCANDIR;
	tbPlaylistButtons[8].dwData= 0;
	tbPlaylistButtons[9].dwData= IDS_PLAY;
	tbPlaylistButtons[10].dwData=IDS_STOP;
	tbPlaylistButtons[11].dwData=IDS_PREVIOUS;
	tbPlaylistButtons[12].dwData=IDS_NEXT;
	tbPlaylistButtons[13].dwData=0;
	tbPlaylistButtons[14].dwData=IDS_SAVEFILE;
	tbPlaylistButtons[15].dwData=IDS_CONVERTFILE;
	tbPlaylistButtons[16].dwData=0;
	tbPlaylistButtons[17].dwData=IDS_MOVEUP;
	tbPlaylistButtons[18].dwData=IDS_MOVEDOWN;
	tbPlaylistButtons[19].dwData=0;
	tbPlaylistButtons[20].dwData=IDS_REMOVE;
	tbPlaylistButtons[21].dwData=IDS_DUPLICATE;
	tbPlaylistButtons[22].dwData=0;
	tbPlaylistButtons[23].dwData=IDS_FILEINFO;
	tbPlaylistButtons[24].dwData=IDS_FILETITLE;
	tbPlaylistButtons[25].dwData=0;
	tbPlaylistButtons[26].dwData=IDS_SELECTALL;
	tbPlaylistButtons[27].dwData=IDS_INVERTSEL;
	tbPlaylistButtons[28].dwData=0;
	tbPlaylistButtons[29].dwData=IDS_SORTLIST;
}

void PlaylistToolbarPlayToggle(BOOL playing)
{
	int index;

	if (playing)
	{
		SendMessage(playlistToolbar,TB_CHANGEBITMAP,(WPARAM)ID_PLAY,(LPARAM)MAKELPARAM(19,0));
		index=SendMessage(playlistToolbar,TB_COMMANDTOINDEX,(WPARAM)ID_PLAY,0L);
		tbPlaylistButtons[index].dwData=IDS_PAUSE;
	}
	else
	{
		SendMessage(playlistToolbar,TB_CHANGEBITMAP,(WPARAM)ID_PLAY,(LPARAM)MAKELPARAM(0,0));
		index=SendMessage(playlistToolbar,TB_COMMANDTOINDEX,(WPARAM)ID_PLAY,0L);
		tbPlaylistButtons[index].dwData=IDS_PLAY;
	}
}

LRESULT CALLBACK PlaylistWndProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    int           index,posx,posy,sizex,sizey,count;
    AFile        *node;
	RECT          rect={0},tbrect={0};
	POINT         point;
	HWND          waitwnd;
	LPNMHDR 	  hdr;
	LV_DISPINFO  *lvdi;

    switch (umsg)
    {
		case WM_CREATE:
			LoadWndPos("PlaylistWindow",&posx,&posy);
			LoadWndSize("PlaylistWindow",&sizex,&sizey);
			SetWindowPos(
						 hwnd,
						 NULL,
						 posx,posy,sizex,sizey,
						 (((posx==DEF_POS) || (posy==DEF_POS))?SWP_NOMOVE:0) |
						 (((sizex==DEF_POS) || (sizey==DEF_POS))?SWP_NOSIZE:0) | 
						 SWP_NOZORDER
						);
			if ((posx==DEF_POS) || (posy==DEF_POS))
				CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			InitPlaylistToolbarTooltips();
			playlistToolbar=CreateToolbarEx(hwnd,
											WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS,
											ID_TOOLBAR,
											23,
											hInst,
											ID_PLAYLISTBUTTONS,
											tbPlaylistButtons,
											sizeof(tbPlaylistButtons)/sizeof(TBBUTTON),
											0,0,16,16,sizeof(TBBUTTON)
										   );
			if ((isPlaying) && (!isPaused))
				SendMessage(playlistToolbar,TB_CHANGEBITMAP,(WPARAM)ID_PLAY,(LPARAM)MAKELPARAM(19,0));
			ShowWindow(playlistToolbar,SW_SHOWNORMAL);
			UpdateWindow(playlistToolbar);
			GetClientRect(hwnd,&rect);
			GetWindowRect(playlistToolbar,&tbrect);
			rect.top+=tbrect.bottom-tbrect.top;
			playList=CreateWindowEx(WS_EX_CLIENTEDGE,
								    WC_LISTVIEW,
								    "",
								    LVS_REPORT | LVS_EDITLABELS | LVS_ALIGNLEFT | LVS_SHOWSELALWAYS | 
								    WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_CHILD,
								    rect.left,
								    rect.top,
								    rect.right-rect.left,
								    rect.bottom-rect.top,
								    hwnd,
								    (HMENU)ID_PLAYLIST,
								    hInst,
								    NULL
							       );
			ShowWindow(playList,SW_SHOWNORMAL);
			UpdateWindow(playList);
			InitListView();
			Selection(hwnd,ListView_GetSelectedCount(playList));
			DragAcceptFiles(hwnd,(BOOL)(opDropSupport!=ID_DROP_OFF));
			break;
		case WM_SIZE:
			GetClientRect(hwnd,&rect);
			GetWindowRect(playlistToolbar,&tbrect);
			MoveWindow(playlistToolbar,rect.left,rect.top,rect.right-rect.left,tbrect.bottom-tbrect.top,TRUE);
			rect.top+=tbrect.bottom-tbrect.top;
			MoveWindow(playList,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,TRUE);
			UpdatePlaylistSelection();
			break;
		case WM_DROPFILES:
			SetForegroundWindow(hwnd);
			OnDropFiles(hwnd,(HDROP)wparm);
			break;
		case WM_RBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
			TrackMainMenu(hwnd);
			return TRUE;
		case WM_DESTROY:
			SaveWndPos(hwnd,"PlaylistWindow",-1);
			SaveWndSize(hwnd,"PlaylistWindow");
			playList=NULL;
			playlistToolbar=NULL;
			break;
		case WM_CLOSE:
			if (inEdit)
				break;
			SetFocus(GetParent(hwnd));
			DestroyWindow(hwnd);
			UpdateWindow(GetParent(hwnd));
			playlistWnd=NULL;
			cursel=NULL;
			break;
		case WM_NOTIFY:
			hdr=(LPNMHDR)lparm;
			if (hdr->code==TTN_NEEDTEXT)
			{
				index=(int)SendMessage(playlistToolbar,TB_COMMANDTOINDEX,(WPARAM)(hdr->idFrom),0L);
				((LPTOOLTIPTEXT)lparm)->lpszText=(LPTSTR)(tbPlaylistButtons[index].dwData);
				((LPTOOLTIPTEXT)lparm)->hinst=hInst;
				break;
			}
			switch (wparm)
			{
				case ID_PLAYLIST:
					lvdi=(LV_DISPINFO*)lparm;
					switch (hdr->code)
					{
						case LVN_COLUMNCLICK:
							PostMessage(hwnd,WM_COMMAND,(WPARAM)iSortMessageID[((NM_LISTVIEW*)lparm)->iSubItem],0L);
							break;
						case LVN_ITEMCHANGED:
						case LVN_KEYDOWN:
							if (!inEdit)
								Selection(hwnd,ListView_GetSelectedCount(playList));
							break;
						case NM_RCLICK:
						case NM_RDBLCLK:
							EnsureSelection(TRUE);
							TrackPlaylistNodeMenu(hwnd);
							break;
						case NM_CLICK:
							EnsureSelection(TRUE);
							break;
						case NM_DBLCLK:
							EnsureSelection(TRUE);
							Play(GetListViewNode(GetCurrentSelection()));
							break;
						case LVN_BEGINLABELEDIT:
							inEdit=TRUE;
							break;
						case LVN_ENDLABELEDIT:
							inEdit=FALSE;
							if (lvdi->item.pszText!=NULL)
							{
								node=GetListViewNode(lvdi->item.iItem);
								if (node!=NULL)
								{
									lstrcpy(node->afName,lvdi->item.pszText);
									ListView_SetItemText(playList,lvdi->item.iItem,0,node->afName);
									list_changed=TRUE;
								}
								ShowStats();
							}
							break;
						default:
							return DefWindowProc(hwnd,umsg,wparm,lparm);
					}
					break;
				default: // ???
					return DefWindowProc(hwnd,umsg,wparm,lparm);
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_PLAY:
					if (!isPlaying)
						Play(GetListViewNode(GetCurrentSelection()));
					else
						Pause();
					break;
				case ID_FILEINFO:
					node=GetListViewNode(GetCurrentSelection());
					if (
						(node!=NULL) &&
						(AFPLUGIN(node)!=NULL) &&
						(AFPLUGIN(node)->InfoBox!=NULL)
					   )
						AFPLUGIN(node)->InfoBox(node,hwnd);
					break;
				case ID_LOAD:
					Load(hwnd);
					break;
				case ID_SAVE:
					if (ListView_GetItemCount(playList)>0)
						Save(hwnd);
					break;
				case ID_FIND:
					Find(hwnd);
					break;
				case ID_SCANDIR:
					ScanDir(hwnd);
					break;
				case ID_ADD:
					Add(hwnd);
					break;
				case ID_ADDDIR:
					AddDir(hwnd);
					break;
				case ID_CLEAR:
					switch (AskSavePlaylist(hwnd))
					{
						case IDYES:
							Save(hwnd);
						case IDNO:
						case IDIGNORE:
							Clear(hwnd);
						case IDCANCEL:
							break;
					}
					break;
				case ID_REMOVE:
					Remover(hwnd);
					break;
				case ID_DUPLICATE:
					DuplicateNode(GetCurrentSelection());
					break;
				case ID_MOVEUP:
					MoveNodeUp(GetCurrentSelection());
					break;
				case ID_MOVEDOWN:
					MoveNodeDown(GetCurrentSelection());
					break;
				case ID_TITLE:
					index=GetCurrentSelection();
					if (index!=NO_SELECTION)
					{
						SetFocus(playList);
						ListView_EditLabel(playList,index);
					}
					break;
				case ID_SAVEFILE:
					Saver(hwnd);
					break;
				case ID_CONVERT:
					Converter(hwnd);
					break;
				case ID_SELECTALL:
					count=ListView_GetItemCount(playList);
					for (index=0;index<count;index++)
						ListView_SetItemState(playList,index,LVIS_SELECTED,LVIS_SELECTED);
					break;
				case ID_INVERTSEL:
					count=ListView_GetItemCount(playList);
					for (index=0;index<count;index++)
					{
						if (ListView_GetItemState(playList,index,LVIS_SELECTED) & LVIS_SELECTED)
						{
							ListView_SetItemState(playList,index,0,LVIS_SELECTED);
						}
						else
							ListView_SetItemState(playList,index,LVIS_SELECTED,LVIS_SELECTED);
					}
					EnsureSelection(FALSE);
					break;
				case ID_SORT:
					SendMessage(playlistToolbar,TB_GETITEMRECT,29,(LPARAM)&rect);
					point.x=rect.left;
					point.y=rect.top;
					ClientToScreen(playlistToolbar,&point);
					TrackPopupMenu(
									GetSubMenu(GetSubMenu(hMenu,1),8),
									TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
									point.x,
									point.y,
									0,
									hwnd,
									NULL
								  );
					break;
				case ID_SORT_REVERSE:
				case ID_SORT_RANDOMIZE:
					count=ListView_GetItemCount(playList);
					if (count<=0)
						break;
					srand((UINT)GetTickCount());
					waitwnd=ShowWaitWindow(hwnd,"Sorting list","Walking the playlist items. Please wait...");
					for (index=0;index<count;index++)
					{
						node=GetListViewNode(index);
						if (node!=NULL)
							node->dwData=(DWORD)((LOWORD(wparm)==ID_SORT_RANDOMIZE)?(rand()):index);
					}
					CloseWaitWindow(waitwnd);
				case ID_SORT_AFTITLE:
				case ID_SORT_AFTIME:
				case ID_SORT_AFTYPE:
				case ID_SORT_RFTITLE:
				case ID_SORT_RFNAME:
				case ID_SORT_RFTYPE:
				case ID_SORT_START:
				case ID_SORT_LENGTH:
				case ID_SORT_AFDATA:
				case ID_SORT_RFDATA:
					count=ListView_GetItemCount(playList);
					if (count<=0)
						break;
					waitwnd=ShowWaitWindow(hwnd,"Sorting list","Sorting the playlist. Please wait...");
					ListView_SortItems(playList,(PFNLVCOMPARE)CompareFunc,(LPARAM)LOWORD(wparm));
					BuildNodeChain(playList);
					ListView_EnsureVisible(playList,GetCurrentSelection(),FALSE);
					CloseWaitWindow(waitwnd);
					break;
				case ID_LIST:
					if (inEdit)
						break;
					SetFocus(playDialog);
					DestroyWindow(hwnd);
					UpdateWindow(playDialog);
					playlistWnd=NULL;
					cursel=NULL;
					break;
				case ID_INFO:
				case ID_OPTIONS:
				case ID_EQUALIZER:
				case ID_TIME_TOGGLE:
				case ID_TIME_ELAPSED:
				case ID_TIME_REMAINING:
				case ID_ONTOP:
				case ID_EASYMOVE:
				case ID_PREV:
				case ID_NEXT:
				case ID_STOP:
				case ID_PLAYNEXT:
				case ID_SHUFFLE:
				case ID_INTROSCAN:
				case ID_REPEAT:
				case ID_REPEAT_ONE:
				case ID_REPEAT_ALL:
				case IDQUIT:
					PostMessage(playDialog,WM_COMMAND,LOWORD(wparm),0L);
					break;
				default:
					return DefWindowProc(hwnd,umsg,wparm,lparm);
			}
			break;
		default:
			return DefWindowProc(hwnd,umsg,wparm,lparm);
    }
    return FALSE;
}
