/*
 * Directory Tree Selector Library source code: CDirectoryTree class
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

#include <string.h>
#include <windows.h>
#include <commctrl.h>

#include "CDirTree.h"
#include "General.h"
#include "Lists.h"
#include "TreeSel.h"
#include "resource.h"


long CDirectoryTree::addDir (const char* path, CDirItem* &dir) {
	long res = -1;
	char* path1 = strdup (path);
	char* pos = path1;
	while ( pos = strchr(pos, '/') ) pos[0] = '\\'; // replace all slashes by backslashes
	if (strchr(path1, '\0')[-1] == '\\') strchr(path1, '\0')[-1] = '\0'; // and remove final one
	if (!dirlist) {
		dirlist = new CList;
		root = new CDirItem (ROOTNAME);
	}
	CDirItem* dlItem;
	if ( !(dlItem = (CDirItem*)dirlist->findName(path1, res)) ) {
		dlItem = new CDirItem (path1);
		dirlist->addToTail (dlItem, root->addSubDir (path1));
		res = dirlist->getCount()-1;
	}
	if (dlItem) dir = (CDirItem*)dlItem->getParent();
	free (path1);
	return res;
}

int CDirectoryTree::addFile (const char* fullName, long size, long dirIndex) {
	CDirItem* dir;
	char *path = strdup (fullName), *name;
	if (!dirlist) {
		dirlist = new CList;
		root = new CDirItem (ROOTNAME);
	}
	if (dirIndex != -1) {
		dirlist->reindex (REBUILD_INDEX);
		dir = (CDirItem*)(*dirlist)[dirIndex];
		dir = (dir) ? (CDirItem*)dir->getParent() : NULL;
		name = path;
	} else {
		char* pos = path;
		while ( pos = strchr(pos, '/') ) pos[0] = '\\';
		name = strrchr (path, '\\');
		if (name) {
			*name = '\0';
			name++;
			addDir (path, dir);
		} else {
			name = path;
			dir = root;
		}
	}
	int res = addFileToDir (name, size, dir);
	free (path);
	return res;
}

int CDirectoryTree::addFileToDir (const char* name, long size, CDirItem* dir) {
	if (dir) {
		dir->addFile (name, size);
		return 1;
	} else
		return 0;
}

// implementation of dialog funcs

void CDirectoryTree::onInitDialog (HWND hwnd) {
	LV_COLUMN col;

	hwndDlg = hwnd;

	clearTemp ();

	imgState = ImageList_LoadBitmap (hInst, MAKEINTRESOURCE (IDB_95CHECK), 11, 4, RGB (0, 255, 0) );
	imgFold  = ImageList_LoadBitmap (hInst, MAKEINTRESOURCE (IDB_FOLDER),  16, 3, RGB (0, 255, 0) );

	hwndTree = GetDlgItem (hwndDlg, ID_TREE);
	TreeView_SetImageList (hwndTree, imgFold,  TVSIL_NORMAL);
	TreeView_SetImageList (hwndTree, imgState, TVSIL_STATE);

	hwndList = GetDlgItem (hwndDlg, ID_LIST);
	ListView_SetImageList (hwndList, imgState, LVSIL_STATE);
		col. mask = LVCF_TEXT | LVCF_WIDTH;
		col. pszText = "File Name";
		col. cchTextMax = 9;
		col. cx = 130;
	ListView_InsertColumn (hwndList, 0, &col);
		col. mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
		col. pszText = "Size";
		col. cchTextMax = 4;
		col. cx = 80;
		col. iSubItem = 1;
		col. fmt = LVCFMT_RIGHT;
	ListView_InsertColumn (hwndList, 1, &col);

	hSortMenu = LoadMenu (hInst, MAKEINTRESOURCE (IDR_SORTMENU) );

	createTree();
	hwndStatus = GetDlgItem (hwndDlg, ID_STATUS);
	showStatistics();
}

void CDirectoryTree::onTreeViewSelChanged (CDirItem* item) {
	showList (item);
	if (item)
		setStatusText (item->getFullName());
	showStatistics();
}

void CDirectoryTree::selectTreeItem (HTREEITEM hItem) {
	TV_ITEM tvi;
	tvi. hItem = hItem;
	tvi. mask = TVIF_PARAM;
	TreeView_GetItem (hwndTree, &tvi);
	invertSelection ((CDirItem*) tvi.lParam);
	showStatistics();
}

void CDirectoryTree::onTreeViewClick (POINT pnt) {
	ScreenToClient (hwndTree, &pnt);
	TV_HITTESTINFO tvht;
	tvht. pt = pnt;
	TreeView_HitTest (hwndTree, &tvht);

	if (tvht. hItem && (tvht. flags & TVHT_ONITEMSTATEICON))
		selectTreeItem (tvht. hItem);
}

void CDirectoryTree::onTreeViewSpace() {
	HTREEITEM hItem = TreeView_GetNextItem (hwndTree, NULL, TVGN_CARET);
	if (hItem) {
		selectTreeItem (hItem);
		hItem = TreeView_GetNextVisible (hwndTree, hItem);
// Uncomment to make 'Insert Moves Cursor' in TreeView
//		if (hItem)
//			TreeView_SelectItem (hwndTree, hItem);
	}
}

void CDirectoryTree::onListViewSelected (CFileItem* item) {
	if (item) {
		const char* path = ((CDirItem*)item->getParent()) -> getFullName();
		long len = 1;
		char* str;
		if (path) len += strlen(path);
		if (item->asString()) len += strlen (item->asString());
		(str = (char*)malloc (len))[0] = '\0';
		if (path) strcat (str, path);
		if (item->asString()) strcat (str, item->asString());
		setStatusText (str);
		free (str);
	}
}

void CDirectoryTree::selectListItem (int iItem) {
	LV_ITEM lvi;
	lvi. iItem = iItem;
	lvi. mask = LVIF_PARAM;
	ListView_GetItem (hwndList, &lvi);
	selectFile ((CFileItem*) lvi.lParam);
	showStatistics();
}

void CDirectoryTree::onListViewClick (POINT pnt, int testWhat) {
	ScreenToClient (hwndList, &pnt);
	LV_HITTESTINFO lvht;
	lvht. pt = pnt;
	int ind = ListView_HitTest (hwndList, &lvht);
	if (ind >= 0 && (lvht. flags & testWhat))
		selectListItem (ind);
}

void CDirectoryTree::onListViewSpace() {
	int iItem = ListView_GetNextItem (hwndList, -1, LVNI_SELECTED);
	if (iItem == -1)
		iItem = ListView_GetNextItem (hwndList, -1, LVNI_FOCUSED);
	if (iItem >= 0) {
		selectListItem (iItem);
		iItem = ListView_GetNextItem (hwndList, iItem, LVNI_BELOW);
		if (iItem >= 0) {
			ListView_SetItemState (hwndList, iItem, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
			ListView_EnsureVisible (hwndList, iItem, FALSE);
		}
	}
}

void CDirectoryTree::beforeKillDialog() {
	TreeView_SetImageList (hwndTree, NULL, TVSIL_NORMAL);
	TreeView_SetImageList (hwndTree, NULL, TVSIL_STATE);
	ListView_DeleteAllItems (hwndList);
	ListView_SetImageList (hwndList, NULL, LVSIL_STATE);
	ImageList_Destroy (imgState);
	ImageList_Destroy (imgFold);
	DestroyMenu (hSortMenu);
}

void CDirectoryTree::setStatusText (const char* str) {
	SendMessage (hwndStatus, SB_SETTEXT, 0, (LPARAM)str);
}

void CDirectoryTree::createTree (CDirItem* from) {
	TV_INSERTSTRUCT tvis;
	HTREEITEM hItem;
	if (from) {
		tvis. hInsertAfter = TVI_SORT;
		if (from->getParent())
			tvis. hParent = (HTREEITEM)from->getParent()->getValue();
		else
			tvis. hParent = TVI_ROOT;
		tvis. item. mask = TVIF_TEXT | TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvis. item. pszText = (char*)from->asString();
		tvis. item. cchTextMax = strlen (from->asString());
		tvis. item. stateMask = TVIS_STATEIMAGEMASK;
		tvis. item. state = INDEXTOSTATEIMAGEMASK (1);
		if ( from->getFiles() ) {
			tvis. item. iImage = 1;
			tvis. item. iSelectedImage = 0;
		} else {
			tvis. item. iImage = 2;
			tvis. item. iSelectedImage = 2;
		}
		tvis. item. lParam = (LPARAM)from;
		from->setValue ( (long)(hItem = TreeView_InsertItem (hwndTree, &tvis)) );

//		if ( from->getFiles() ) {
//			tvis. item. hItem = hItem;
//			tvis. item. mask = TVIF_STATE;
//			tvis. item. state = TVIS_BOLD;
//			tvis. item. stateMask = TVIS_BOLD;
//			TreeView_SetItem (hwndTree, &tvis. item);
//		}

		if (from->getSubdirs())
			for (long i=0; i<from->getSubdirs()->getCount(); i++)
				createTree ( (CDirItem*)((*from->getSubdirs())[i]) );

		TreeView_SelectItem (hwndTree, TreeView_GetFirstVisible (hwndTree));
	}
}

void CDirectoryTree::createTree() {
	if (!root) return;
	if (showRoot)
		createTree (root);
	else if ( root->getSubdirs() ) {
		root->setValue (0);
		for (long i=0; i<root->getSubdirs()->getCount(); i++)
			createTree ( (CDirItem*)((*root->getSubdirs())[i]) );
	}
}

void CDirectoryTree::showList (CDirItem* item) {
	if (item == currentList) return;
	ListView_DeleteAllItems (hwndList);
	if (item && item->getFiles()) {
		LVITEM lvi;
		SendMessage (hwndList, WM_SETREDRAW, FALSE, 0);
		for (long i=0; i<item->getFiles()->getCount(); i++) {
			CFileItem* file = (CFileItem*)(*item->getFiles())[i];
			lvi. mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
			lvi. pszText = (char*)file->asString();
			lvi. stateMask = LVIS_STATEIMAGEMASK;
			lvi. state = INDEXTOSTATEIMAGEMASK ( file->getSelected()+2 );
			lvi. lParam = (LPARAM)file;
			lvi. iSubItem = 0;
			long ind = ListView_InsertItem (hwndList, &lvi);
			file->setValue (ind);

			if (file->getSize() != FILE_SIZE_UNKNOWN) {
				char size[20];
				lvi. mask = LVIF_TEXT;
				lvi. iItem = ind;
				lvi. iSubItem = 1;
				lvi. pszText = ltoa (file->getSize(), size, 10);
				lvi. cchTextMax = strlen (size);
				ListView_SetItem (hwndList, &lvi);
			}
		}
		SendMessage (hwndList, WM_SETREDRAW, TRUE, 0);
		InvalidateRect (hwndList, NULL, FALSE);

		sortList (item);
		ListView_SetItemState (hwndList, 0, LVIS_FOCUSED, LVIS_FOCUSED);
	}
	currentList = item;
}

void __stdcall OnItemChanged (CListItem* item, int from, CDirectoryTree* dirTree) {
	if (item && dirTree)
		if (from == FILE_NOTIFICATION && dirTree->currentList == item->getParent()) {
			CFileItem* file = (CFileItem*) item;
			ListView_SetItemState (dirTree->hwndList, file->getValue(),
				INDEXTOSTATEIMAGEMASK (file->getSelected() + 2), LVIS_STATEIMAGEMASK);
		} else if (from == DIR_NOTIFICATION) {
			TV_ITEM tvi;
			if (tvi. hItem = (HTREEITEM)item->getValue()) {
				tvi. mask = TVIF_STATE;
				tvi. stateMask = TVIS_STATEIMAGEMASK;
				tvi. state = INDEXTOSTATEIMAGEMASK (((CDirItem*)item)->getSelType() + 1);
				TreeView_SetItem (dirTree->hwndTree, &tvi);
			}
		}
}

void CDirectoryTree::renewParent (CListItem* item) {
	if (!item) return;
	while (item = item->getParent())
		OnItemChanged (item, DIR_NOTIFICATION, this);
}

void CDirectoryTree::invertSelection (CDirItem* item) {
	if (!item) return;
	prevSelCount = getSelCount();
	item->invertSel ((ItemNotification)&OnItemChanged, (long)this);
	renewParent (item);
	showSelChange();
}

void CDirectoryTree::selectFile (CFileItem* item) {
	if (!item) return;
	prevSelCount = getSelCount();
	item->setSelected ( item->getSelected()?ITEM_SELECT_NONE:ITEM_SELECT_ALL,
		(ItemNotification)&OnItemChanged, (long)this);
	renewParent (item);
	showSelChange();
}

void CDirectoryTree::selectGroup (int sel, int recursive, const char* mask, int testpath) {
	prevSelCount = getSelCount();
	if (recursive) {
		root->selectAll (sel, recursive, (ItemNotification)&OnItemChanged, (long)this,
			matchFunc, mask, testpath);
	} else {
		if (currentList) {
			currentList->selectAll (sel, recursive, (ItemNotification)&OnItemChanged,
				(long)this, matchFunc, mask, 0);
			renewParent (currentList);
		}
	}
	showSelChange();
}

void CDirectoryTree::showSelChange() {
	char str[200] = "";
	long cnt = getSelCount() - prevSelCount;
	if (cnt) {
		ltoa (labs (cnt), str, 10);
		strcat (str, " file(s) ");
		if (cnt < 0) strcat (str,"de");
		strcat (str, "selected");
	} else
		strcpy (str, "Selection is unchanged");
	SendMessage (hwndStatus, SB_SETTEXT, 0, (LPARAM)str);
}

const int IDs[8] = {ID_TOTSIZE, ID_TOTCNT, ID_TOTSELSIZE, ID_TOTSELCNT,
					ID_CURSIZE, ID_CURCNT, ID_CURSELSIZE, ID_CURSELCNT};
void CDirectoryTree::showStatistics() {
	long vals[8] =
		{ root ? root->getTotalSize():0,
		  root ? root->getTotalCount():0,
		  root ? root->getSelSize():0,
		  root ? root->getSelCount():0,
		  currentList ? currentList->getCurSize():0,
		  currentList ? currentList->getCurCount():0,
		  currentList ? currentList->getCurSelSize():0,
		  currentList ? currentList->getCurSelCount():0 };
	for (int i=0; i<8; i++)
		SetDlgItemInt (hwndDlg, IDs[i], vals[i], TRUE);
}

void CDirectoryTree::forEach (int what, RegexpTester func, const void* param) {
	if (root)
		root->selectAll (what, RECURSIVE, NULL, NULL,
			func, (const char*)param, 1);
}


int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	if (!lParam1)
		return -1;
	if (!lParam2)
		return 1;
	CFileItem *file1 = (CFileItem*) lParam1,
		*file2 = (CFileItem*) lParam2;

	int res = 0;

	if (sortType) {
		const char *ext1 = strrchr (file1->asString(), '.'),
			*ext2 = strrchr (file2->asString(), '.');
		if (!ext1 && ext2) return -1;
		if (ext1 && !ext2) return 1;
		if (ext1 && ext2)
			res = strcmpi (ext1+1, ext2+1);
	}

	if (!res)
		res = strcmpi (file1->asString(), file2->asString());
	return res;
}

void CDirectoryTree::sortList (CDirItem* item) {
	if (!item)
		item = currentList;
	if (!item || !item->getFiles())
		return;

	ListView_SortItems (hwndList, &CompareFunc, NULL);

	for (int i=0; i<item->getFiles()->getCount(); i++) {
		LV_ITEM lvi;
		lvi. iItem = i;
		lvi. mask = TVIF_PARAM;
		ListView_GetItem (hwndList, &lvi);
		CFileItem* file = (CFileItem*) lvi. lParam;
		if (file)
			file->setValue (i);
	}
}