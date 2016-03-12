#ifndef _GAP_LISTVIEW_H
#define _GAP_LISTVIEW_H

#include <windows.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

#define NO_SELECTION (-1)

extern HWND playList;

void InitListView(void);
void BuildNodeChain(HWND hwnd);
void UpdatePlaylistSelection(void);
AFile* GetListViewNode(int index);
int  GetListViewIndex(AFile* node);
int  GetItemForPos(const POINT *point);
void EnsureSelection(BOOL bCheckCursor);
int  GetCurrentSelection(void);
void SetCurrentSelection(int item);
void InsertNodeIntoListView(int index, AFile *node);
void AddNodeToListView(AFile *node);
void UpdatePlaylistNodeTime(AFile* node);

#endif
