/*
 * Game Audio Player source code: progress dialog boxes
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

#include "..\Plugins\ResourceFile\RFPlugin.h"

#include "Globals.h"
#include "Misc.h"
#include "Messages.h"
#include "Progress.h"
#include "Plugins.h"
#include "PlayDialog.h"
#include "resource.h"

#define PROGRESS_MAX (100)

BOOL	  cancelFlag=FALSE,
		  cancelAllFlag=FALSE;
HWND	  progressDialog=NULL;

BOOL IsCancelled(void)
{
	if (progressDialog==NULL)
		return FALSE;

    ProcessMessages(progressDialog,TRUE);

    return cancelFlag;
}

BOOL IsAllCancelled(void)
{
	if (progressDialog==NULL)
		return FALSE;
	
	ProcessMessages(progressDialog,TRUE);

    return cancelAllFlag;
}

void InitPercentageGauge(int id_progress, int id_percent)
{
	SendDlgItemMessage(progressDialog,id_progress,PBM_SETRANGE,0,MAKELPARAM(0,PROGRESS_MAX));
    SendDlgItemMessage(progressDialog,id_progress,PBM_SETPOS,0,0L);
	SetWindowLong(GetDlgItem(progressDialog,id_percent),GWL_USERDATA,0L);
	SetDlgItemText(progressDialog,id_percent,"0%  ");
}

void UpdatePercentageGauge(int id, WORD val)
{
	LONG curval;
	char str[10];

	curval=GetWindowLong(GetDlgItem(progressDialog,id),GWL_USERDATA);
	if ((LONG)val!=curval)
	{
		wsprintf(str,"%u%% ",val);
		SetDlgItemText(progressDialog,id,str);
	}
}

void ShowMasterProgress(DWORD cur, DWORD full)
{
	if (progressDialog==NULL)
		return;

    SendDlgItemMessage(progressDialog,ID_MASTERPROGRESS,PBM_SETPOS,MulDiv(PROGRESS_MAX,cur,full),0L);
	UpdatePercentageGauge(ID_MASTERPERCENT,(WORD)MulDiv(100,cur,full));
}

void ShowProgress(DWORD cur, DWORD full)
{
	if (progressDialog==NULL)
		return;

    SendDlgItemMessage(progressDialog,ID_PROGRESS,PBM_SETPOS,MulDiv(PROGRESS_MAX,cur,full),0L);
	UpdatePercentageGauge(ID_PERCENT,(WORD)MulDiv(100,cur,full));
}

void ShowFileProgress(RFHandle* f)
{
    ShowProgress(RFPLUGIN(f)->GetFilePointer(f),RFPLUGIN(f)->GetFileSize(f));
}

void ShowMasterProgressHeaderMsg(const char* msg)
{
	if (progressDialog==NULL)
		return;

    SetDlgItemText(progressDialog,ID_MASTERHEADER,msg);
    UpdateWindow(progressDialog);
}

void ShowProgressHeaderMsg(const char* msg)
{
	if (progressDialog==NULL)
		return;

    SetDlgItemText(progressDialog,ID_HEADER,msg);
    UpdateWindow(progressDialog);
}

void ShowProgressStateMsg(const char* msg)
{
	if (progressDialog==NULL)
		return;

    SetDlgItemText(progressDialog,ID_STATE,msg);
    UpdateWindow(progressDialog);
}

LRESULT CALLBACK ProgressDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RECT         rect;
	POINT        point;

	static BOOL  bIsDragging=FALSE;
	static POINT dragPoint;
	static HWND  oldCapture;

    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			ClearMessageQueue(hwnd,50); // ???
			return TRUE;
		case WM_NCLBUTTONDOWN:
			if (
				((INT)wParam==HTCLOSE) ||
				((INT)wParam==HTERROR) ||
				((INT)wParam==HTSIZE) ||
				((INT)wParam==HTHELP) ||
				((INT)wParam==HTMENU) ||
				((INT)wParam==HTMAXBUTTON) ||
				((INT)wParam==HTMINBUTTON) ||
				((INT)wParam==HTREDUCE) ||
				((INT)wParam==HTZOOM) ||
				((INT)wParam==HTSYSMENU)
			   )
				break;
		case WM_LBUTTONDOWN:
			if (opEasyMove)
			{
				GetWindowRect(hwnd,&rect);
				dragPoint.x=(LONG)LOWORD(lParam);
				dragPoint.y=(LONG)HIWORD(lParam);
				if (uMsg==WM_LBUTTONDOWN)
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
				point.x=(LONG)LOWORD(lParam);
				point.y=(LONG)HIWORD(lParam);
				ClientToScreen(hwnd,&point);
				point.x-=dragPoint.x;
				point.y-=dragPoint.y;
				SetWindowPos(hwnd,NULL,point.x,point.y,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
				UpdateWindow(playDialog);
			}
			break;
		case WM_GAP_ISCANCELLED:
			SetWindowLong(hwnd,DWL_MSGRESULT,(LONG)IsCancelled());
			return TRUE;
		case WM_GAP_ISALLCANCELLED:
			SetWindowLong(hwnd,DWL_MSGRESULT,(LONG)IsAllCancelled());
			return TRUE;
		case WM_GAP_SHOWMASTERPROGRESS:
			ShowMasterProgress((DWORD)wParam,(DWORD)lParam);
			break;
		case WM_GAP_SHOWPROGRESS:
			ShowProgress((DWORD)wParam,(DWORD)lParam);
			break;
		case WM_GAP_SHOWFILEPROGRESS:
			ShowFileProgress((RFHandle*)lParam);
			break;
		case WM_GAP_SHOWMASTERPROGRESSHEADER:
			ShowMasterProgressHeaderMsg((const char*)lParam);
			break;
		case WM_GAP_SHOWPROGRESSHEADER:
			ShowProgressHeaderMsg((const char*)lParam);
			break;
		case WM_GAP_SHOWPROGRESSSTATE:
			ShowProgressStateMsg((const char*)lParam);
			break;
		case WM_CLOSE:
			PostMessage(hwnd,WM_COMMAND,(WPARAM)ID_CANCELALL,0L);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
					cancelFlag=TRUE;
					break;
				case ID_CANCELALL:
					cancelFlag=TRUE;
					cancelAllFlag=TRUE;
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

HWND OpenProgress(HWND hwnd, DWORD type, const char* title)
{
    if (progressDialog!=NULL)
		return progressDialog;

	cancelFlag=FALSE;
	cancelAllFlag=FALSE;
	switch (type)
	{
		case PDT_DOUBLE:
			progressDialog=CreateDialog(hInst,"DoubleProgressDialog",hwnd,(DLGPROC)ProgressDlgProc);
			break;
		case PDT_SINGLE2:
			progressDialog=CreateDialog(hInst,"ProgressDialog2",hwnd,(DLGPROC)ProgressDlgProc);
			break;
		case PDT_SINGLE:
			progressDialog=CreateDialog(hInst,"ProgressDialog",hwnd,(DLGPROC)ProgressDlgProc);
			break;
		case PDT_SIMPLE:
			progressDialog=CreateDialog(hInst,"SimpleProgress",hwnd,(DLGPROC)ProgressDlgProc);
			break;
		default:
			return NULL;
	}
    SetWindowText(progressDialog,title);
    UpdateWholeWindow(hwnd);
	InitPercentageGauge(ID_PROGRESS,ID_PERCENT);
	if (type==PDT_DOUBLE)
		InitPercentageGauge(ID_MASTERPROGRESS,ID_MASTERPERCENT);

    return progressDialog;
}

void CloseProgress(void)
{
	if (progressDialog==NULL)
		return;

    SetFocus(GetParent(progressDialog));
    DestroyWindow(progressDialog);
	UpdateWindow(GetParent(progressDialog));
    progressDialog=NULL;
    cancelFlag=FALSE;
	cancelAllFlag=FALSE;
}

void ResetCancelFlag(void)
{
	cancelFlag=FALSE;
}

void SetCancelFlag(void)
{
	cancelFlag=TRUE;
}

void ResetCancelAllFlag(void)
{
	cancelAllFlag=FALSE;
}

void SetCancelAllFlag(void)
{
	cancelAllFlag=TRUE;
}
