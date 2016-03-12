#ifndef _GAP_GLOBALS_H
#define _GAP_GLOBALS_H

#include <windows.h>

#define MAX_FILE   (60000ul)
#define MAX_FILTER (30000ul)

#define BUFFERSIZE (64000ul)

extern HINSTANCE hInst;
extern HMENU     hMenu;
extern char      szAppName[];
extern char      szINIFileName[MAX_PATH];
extern char		 szAppDirectory[MAX_PATH];

#endif
