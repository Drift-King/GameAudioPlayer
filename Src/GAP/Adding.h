#ifndef _GAP_ADDING_H
#define _GAP_ADDING_H

#include <windows.h>

extern BOOL opAllowMultipleAFPlugins,
            opAllowMultipleRFPlugins;

void  Add(HWND hwnd);
DWORD AddFile(HWND hwnd, const char* fname);
void  AddDir(HWND hwnd);

#endif
