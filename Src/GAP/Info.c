/*
 * Game Audio Player source code: info dialog box
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

#include <string.h>

#include <windows.h>
#include <wingdi.h>
#include <shellapi.h>
#include <prsht.h>

#include "Globals.h"
#include "Info.h"
#include "Misc.h"
#include "Errors.h"
#include "resource.h"

int infoPropPage=0;

void TrackInfoPropPage(HWND hwnd, int index)
{
    if (PropSheet_GetCurrentPageHwnd(GetParent(hwnd))==hwnd)
		infoPropPage=index;
}

void ShellExec(HWND hwnd, const char* command, const char* failexe, const char* errmsg)
{
	char str[MAX_PATH+20];

	if ((DWORD)ShellExecute(hwnd,"open",command,NULL,NULL,SW_SHOWNORMAL)<=32)
	{
		lstrcpy(str,failexe);
		lstrcat(str," ");
		lstrcat(str,command);
		if (WinExec(str,SW_SHOW)<=31)
			ReportError(hwnd,errmsg,NULL);
	}
}

HFONT   hFont;
HCURSOR hCursor;

LRESULT CALLBACK LinkButtonProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    switch (umsg)
    {
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONUP:
			SetCursor(hCursor);
			break;
		default:
			break;
    }

    return CallWindowProc((WNDPROC)GetWindowLong(hwnd,GWL_USERDATA),hwnd,umsg,wparm,lparm);
}

void InitLink(HWND hwnd, int id)
{
	LONG oldproc;

	oldproc=SetWindowLong(GetDlgItem(hwnd,id),GWL_WNDPROC,(LONG)LinkButtonProc);
	SetWindowLong(GetDlgItem(hwnd,id),GWL_USERDATA,oldproc);
	SendDlgItemMessage(hwnd,id,WM_SETFONT,(WPARAM)hFont,MAKELPARAM(TRUE,0));
}

LRESULT CALLBACK InfoMainProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	LPNMHDR lpnmhdr;
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;
	char	str[MAX_PATH],
			ctrltext[256];
	LPDRAWITEMSTRUCT lpdis;

    switch (umsg)
    {
		case WM_INITDIALOG:
			hCursor=LoadCursor(hInst,(LPCTSTR)IDC_LINK);
			hFont=CreateFont(8,0,0,0,FW_NORMAL,FALSE,TRUE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"MS Sans Serif");
			InitLink(hwnd,ID_ANXURL);
			InitLink(hwnd,ID_ANXMAIL);
			InitLink(hwnd,ID_SAMAELMAIL);
			InitLink(hwnd,ID_BIMURL);
			InitLink(hwnd,ID_BIMMAIL);
			InitLink(hwnd,ID_SHEPURL);
			InitLink(hwnd,ID_SHEPMAIL);
			InitLink(hwnd,ID_DENVERURL);
			InitLink(hwnd,ID_DENVERMAIL);
			return TRUE;
		case WM_DRAWITEM:
			lpdis=(LPDRAWITEMSTRUCT)lparm;
			if (lpdis->CtlType==ODT_BUTTON)
			{
				GetDlgItemText(hwnd,lpdis->CtlID,ctrltext,sizeof(ctrltext));
				if ((lpdis->itemState) & ODS_SELECTED)
					SetTextColor(lpdis->hDC,RGB(0xFF,0,0));
				else
					SetTextColor(lpdis->hDC,RGB(0,0,0xFF));
				TextOut(lpdis->hDC,0,0,ctrltext,lstrlen(ctrltext));
			}
			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
				case PSN_RESET:
					DeleteObject(hFont);
					TrackInfoPropPage(hwnd,0);
					break;
				default:
					break;
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case ID_ANXURL:
				case ID_BIMURL:
				case ID_SHEPURL:
				case ID_DENVERURL:
					GetDlgItemText(hwnd,LOWORD(wparm),ctrltext,sizeof(ctrltext));
					if (_strnicmp(ctrltext,"http://",lstrlen("http://")))
						wsprintf(str,"http://%s",ctrltext);
					else
						lstrcpy(str,ctrltext);
					ShellExec(hwnd,str,"iexplore.exe","Failed to launch Internet Explorer.");
					break;
				case ID_ANXMAIL:
				case ID_SAMAELMAIL:
				case ID_BIMMAIL:
				case ID_SHEPMAIL:
				case ID_DENVERMAIL:
					GetDlgItemText(hwnd,LOWORD(wparm),ctrltext,sizeof(ctrltext));
					if (_strnicmp(ctrltext,"mailto:",lstrlen("mailto:")))
						wsprintf(str,"mailto:%s",ctrltext);
					else
						lstrcpy(str,ctrltext);
					ShellExec(hwnd,str,"iexplore.exe","Failed to launch Internet Explorer.");
					break;
				case ID_README:
					ReplaceAppFileExtension(str,sizeof(str),".txt");
					ShellExec(hwnd,str,"notepad.exe","Failed to display documentation.");
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

LRESULT CALLBACK InfoCreditsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	LPNMHDR lpnmhdr;
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;

    switch (umsg)
    {
		case WM_INITDIALOG:
			hres=FindResource(hInst,(LPCTSTR)IDR_CREDITS,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(hInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_TEXT,(res!=NULL)?res:"No info available.");
			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
				case PSN_RESET:
					TrackInfoPropPage(hwnd,1);
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

LRESULT CALLBACK InfoHelpProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	LPNMHDR lpnmhdr;
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;

    switch (umsg)
    {
		case WM_INITDIALOG:
			hres=FindResource(hInst,(LPCTSTR)IDR_HELP,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(hInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_TEXT,(res!=NULL)?res:"No info available.");
			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
				case PSN_RESET:
					TrackInfoPropPage(hwnd,2);
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

LRESULT CALLBACK InfoTipsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	LPNMHDR lpnmhdr;
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;
	RECT	rect;
	int		pt[1];

    switch (umsg)
    {
		case WM_INITDIALOG:
			GetClientRect(GetDlgItem(hwnd,ID_TEXT),&rect);
			pt[0]=(rect.right-rect.left)/4;
			SendDlgItemMessage(hwnd,ID_TEXT,EM_SETTABSTOPS,sizeof(pt)/sizeof(pt[0]),(LPARAM)pt);
			hres=FindResource(hInst,(LPCTSTR)IDR_TIPS,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(hInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_TEXT,(res!=NULL)?res:"No info available.");
			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
				case PSN_RESET:
					TrackInfoPropPage(hwnd,3);
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

LRESULT CALLBACK InfoOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	LPNMHDR lpnmhdr;
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;

    switch (umsg)
    {
		case WM_INITDIALOG:
			hres=FindResource(hInst,(LPCTSTR)IDR_OPTIONS,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(hInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_TEXT,(res!=NULL)?res:"No info available.");
			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
				case PSN_RESET:
					TrackInfoPropPage(hwnd,4);
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

LRESULT CALLBACK InfoProjectProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
	LPNMHDR lpnmhdr;
	HRSRC   hres=NULL;
	HGLOBAL hresdata=NULL;
	LPVOID  res=NULL;

    switch (umsg)
    {
		case WM_INITDIALOG:
			hres=FindResource(hInst,(LPCTSTR)IDR_PROJECT,"TEXT");
			if (hres!=NULL)
				hresdata=LoadResource(hInst,hres);
			if (hresdata!=NULL)
				res=LockResource(hresdata);
			SetDlgItemText(hwnd,ID_TEXT,(res!=NULL)?res:"No info available.");
			return TRUE;
		case WM_NOTIFY:
			lpnmhdr=(LPNMHDR)lparm;
			switch (lpnmhdr->code)
			{
				case PSN_APPLY:
				case PSN_RESET:
					TrackInfoPropPage(hwnd,5);
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

int CreateInfoDlg(HWND hwnd)
{
    PROPSHEETPAGE   psp[6];
    PROPSHEETHEADER psh;

    psp[0].dwSize = sizeof(PROPSHEETPAGE);
    psp[0].dwFlags = PSP_USETITLE;
    psp[0].hInstance = hInst;
    psp[0].pszTemplate = "InfoMainPage";
    psp[0].pfnDlgProc = InfoMainProc;
    psp[0].pszTitle = "GAP";
    psp[0].lParam = 0;

	psp[1].dwSize = sizeof(PROPSHEETPAGE);
    psp[1].dwFlags = PSP_USETITLE;
    psp[1].hInstance = hInst;
    psp[1].pszTemplate = "InfoCreditsPage";
    psp[1].pfnDlgProc = InfoCreditsProc;
    psp[1].pszTitle = "Credits";
    psp[1].lParam = 0;

	psp[2].dwSize = sizeof(PROPSHEETPAGE);
    psp[2].dwFlags = PSP_USETITLE;
    psp[2].hInstance = hInst;
    psp[2].pszTemplate = "InfoHelpPage";
    psp[2].pfnDlgProc = InfoHelpProc;
    psp[2].pszTitle = "Help";
    psp[2].lParam = 0;

	psp[3].dwSize = sizeof(PROPSHEETPAGE);
    psp[3].dwFlags = PSP_USETITLE;
    psp[3].hInstance = hInst;
    psp[3].pszTemplate = "InfoTipsPage";
    psp[3].pfnDlgProc = InfoTipsProc;
    psp[3].pszTitle = "Tips";
    psp[3].lParam = 0;

	psp[4].dwSize = sizeof(PROPSHEETPAGE);
    psp[4].dwFlags = PSP_USETITLE;
    psp[4].hInstance = hInst;
    psp[4].pszTemplate = "InfoOptionsPage";
    psp[4].pfnDlgProc = InfoOptionsProc;
    psp[4].pszTitle = "Options";
    psp[4].lParam = 0;

	psp[5].dwSize = sizeof(PROPSHEETPAGE);
    psp[5].dwFlags = PSP_USETITLE;
    psp[5].hInstance = hInst;
    psp[5].pszTemplate = "InfoProjectPage";
    psp[5].pfnDlgProc = InfoProjectProc;
    psp[5].pszTitle = "Project";
    psp[5].lParam = 0;

    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
    psh.hwndParent = hwnd;
    psh.hInstance = hInst;
    psh.pszCaption = (LPSTR)"ANX Game Audio Player Info";
    psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
    psh.nStartPage=infoPropPage;
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;

    return PropertySheet(&psh);
}
