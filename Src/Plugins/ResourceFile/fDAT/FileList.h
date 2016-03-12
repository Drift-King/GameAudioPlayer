#ifndef _GAP_FDAT_FILELIST_H
#define _GAP_FDAT_FILELIST_H

#include <windows.h>

#include "..\RFPlugin.h"


RFile* __stdcall PluginGetFileList (const char* resname);
BOOL __stdcall PluginFreeFileList (RFile* list);

#endif