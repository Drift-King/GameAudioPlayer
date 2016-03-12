#ifndef _GAP_SCANNING_H
#define _GAP_SCANNING_H

#include <windows.h>

typedef struct tagSearchState
{
	SearchPattern* lpPattern;
	DWORD *table;
	DWORD  curLength;
} SearchState;

#define PSSTATE(p) ((SearchState*)(p))

void  Find(HWND hwnd);
DWORD Scan(HWND hwnd, const char* file);
void  ScanDir(HWND hwnd);
void  InitSearchState(SearchState* lpSearchState, SearchPattern* lpPattern);
void  ShutdownSearchState(SearchState* lpSearchState);

#endif
