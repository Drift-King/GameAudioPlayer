/*
 * Directory Tree Selector Library source code: general definitions and
 *   global variables
 *
 * Copyright (C) 2000 Alexander Belyakov
 * E-mail: abel@krasu.ru
 *
 * WildMatch function is taken from the
 * "InstallShield Compression and Maintenance util" by fOSSiL.
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

#include "General.h"
#include "TreeSel.h"

BOOL deSelect = FALSE;
CHAR Mask[200];
BOOL Recursively = TRUE;
		// true means select anywhere
		// false - in current dir only
BOOL FullName = TRUE;	// test only name (FALSE) or catenated path and name (TRUE)
HINSTANCE hInst;
int sortType = 1; // type of sorting whithin ListView
	// 0 is Sort By Name, 1 - by Extension


void __stdcall CenterInParent (HWND child) {
	HWND parent = GetParent (child);
	if (!parent)
		parent = GetDesktopWindow();
	RECT pRect, cRect, dRect;
	int x, y;
	GetWindowRect (parent, &pRect);
	GetWindowRect (child, &cRect);
	SystemParametersInfo (SPI_GETWORKAREA, 0, &dRect, 0);
	int cx = cRect. right - cRect. left,
		cy = cRect. bottom - cRect. top;

	x = ((pRect. right + pRect. left) - cx) / 2;
	y = ((pRect. bottom + pRect. top) - cy) / 2;

	if (x < dRect. left)
		x = dRect. left;
	else if (x+cx > dRect. right)
		x = dRect. right - cx;
	if (y < dRect. top)
		y = dRect. top;
	else if (y+cy > dRect. bottom)
		y = dRect. bottom - cy;

	SetWindowPos (child, NULL, x,y, 0,0, SWP_NOSIZE | SWP_NOZORDER);
}

BOOL CALLBACK WildMatch(LPSTR str, LPSTR wildcard)
{
	LPSTR tstr;

	if (!wildcard)
		return TRUE;
	if (!str)
		return FALSE;

	while (*wildcard != '\0' && (toupper(*str) == toupper(*wildcard) || *wildcard == '*' || *wildcard == '?')) {
		if (*wildcard == '?') {
			wildcard++;
			if (*str == '\0')
				return FALSE;
			str++;
		} else if (*wildcard == '*') {
			wildcard++;
			if (*wildcard == '\0')
				return TRUE;
			tstr = str;
			while (*tstr != '\0' && !WildMatch(tstr, wildcard))
				tstr++;
			if (*tstr != '\0')
				str = tstr;
		} else {
			wildcard++;
			str++;
		}
	}
	return toupper(*str) == toupper(*wildcard);
}

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_ATTACH)
		hInst = hinstDLL;
	return TRUE;
}
