#ifndef _TREESEL_DIALOGS_H
#define _TREESEL_DIALOGS_H

#include <windows.h>

BOOL CALLBACK DirTreeSelectorDialogProc (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK WildcardDialogProc (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif