#ifndef _GAP_OPTIONS_AFPLUGINS_H
#define _GAP_OPTIONS_AFPLUGINS_H

#include <windows.h>

void EnableButtons(HWND hwnd, BOOL cstate, BOOL astate);
LONG GetCurPlugin(HWND hwnd);
LRESULT CALLBACK AFPluginsOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm);

#endif
