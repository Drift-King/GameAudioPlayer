/*
 * Directory Tree Selector Library source code: dialog box procs
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

#include <windows.h>
#include <commctrl.h>

#include "Dialogs.h"
#include "resource.h"
#include "CDirTree.h"
#include "Lists.h"
#include "General.h"
#include "TreeSel.h"


CList History;

BOOL CALLBACK DirTreeSelectorDialogProc (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam){
	CDirectoryTree* dirTree;
	POINT pnt;

	switch (uMsg)
	{
	case WM_NOTIFY:
		{
			NMHDR* nmh = (LPNMHDR) lParam;
			dirTree = (CDirectoryTree*) GetWindowLong (hwndDlg, GWL_USERDATA);
			if (!dirTree) return FALSE;

			if (nmh->idFrom == ID_TREE) {
				NM_TREEVIEW* nmtv = (LPNM_TREEVIEW) lParam;
				if (nmh->code == TVN_SELCHANGED)
					dirTree->onTreeViewSelChanged ((CDirItem*) nmtv->itemNew.lParam);
				else if (nmh->code == NM_CLICK) {
					GetCursorPos (&pnt);
					dirTree->onTreeViewClick (pnt);
				} else if (nmh->code == TVN_KEYDOWN) {
					TV_KEYDOWN* tvkd = (TV_KEYDOWN*) lParam;
					if (tvkd->wVKey == VK_INSERT)
						dirTree->onTreeViewSpace();
				}
			} else if (nmh->idFrom == ID_LIST) {
				NM_LISTVIEW* nmlv = (LPNM_LISTVIEW) lParam;
				if (nmh->code == LVN_ITEMCHANGED) {
					if ( (nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED) )
						dirTree->onListViewSelected ((CFileItem*) nmlv->lParam);
				} else if (nmh->code == NM_CLICK || nmh->code == NM_DBLCLK) {
					GetCursorPos (&pnt);
					dirTree->onListViewClick (pnt, (nmh->code == NM_CLICK)?LVHT_ONITEMSTATEICON:LVHT_ONITEMLABEL );
				} else if (nmh->code == LVN_KEYDOWN) {
					LV_KEYDOWN* lvkd = (LV_KEYDOWN*) lParam;
					if (lvkd->wVKey == VK_INSERT || lvkd->wVKey == VK_SPACE)
						dirTree->onListViewSpace();
				} else if (nmh->code == NM_RCLICK) {
					GetCursorPos (&pnt);
					CheckMenuItem (dirTree->hSortMenu, !sortType?ID_NAMESORT:ID_EXTSORT, MF_BYCOMMAND | MF_CHECKED);
					CheckMenuItem (dirTree->hSortMenu, sortType?ID_NAMESORT:ID_EXTSORT, MF_BYCOMMAND | MF_UNCHECKED);
					TrackPopupMenu (GetSubMenu (dirTree->hSortMenu,0), TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pnt. x, pnt. y,
						0, hwndDlg, NULL);
				}
			}
		}
		break;
	case WM_INITDIALOG:
//		Recursively = TRUE;
		CenterInParent (hwndDlg);

		SetWindowLong (hwndDlg, GWL_USERDATA, lParam);
		dirTree = (CDirectoryTree*) lParam;
		if (dirTree)
			dirTree->onInitDialog (hwndDlg);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_SELECT:
		case ID_DESELECT:
			dirTree = (CDirectoryTree*) GetWindowLong (hwndDlg, GWL_USERDATA);
			if (!dirTree) break;
			deSelect = (LOWORD(wParam)==ID_DESELECT);
			if ( DialogBox (hInst, MAKEINTRESOURCE (WILDCARD_DIALOG), hwndDlg, (DLGPROC)WildcardDialogProc)==IDOK ) {
				dirTree->selectGroup ( deSelect?ITEM_SELECT_NONE:ITEM_SELECT_ALL,
					Recursively?RECURSIVE:NO_RECURSIVE, Mask, FullName);
				dirTree->showStatistics();
			}
			break;
		case ID_NAMESORT:
		case ID_EXTSORT: {
			int oldSortType = sortType;
			sortType = (LOWORD(wParam) == ID_NAMESORT)? 0: 1;
			if (sortType != oldSortType) {
				dirTree = (CDirectoryTree*) GetWindowLong (hwndDlg, GWL_USERDATA);
				if (!dirTree) break;
					dirTree -> sortList (NULL);
			}
			break;}
		case IDOK:
		case IDCANCEL:
			dirTree = (CDirectoryTree*) GetWindowLong (hwndDlg, GWL_USERDATA);
			if (dirTree)
				dirTree->beforeKillDialog();
			EndDialog (hwndDlg, LOWORD(wParam));
			break;
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
	return TRUE;
}


BOOL CALLBACK WildcardDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND wild;
	CListItem* item;
	
	switch (uMsg)
	{
	case WM_INITDIALOG:
		CenterInParent (hwndDlg);

		CheckRadioButton (hwndDlg, ID_ANYWHERE, ID_CURRENT, Recursively ? ID_ANYWHERE:ID_CURRENT);
		CheckDlgButton (hwndDlg, ID_FULLNAME, FullName?BST_CHECKED:BST_UNCHECKED);
		EnableWindow (GetDlgItem (hwndDlg, ID_FULLNAME), Recursively);
		SetWindowText (hwndDlg,  deSelect ? "Deselect Group":"Select Group");

		wild = GetDlgItem(hwndDlg, ID_WILD);
		History. beginEnum();
		while ( (item = History. getNextItem()) != NULL ) {
			SendMessage (wild, CB_ADDSTRING, 0, (long)item->asString());
		}
		SetDlgItemText (hwndDlg, ID_WILD, Mask);
		break;
	case WM_COMMAND:
		switch LOWORD(wParam)
		{
		case IDOK:
			GetDlgItemText (hwndDlg, ID_WILD, Mask, 200);
			Recursively = IsDlgButtonChecked (hwndDlg, ID_ANYWHERE);
			FullName = IsDlgButtonChecked (hwndDlg, ID_FULLNAME);

			if (strlen(Mask) != 0) {
				History. removeNode ( History. findName (Mask) );
				History. addToHead ( new CListItem (Mask) );
			}
		case IDCANCEL:
			EndDialog (hwndDlg, LOWORD(wParam));
			break;
		case ID_ANYWHERE:
		case ID_CURRENT:
			EnableWindow (GetDlgItem (hwndDlg, ID_FULLNAME),
				IsDlgButtonChecked (hwndDlg, ID_ANYWHERE));
			break;
		}
		break;
	case WM_CLOSE:
		EndDialog (hwndDlg, IDCANCEL);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}
