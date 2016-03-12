#ifndef _TREESEL_CDIRTREE_H
#define _TREESEL_CDIRTREE_H

#include <string.h>
#include <windows.h>
#include <commctrl.h>

#include "Lists.h"
#include "TreeSel.h"


class  CDirectoryTree {
friend void __stdcall OnItemChanged (CListItem*, int, CDirectoryTree*);
protected:
	CDirItem* root;
	CList* dirlist; // temporary container, used only at initialization
	int showRoot; // if user has specified files without path, then
	// to make them available we should include the "Root" dir in the tree.
	// In this case showRoot is 1.
	int cleared;
	RegexpTester matchFunc;
	long prevSelCount;

	HWND hwndDlg, hwndTree, hwndList, hwndStatus;
	HIMAGELIST imgState, imgFold;
	CDirItem* currentList;
public:
	CDirectoryTree (RegexpTester match = NULL):
		root (NULL), dirlist (NULL), currentList (NULL),
		matchFunc ( (match)?match:&WildMatch ),
		cleared (0) {};
	virtual ~CDirectoryTree() {
		if (dirlist) delete (dirlist);
		if (root) delete (root);
	};
	int addFileToDir (const char* name, long size, CDirItem* dir);
	int addFile (const char* fullName, long size, long dirIndex);
		// If fileName includes path, this directory is searched through
		//   dir list, if not found then new dir item is created.
		// If there is no path in the fileName, dirIndex must be specified,
		//   it is the number under which the file path was added in call
		//   to addDir function.
		// Since adding file with path can change indexing in the dir list it
		//   is preferred to use only one method - either indexing or path names.
	long addDir (const char* fullPath, CDirItem* &dir); // returns index of directory
		// fullPath can be "path1\..\pathN\dirname".
		// Only dirs containing files should be added.
		// If there are files in the topmost directory (i.e. without
		//   path) it is good idea to prefix all the dirNames with ".\" or
		//   some other string, so these files can be accessed too.
		// If such path prefixing is not made, the "Root" directory will be
		//   showed automaticaly, but all the file names in tree will
		//   remain the same. "Root" dir is used only to show topmost files in
		//   the dialog box.
		// Dir parameter can be used to speed up file adding, use it in
		//   call to addFileToDir.
protected:
	void clearTemp() {
		if (!cleared) {
			if (dirlist) delete(dirlist);
			dirlist = NULL;
			if (root) {
				root->buildIndexes();
				showRoot = (root->getFiles() && root->getFiles()->getCount() != 0);
				root->setParentName (NULL, 0);
			}
			cleared = 1;
		}
	};
public:	
	// Dialog functions
	void onInitDialog (HWND hwnd);
	void onTreeViewSelChanged (CDirItem* item);
	void onTreeViewClick (POINT pnt);
	void onTreeViewSpace();
	void onListViewSelected (CFileItem* item);
	void onListViewClick (POINT pnt, int testWhat);
	void onListViewSpace();
	void beforeKillDialog();
protected:
	void selectTreeItem (HTREEITEM hItem);
	void selectListItem (int iItem);
	void setStatusText (const char* str);
	void createTree ();
	void createTree (CDirItem* from);
	void showList (CDirItem* item);
	void renewParent (CListItem* item);
	void invertSelection (CDirItem* item);
	void selectFile (CFileItem* item);
	long getSelCount() { return (root? root->getSelCount():0); };
	void showSelChange();
public:
	void selectGroup (int sel, int recursive, const char* mask, int testpath);
	void showStatistics();
	void forEach (int what, RegexpTester func, const void* param);
	void sortList (CDirItem* item);
	HMENU hSortMenu;
};

#endif