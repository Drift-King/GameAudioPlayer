#ifndef _GAP_PLAYLIST_NODEOPS_H
#define _GAP_PLAYLIST_NODEOPS_H

#include <windows.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

extern AFile *list_start,*list_end,*cursel,*curNode;
extern int    list_size;
extern BOOL   list_changed,opNoTime;
extern char   list_filename[MAX_PATH];

void Clear(HWND hwnd);
AFile* Duplicate(AFile* node);
void DeleteNode(int index);
void AddNode(AFile *node);
void SwapNodes(int index);
void MoveNodeUp(int index);
void MoveNodeDown(int index);
void DuplicateNode(int index);
AFile* GetNodeByIndex(int index);
int  GetNodePos(AFile* node);

#endif
