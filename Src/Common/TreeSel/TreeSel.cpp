/*
 * Directory Tree Selector Library source code: interface functions
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

//#include "TreeSel.h"
#include "CDirTree.h"
#include "Dialogs.h"
//#include "TreeSel.h"
#include "Lists.h"
#include "General.h"
#include "resource.h"

HDIRSEL __stdcall CreateDirSelector (RegexpTester match) {
	return new CDirectoryTree (match);
}

long __stdcall GetDirectoryIndex (HDIRSEL dirSel, const char* path) {
	CDirItem* dummy;
	if (dirSel && path)
		return ((CDirectoryTree*)dirSel) -> addDir (path, dummy);
	else
		return -1;
}

HDIRECTORY __stdcall AddDirectory (HDIRSEL dirSel, const char* path) {
	if (dirSel && path) {
		CDirItem* item;
		if (((CDirectoryTree*)dirSel) -> addDir (path, item) != -1)
			return item;
		else
			return NULL;
	} else
		return NULL;
}

int __stdcall AddFile (HDIRSEL dirSel, const char* fullName, long size, long dirIndex) {
	if (dirSel && fullName)
		return ((CDirectoryTree*) dirSel) -> addFile (fullName, size, dirIndex);
	else
		return 0;
}

int __stdcall AddFileToDirectory (HDIRSEL dirSel, const char* name, long size, HDIRECTORY dir) {
	if (dirSel && dir && name) {
		return ((CDirectoryTree*) dirSel) -> addFileToDir (name, size, (CDirItem*)dir);
	} else
		return 0;
}

int __stdcall ShowDialog (HDIRSEL dirSel) {
	if (dirSel)
		return DialogBoxParam (hInst, MAKEINTRESOURCE (DIRTREESEL_DIALOG),
			GetFocus(), (DLGPROC)DirTreeSelectorDialogProc, (LPARAM)dirSel);
	else
		return IDCANCEL;
}

void __stdcall DestroyDirSelector (HDIRSEL dirSel) {
	if (dirSel) delete ((CDirectoryTree*)dirSel);
}


int __stdcall RegisterFileName (char* name, CSimpleList* list) {
	if (list && name)
		list->addToTail (name);
	return 0;
}

NLItem* __stdcall CreateNameList (HDIRSEL dirSel, int includeWhat) {
	NLItem* res = NULL;
	if (dirSel) {
		CDirectoryTree* dirTree = (CDirectoryTree*) dirSel;
		CSimpleList list;
		dirTree->forEach (includeWhat, (RegexpTester)RegisterFileName, &list);
		res = list. getHead();
	}
	return res;
}

void __stdcall DestroyNameList (NLItem* first) {
	NLItem* temp;
	while (first) {
		temp = first->next;
		delete (first);
		first = temp;
	}
}