/*
 * Game Audio Player source code: main (player) dialog box
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
#include <wingdi.h>
#include <commctrl.h>
#include <shellapi.h>

#include "Globals.h"
#include "Messages.h"
#include "FSHandler.h"
#include "Misc.h"
#include "Player.h"
#include "Info.h"
#include "Options.h"
#include "PlayDialog.h"
#include "Playlist.h"
#include "Playlist_SaveLoad.h"
#include "Playlist_NodeOps.h"
#include "ListView.h"
#include "Plugins.h"
#include "Progress.h"
#include "Adding.h"
#include "Scanning.h"
#include "resource.h"

#define TRACKBAR_MAX (700)

char szIdleName[]="GAP v1.32";

BOOL isTrackBarSliderLocked;
HWND playDialog=NULL;
HWND playerToolbar=NULL;
BOOL opAutoLoad,
	 opAutoPlay,
	 opPlayerOnTop,
	 opEasyMove,
	 opTimeElapsed;
int  opDropSupport;

char* tooltipString[]=
{
    "Play",
    "Pause",
    "Stop",
    "Previous",
    "Next",
    "Playlist",
    "Equalizer",
    "Options",
    "Info"
};

TBBUTTON tbPlayerButtons[]=
{
    {0,ID_PLAY,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_PLAY,     0},
    {2,ID_STOP,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_STOP,     0},
    {3,ID_PREV,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_PREVIOUS, 0},
    {4,ID_NEXT,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_NEXT,     0},
    {0,0,           TBSTATE_ENABLED,TBSTYLE_SEP,   0,            0},
    {5,ID_LIST,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_PLAYLIST, 0},
    {6,ID_EQUALIZER,TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_EQUALIZER,0},
    {7,ID_OPTIONS,  TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_OPTIONS,  0},
    {8,ID_INFO,     TBSTATE_ENABLED,TBSTYLE_BUTTON,IDS_INFO,     0},
};

DWORD CorrectTrackBarPos(HWND hwnd, LONG xpos)
{
	LONG pos,corrpos,thumblength,length;
	RECT rect;

	SendMessage(hwnd,TBM_GETCHANNELRECT,(WPARAM)0,(LPARAM)&rect);
	pos=xpos-rect.left;
	length=rect.right-rect.left;
	SendMessage(hwnd,TBM_GETTHUMBRECT,(WPARAM)0,(LPARAM)&rect);
	thumblength=rect.right-rect.left;
	corrpos=pos+MulDiv(thumblength,(pos-length/2),length);
	if (corrpos<0)
		corrpos=0;
	if (corrpos>length)
		corrpos=length;

	return MulDiv(TRACKBAR_MAX,corrpos,length);;
}

LRESULT CALLBACK TrackBarProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	POINT point;
    switch (umsg)
    {
		case WM_LBUTTONDOWN:
			isTrackBarSliderLocked=TRUE;
			point.x=LOWORD(lparm);
			point.y=HIWORD(lparm);
			SendDlgItemMessage(playDialog,ID_CURPOS,TBM_SETPOS,(WPARAM)TRUE,(LPARAM)CorrectTrackBarPos(hwnd,point.x));
			break;
		case WM_LBUTTONUP:
			if (isPlaying)
				Seek();
			isTrackBarSliderLocked=FALSE;
			break;
		default:
			break;
    }

    return CallWindowProc((WNDPROC)GetWindowLong(hwnd,GWL_USERDATA),hwnd,umsg,wparm,lparm);
}

void UpdateTrackBar(HWND hwnd)
{
	if (GetFocus()==hwnd)
		SetFocus(SetFocus(NULL));
}

int Quit(HWND hwnd)
{
	int retval;

	switch (retval=AskSavePlaylist(hwnd))
	{
		case IDYES:
			Save(hwnd);
		case IDNO:
		case IDIGNORE:
			Stop();
			SaveLastPlaylist(hwnd);
			PostQuitMessage(0);
		case IDCANCEL:
			break;
		default: // ???
			break;
	}

	return retval;
}

#define MF_Checked(b) ((b)?MF_CHECKED:MF_UNCHECKED)
#define MF_Enabled(b) ((b)?MF_ENABLED:MF_DISABLED)

#define CheckMenuItemBool(menu,id,bCheck) (CheckMenuItem((menu),(id),MF_BYCOMMAND | MF_Checked(bCheck)))
#define EnableMenuItemBool(menu,id,bEnable) (EnableMenuItem((menu),(id),MF_BYCOMMAND | MF_Enabled(bEnable)))

void InitMainMenu(HWND hwnd, HMENU menu)
{
	CheckMenuRadioItem(menu,ID_REPEAT,ID_REPEAT_ALL,opRepeatType,MF_BYCOMMAND);
	CheckMenuRadioItem(menu,ID_TIME_ELAPSED,ID_TIME_REMAINING,(opTimeElapsed)?ID_TIME_ELAPSED:ID_TIME_REMAINING,MF_BYCOMMAND);
	CheckMenuItemBool(menu,ID_ONTOP,opPlayerOnTop);
	CheckMenuItemBool(menu,ID_EASYMOVE,opEasyMove);
	CheckMenuItemBool(menu,ID_LIST,(playlistWnd!=NULL));
	CheckMenuItemBool(menu,ID_EQUALIZER,FALSE); // !!!
	CheckMenuItemBool(menu,ID_PLAYNEXT,(!opPlayNext));
	CheckMenuItemBool(menu,ID_SHUFFLE,opShuffle);
	CheckMenuItemBool(menu,ID_INTROSCAN,opIntroScan);
}

void TrackMainMenu(HWND hwnd)
{
	HMENU hSubMenu;
	POINT point;

	hSubMenu=GetSubMenu(hMenu,0);
	InitMainMenu(hwnd,hSubMenu);
	GetCursorPos(&point);
	TrackPopupMenu(hSubMenu,TPM_LEFTBUTTON | TPM_RIGHTBUTTON,point.x,point.y,0,hwnd,NULL);
}

DWORD AcceptFiles
(
	HWND hwnd, 
	HDROP hdrop, 
	const char* title, 
	const char* header, 
	DWORD (*Action)(HWND,const char*)
)
{
	char  filename[MAX_PATH],str[256];
	UINT  i,count;
	DWORD retval=0;
	HWND  pwnd;

	if (hdrop==INVALID_HANDLE_VALUE)
		return retval;

	count=DragQueryFile(hdrop,0xFFFFFFFF,filename,sizeof(filename));
	if (count==0)
		return retval;
	if (count==1)
	{
		DragQueryFile(hdrop,0,filename,sizeof(filename));
		wsprintf(str,"%s: %s",title,GetFileTitleEx(filename));
		pwnd=OpenProgress(hwnd,PDT_SINGLE,str);
		retval+=Action(pwnd,filename);
	}
	else
	{
		pwnd=OpenProgress(hwnd,PDT_DOUBLE,title);
		ShowMasterProgress(0,count);
		ShowMasterProgressHeaderMsg(header);
		for (i=0;i<count;i++)
		{
			if (IsAllCancelled())
				break;
			DragQueryFile(hdrop,i,filename,sizeof(filename));
			ResetCancelFlag();
			retval+=Action(pwnd,filename);
			ShowMasterProgress(i,count);
		}
	}
	CloseProgress();

	return retval;
}

void OnDropFiles(HWND hwnd, HDROP hdrop)
{
	DWORD num;
	HMENU hSubMenu;
	POINT point;
	UINT  choice;
	char  str[256];

	if (opDropSupport==ID_DROP_ASK)
	{
		DragQueryPoint(hdrop,&point);
		ClientToScreen(hwnd,&point);
		hSubMenu=GetSubMenu(hMenu,3);
		choice=(UINT)TrackPopupMenu(hSubMenu,TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,point.x,point.y,0,hwnd,NULL);
		if (choice==0)
		{
			DragFinish(hdrop);
			return;
		}
	}
	else
		choice=opDropSupport;
	switch (choice)
	{
		case ID_DROP_OFF:
			break;
		default:
		case ID_DROP_ADD:
			AcceptFiles(hwnd,hdrop,"File verification","Verified files",AddFile);
			break;
		case ID_DROP_SCAN:
			num=AcceptFiles(hwnd,hdrop,"File search","Scanned files",Scan);
			if (num>0)
				wsprintf(str,"Found %lu files.",num);
			else
				wsprintf(str,"Found no files.");
			MessageBox(GetFocus(),str,szAppName,MB_OK | MB_ICONINFORMATION);
			break;
		case ID_DROP_LOAD:
			AcceptFiles(hwnd,hdrop,"Playlist loading","Loaded playlists",LoadPlaylist);
			break;
	}
	DragFinish(hdrop);
}


void InitPlayerToolbarTooltips(void)
{
	
	tbPlayerButtons[0].dwData=IDS_PLAY;
	tbPlayerButtons[1].dwData=IDS_STOP;
	tbPlayerButtons[2].dwData=IDS_PREVIOUS;
	tbPlayerButtons[3].dwData=IDS_NEXT;
	tbPlayerButtons[4].dwData=0;
	tbPlayerButtons[5].dwData=IDS_PLAYLIST;
	tbPlayerButtons[6].dwData=IDS_EQUALIZER;
	tbPlayerButtons[7].dwData=IDS_OPTIONS;
	tbPlayerButtons[8].dwData=IDS_INFO;
}

void PlayDlgToolbarPlayToggle(BOOL playing)
{
	int index;

	if (playing)
	{
		SendMessage(playerToolbar,TB_CHANGEBITMAP,(WPARAM)ID_PLAY,(LPARAM)MAKELPARAM(1,0));
		index=SendMessage(playerToolbar,TB_COMMANDTOINDEX,(WPARAM)ID_PLAY,0L);
		tbPlayerButtons[index].dwData=IDS_PAUSE;
	}
	else
	{
		SendMessage(playerToolbar,TB_CHANGEBITMAP,(WPARAM)ID_PLAY,(LPARAM)MAKELPARAM(0,0));
		index=SendMessage(playerToolbar,TB_COMMANDTOINDEX,(WPARAM)ID_PLAY,0L);
		tbPlayerButtons[index].dwData=IDS_PLAY;
	}
}

LRESULT CALLBACK PlayDialog(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    int 		  index,posx,posy;
    HICON		  hicon;
    LPNMHDR 	  hdr;
	POINT         point;
	RECT          rect;
	HMENU		  hSysMenu,hSubMenu;
	LONG          oldproc;

	static BOOL   bIsDragging=FALSE;
	static POINT  dragPoint;
	static HWND   oldCapture;

    switch (umsg)
    {
		case WM_INITDIALOG:
			playDialog=hwnd;
			opPlayerOnTop=LoadWndPos("PlayerWindow",&posx,&posy);
			if ((posx!=DEF_POS) && (posy!=DEF_POS))
				SetWindowPos(hwnd,NULL,posx,posy,0,0,SWP_NOSIZE | SWP_NOZORDER);
			else
				CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetWindowPos(hwnd,(opPlayerOnTop)?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
			hicon=LoadIcon(hInst,"PlayerIcon");
			SendMessage(hwnd,WM_SETICON,(WPARAM)ICON_BIG,(LPARAM)hicon);
			SendMessage(hwnd,WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)hicon);
			InitPlayerToolbarTooltips();
			playerToolbar=CreateToolbarEx(GetDlgItem(hwnd,ID_TBWND),
									   WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS,
									   ID_TOOLBAR,
									   9,
									   hInst,
									   ID_BUTTONS,
									   tbPlayerButtons,
									   sizeof(tbPlayerButtons)/sizeof(TBBUTTON),
									   0,0,16,16,sizeof(TBBUTTON)
									  );
			SendMessage(playerToolbar,TB_SETPARENT,(WPARAM)hwnd,0);
			SendDlgItemMessage(hwnd,ID_CURPOS,TBM_SETRANGEMIN,(WPARAM)TRUE,(LPARAM)0);
			SendDlgItemMessage(hwnd,ID_CURPOS,TBM_SETRANGEMAX,(WPARAM)TRUE,(LPARAM)TRACKBAR_MAX);
			isTrackBarSliderLocked=FALSE;
			oldproc=SetWindowLong(GetDlgItem(hwnd,ID_CURPOS),GWL_WNDPROC,(LONG)TrackBarProc);
			SetWindowLong(GetDlgItem(hwnd,ID_CURPOS),GWL_USERDATA,oldproc);
			hSubMenu=GetSubMenu(hMenu,0);
			hSysMenu=GetSystemMenu(hwnd,FALSE);
			InsertMenu(hSysMenu,0,MF_BYPOSITION | MF_STRING | MF_POPUP,(UINT)hSubMenu,szAppName);
			InsertMenu(hSysMenu,1,MF_BYPOSITION | MF_SEPARATOR,0,NULL);
			DrawMenuBar(hwnd);
			InitStats();
			ResetStats();
			ShowWindow(hwnd,SW_SHOWNORMAL);
			InvalidateRect(hwnd,NULL,FALSE);
			UpdateWindow(hwnd);
			SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,0,(LPARAM)"Loading options...");
			LoadOptions();
			SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,0,(LPARAM)"Init plug-ins...");
			InitPlugins();
			SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,0,(LPARAM)"Init playlist...");
			InitPlaylist();
			SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,0,(LPARAM)"Init player...");
			InitPlayer();
			ResetStats();
			if (opAutoLoad)
			{
				LoadLastPlaylist(hwnd);
				if (opAutoPlay)
					Play(curNode);
			}
			InvalidateRect(hwnd,NULL,FALSE);
			UpdateWindow(hwnd);
			DragAcceptFiles(hwnd,(BOOL)(opDropSupport!=ID_DROP_OFF));
			return TRUE;
		case WM_DROPFILES:
			SetForegroundWindow(hwnd);
			OnDropFiles(hwnd,(HDROP)wparm);
			break;
		case WM_SETCURSOR:
			if (LOWORD(lparm)==HTCAPTION)
				InitMainMenu(hwnd,GetSystemMenu(hwnd,FALSE));
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
				((INT)wparm==HTZOOM)
			   )
				break;
			if ((INT)wparm==HTSYSMENU)
			{
				InitMainMenu(hwnd,GetSystemMenu(hwnd,FALSE));
				break;
			}
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
				UpdateTrackBar(GetDlgItem(playDialog,ID_CURPOS));
			}
			break;
		case WM_GAP_END_OF_PLAYBACK:
			Stop();
			PlayNext();
			break;
		case WM_GAP_SHOW_PLAYBACK_TIME:
			ShowPlaybackTime();
			break;
		case WM_GAP_SHOW_PLAYBACK_POS:
			ShowPlaybackPos((DWORD)wparm,(DWORD)lparm);
			break;
		case WM_DESTROY:
			SaveWndPos(hwnd,"PlayerWindow",opPlayerOnTop);
			ShutdownPlayer();
			SaveOptions();
			ShutdownPlugins();
			break;
		case WM_CLOSE:
			Quit(hwnd);
			break;
		case WM_NOTIFY:
			hdr=(LPNMHDR)lparm;
			if (hdr->code==TTN_NEEDTEXT)
			{
				index=(int)SendMessage(playerToolbar,TB_COMMANDTOINDEX,(WPARAM)(hdr->idFrom),0L);
				((LPTOOLTIPTEXT)lparm)->lpszText=(LPTSTR)(tbPlayerButtons[index].dwData);
				((LPTOOLTIPTEXT)lparm)->hinst=hInst;
			}
			break;
		case WM_HSCROLL:
			if (GetDlgCtrlID((HWND)lparm)==ID_CURPOS)
			{
				if (curNode!=NULL)
				{
					if (isTrackBarSliderLocked)
						ShowPlaybackTime();
				}
			}
			break;
		case WM_SYSCOMMAND:
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_TIME_ELAPSED:
				case ID_TIME_REMAINING:
					opTimeElapsed=(BOOL)(LOWORD(wparm)==ID_TIME_ELAPSED);
					ShowPlaybackTime();
					break;
				case ID_TIME_TOGGLE:
					opTimeElapsed=!opTimeElapsed;
					ShowPlaybackTime();
					break;
				case ID_ONTOP:
					opPlayerOnTop=!opPlayerOnTop;
					SetWindowPos(hwnd,(opPlayerOnTop)?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
					if (playlistWnd!=NULL)
						SetWindowPos(playlistWnd,(opPlayerOnTop)?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
					break;
				case ID_EASYMOVE:
					opEasyMove=!opEasyMove;
					break;
				case ID_PLAY:
					if (!isPlaying)
						Play(curNode);
					else
						Pause();
					break;
				case ID_PREV:
					Previous();
					break;
				case ID_NEXT:
					Next();
					break;
				case ID_STOP:
					Stop();
					break;
				case ID_OPTIONS:
					if (CreateOptionsDlg(hwnd)==-1)
						MessageBox(GetFocus(),"Failed to create options dialog!",szAppName,MB_OK | MB_ICONEXCLAMATION);
					break;
				case ID_EQUALIZER:
					MessageBox(GetFocus(),"Sorry, equalizer not implemented yet.",szAppName,MB_OK | MB_ICONEXCLAMATION);
					break;
				case ID_INFO:
					if (CreateInfoDlg(hwnd)==-1)
						MessageBox(GetFocus(),"Failed to create info dialog!",szAppName,MB_OK | MB_ICONEXCLAMATION);
					break;
				case ID_LIST:
					ShowPlaylist(hwnd);
					break;
				case ID_FILEINFO:
					if (
						(curNode!=NULL) && 
						(AFPLUGIN(curNode)!=NULL) && 
						(AFPLUGIN(curNode)->InfoBox!=NULL)
					   )
						AFPLUGIN(curNode)->InfoBox(curNode,hwnd);
					break;
				case ID_LOAD:
					Load(hwnd);
					break;
				case ID_ADD:
					Add(hwnd);
					break;
				case ID_FIND:
					Find(hwnd);
					break;
				case IDQUIT:
					Quit(hwnd);
					break;
				case ID_PLAYNEXT:
					opPlayNext=!opPlayNext;
					break;
				case ID_SHUFFLE:
					opShuffle=!opShuffle;
					break;
				case ID_INTROSCAN:
					opIntroScan=!opIntroScan;
					break;
				case ID_REPEAT:
				case ID_REPEAT_ONE:
				case ID_REPEAT_ALL:
					opRepeatType=LOWORD(wparm);
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

void InitStats(void)
{
    HDC     hdc;
    RECT    rect;
    SIZE    size;
    int     pt[4];

    SetDlgItemText(playDialog,ID_CURNAME,"");
    hdc=GetDC(playDialog);
    GetClientRect(playDialog,&rect);
    pt[3]=rect.right;
    if (GetTextExtentPoint32(hdc,"Playing: 0000/00000",19,&size))
		pt[0]=rect.left+(size.cx-25);
    else
		pt[0]=0;
    if (GetTextExtentPoint32(hdc,"00:00:00",8,&size))
		pt[1]=pt[0]+(size.cx);
    else
		pt[1]=0;
    if (GetTextExtentPoint32(hdc,"XXXXXXX Audio File",15,&size))
		pt[2]=pt[1]+(size.cx);
    else
		pt[2]=0;
    ReleaseDC(playDialog,hdc);
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETPARTS,(WPARAM)(sizeof(pt)/sizeof(pt[0])),(LPARAM)(LPINT)pt);;
    ResetStats();
}

void ResetTrackBar(void)
{
    int  i;
	LONG style;

    EnableWindow(GetDlgItem(playDialog,ID_CURPOS),FALSE);
    for (i=0;i<8;i++)
		SetDlgItemText(playDialog,ID_LABEL1+i,"");
	style=GetWindowLong(GetDlgItem(playDialog,ID_CURPOS),GWL_STYLE);
	SetWindowLong(GetDlgItem(playDialog,ID_CURPOS),GWL_STYLE,style | TBS_NOTICKS | TBS_NOTHUMB);
    //SendDlgItemMessage(playDialog,ID_CURPOS,TBM_CLEARTICS,TRUE,0L); // causes severe error
}

void InitTrackBar(DWORD time)
{
    WORD i;
	LONG style;
    char str[20];

    if (time!=-1)
    {
		style=GetWindowLong(GetDlgItem(playDialog,ID_CURPOS),GWL_STYLE);
		SetWindowLong(GetDlgItem(playDialog,ID_CURPOS),GWL_STYLE,style & (~(TBS_NOTICKS | TBS_NOTHUMB)));
		if ((AFPLUGIN(curNode)!=NULL) && (AFPLUGIN(curNode)->Seek!=NULL))
			EnableWindow(GetDlgItem(playDialog,ID_CURPOS),TRUE);
		else
			EnableWindow(GetDlgItem(playDialog,ID_CURPOS),FALSE);
		GetShortTimeString(0,str);
		SetDlgItemText(playDialog,ID_LABEL1,str);
		GetShortTimeString(time/1000,str);
		SetDlgItemText(playDialog,ID_LABEL8,str);
		for (i=1;i<7;i++)
		{
			SendDlgItemMessage(playDialog,ID_CURPOS,TBM_SETTIC,0,MulDiv(i,TRACKBAR_MAX,7));
			GetShortTimeString(MulDiv(i,time,7000),str);
			SetDlgItemText(playDialog,ID_LABEL1+i,str);
		}
    }
    else
		ResetTrackBar();
    UpdateWindow(playDialog);
}

DWORD GetPlaybackPos()
{
    if ((curNode==NULL) || (curNode->afTime==-1))
		return 0;
    else
		return MulDiv(SendDlgItemMessage(playDialog,ID_CURPOS,TBM_GETPOS,0,0L),curNode->afTime,TRACKBAR_MAX);
}

void ResetStats(void)
{
    ResetPlaybackPos();
    ResetTrackBar();
    SetDlgItemText(playDialog,ID_CURNAME,szIdleName);
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,0,(LPARAM)"Idle");
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,2,(LPARAM)"N/A");
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,3,(LPARAM)"N/A");
}

void ShowStats(void)
{
    char str[300],tstr[20];

    if (curNode==NULL)
    {
		ResetStats();
		return;
    }
    if (curNode->afTime==-1)
		lstrcpy(str,curNode->afName);
    else
    {
		UpdatePlaylistNodeTime(curNode);
		GetShortTimeString((curNode->afTime)/1000,tstr);
		wsprintf(str,"%s (%s)",curNode->afName,tstr);
    }
    SetDlgItemText(playDialog,ID_CURNAME,str);
    if (!isPlaying)
		lstrcpy(tstr,"Idle");
    else if (!isPaused)
		lstrcpy(tstr,"Playing");
    else
		lstrcpy(tstr,"Paused");
    wsprintf(str,"%s: %d/%d",tstr,GetNodePos(curNode)+1,list_size);
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,0,(LPARAM)str);
    wsprintf(str,"%s Audio File",curNode->afID);
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,2,(LPARAM)str);
    wsprintf(str,"%s Resource File",curNode->rfID);
    SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,3,(LPARAM)str);
    InitTrackBar(curNode->afTime);
    UpdateWindow(playDialog);
}

#define BLINK_TIME (700)

void ShowPlaybackTime(void)
{
    char str[40];
    static DWORD lastBlink=0;
    DWORD newBlink,time;

	time=GetPlaybackPos();
	if (
		(!opTimeElapsed) && 
		(curNode!=NULL) && 
		(curNode->afTime!=-1)
	   )
		time=(curNode->afTime)-time;
    if (isPaused)
    {
		newBlink=GetTickCount();
		if ((newBlink-lastBlink)<BLINK_TIME)
			return;
		lastBlink=newBlink;
		SendDlgItemMessage(playDialog,ID_STATS,SB_GETTEXT,1,(LPARAM)str);
		if (!lstrcmpi(str,""))
		{
			GetTimeString(time/1000,str);
			SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,1,(LPARAM)str);
		}
		else
			SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,1,(LPARAM)"");
	}
	else
	{
		GetTimeString(time/1000,str);
		SendDlgItemMessage(playDialog,ID_STATS,SB_SETTEXT,1,(LPARAM)str);
    }
}

void ResetPlaybackPos(void)
{
    SendDlgItemMessage(playDialog,ID_CURPOS,TBM_SETPOS,(WPARAM)TRUE,(LPARAM)0);
    ShowPlaybackTime();
}

void ShowPlaybackPos(DWORD pos, DWORD time)
{
    if (time>0)
		SendDlgItemMessage(playDialog,ID_CURPOS,TBM_SETPOS,(WPARAM)TRUE,MulDiv(TRACKBAR_MAX,pos,time));
    else
		SendDlgItemMessage(playDialog,ID_CURPOS,TBM_SETPOS,(WPARAM)TRUE,TRACKBAR_MAX);
    ShowPlaybackTime();
}
