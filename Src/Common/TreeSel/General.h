#ifndef _TREESEL_GENERAL_H
#define _TREESEL_GENERAL_H

#include <windows.h>


// Notification constants
#define UNKNOWN_NOTIFICATION 0
#define FILE_NOTIFICATION 1
#define DIR_NOTIFICATION 2
// DirItem consts
#define ITEM_SELECT_ALL 1
#define ITEM_SELECT_NONE 0
#define ITEM_SELECT_PART 2
#define RECURSIVE 1
#define NO_RECURSIVE 0
// Root item name
#define ROOTNAME "Root"

extern BOOL deSelect;
extern CHAR Mask[200];
extern BOOL Recursively;
extern BOOL FullName;
extern HINSTANCE hInst;
extern int sortType;

#endif