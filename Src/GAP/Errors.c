/*
 * Game Audio Player source code: error message boxes
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
#include <mmsystem.h>

#include "Globals.h"
#include "Errors.h"

char *lastErrorString;

void ReportMMError(HWND hwnd, MMRESULT mmr, const char *errstr)
{
    char str[MAXERRORLENGTH],*msgbuf;

    waveOutGetErrorText(mmr,str,sizeof(str));
    msgbuf=(char*)LocalAlloc(LPTR,lstrlen(str)+lstrlen(errstr)+2);
	wsprintf(msgbuf,"%s%s%s",errstr,(errstr!=NULL)?"\n":"",str);
    MessageBox(GetFocus(),msgbuf,szAppName,MB_OK | MB_ICONERROR);
    LocalFree(msgbuf);
}

void ReportError(HWND hwnd, const char* errstr, HMODULE hmod)
{
	DWORD  errcode,mask;
    char  *msgbuf,lpErrMsg[500];

	mask=1<<29;
	errcode=GetLastError();
	lstrcpy(lpErrMsg,"");
	if (errcode & mask)
	{
		errcode^=mask;
		if (hmod==NULL)
			hmod=GetModuleHandle(NULL);
		LoadString(hmod,errcode,lpErrMsg,500);
	}
	else
	    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,errcode,0,(LPTSTR)lpErrMsg,500,NULL);
    msgbuf=(char*)LocalAlloc(LPTR,lstrlen(lpErrMsg)+lstrlen(errstr)+2);
	wsprintf(msgbuf,"%s%s%s",errstr,(errstr!=NULL)?"\n":"",(lstrcmpi(lpErrMsg,""))?((LPCSTR)lpErrMsg):"No error message available.");
    MessageBox(GetFocus(),msgbuf,szAppName,MB_OK | MB_ICONERROR);
    LocalFree(msgbuf);
}

