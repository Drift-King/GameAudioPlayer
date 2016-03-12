#ifndef _GAP_ERRORS_H
#define _GAP_ERRORS_H

#include <windows.h>
#include <mmsystem.h>

void ReportMMError(HWND hwnd, MMRESULT mmr, const char *title);
void ReportError(HWND hwnd, const char* errstr, HMODULE hmod);

#endif
