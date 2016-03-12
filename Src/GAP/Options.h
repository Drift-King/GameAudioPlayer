#ifndef _GAP_OPTIONS_H
#define _GAP_OPTIONS_H

#include <windows.h>

void SaveOptions(void);
void LoadOptions(void);
int  CreateOptionsDlg(HWND hwnd);
void SetCheckBox(HWND hwnd, int cbId, BOOL cbVal);
BOOL GetCheckBox(HWND hwnd, int cbId);
void EnableApply(HWND hwnd);
void DisableApply(HWND hwnd);
void TrackPropPage(HWND hwnd, int index);

#endif
