/*
 * Game Audio Player source code: playlist listview control
 *
 * Copyright (C) 1998-2000 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <windows.h>
#include <commctrl.h>

#include <string.h>

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Playlist.h"
#include "ListView.h"
#include "Playlist_NodeOps.h"
#include "Misc.h"
#include "resource.h"

HWND playList;

AFile* GetListViewNode(int index)
{
	LV_ITEM lvi;

	if ((playList==NULL) || (index==-1))
		return NULL;

	lvi.mask=LVIF_PARAM;
	lvi.iItem=index;
	lvi.iSubItem=0;

	return ((ListView_GetItem(playList,&lvi))?((AFile*)lvi.lParam):NULL);
}

int GetListViewIndex(AFile* node)
{
	LV_FINDINFO lvfi;

	if ((playList==NULL) || (node==NULL))
		return (-1);

	lvfi.flags=LVFI_PARAM;
	lvfi.lParam=(LPARAM)(DWORD)node;

	return ListView_FindItem(playList,-1,&lvfi);
}

int GetItemForPos(const POINT *point)
{
	int  index,top,bottom;
	RECT rect;

	if ((playList==NULL) || (point==NULL))
		return (-1);

	top=ListView_GetTopIndex(playList);
	bottom=top+ListView_GetCountPerPage(playList);	
	for (index=top;index<=bottom;index++)
	{
		ListView_GetItemRect(playList,index,&rect,LVIR_BOUNDS);
		if (
			(point->x<=rect.right) &&
			(point->x>=rect.left) &&
			(point->y<=rect.bottom) &&
			(point->y>=rect.top)
		   )
			return index;
	}

	return (-1);
}

void EnsureSelection(BOOL bCheckCursor)
{
	int   index;
	POINT point;

	if (playList==NULL)
		return;

	if (ListView_GetSelectedCount(playList)>1)
		return;

	if (bCheckCursor)
	{
		GetCursorPos(&point);
		ScreenToClient(playList,&point);
		index=GetItemForPos(&point);		
		if (index!=-1)
		{
			SetCurrentSelection(index);
			return;
		}
	}
	index=GetCurrentSelection();
    if (index==NO_SELECTION)
		SetCurrentSelection(GetListViewIndex(cursel));
    else
		cursel=GetListViewNode(index);
}

void UpdatePlaylistSelection(void)
{
	if (playList==NULL)
		return;
	UpdateWindow(playList);
	if (ListView_GetSelectedCount(playList)>1)
		return;

    SetCurrentSelection(GetListViewIndex(curNode));
}

void BuildNodeChain(HWND hwnd)
{
	int    i,count;
	AFile *node,*prevnode;

	if (hwnd==NULL)
		return;

	count=ListView_GetItemCount(hwnd);
	prevnode=NULL;
	node=NULL;
	list_start=NULL;
	list_size=0;
	for (i=0;i<count;i++)
	{
		node=GetListViewNode(i);
		if (node!=NULL)
		{
			node->prev=prevnode;
			if (prevnode!=NULL)
				prevnode->next=node;
			node->next=NULL;
			list_size++;
			if (list_start==NULL)
				list_start=node;
			prevnode=node;
		}
	}
	list_end=node;
	list_changed=TRUE;
}

void InitListView(void)
{
    LV_COLUMN lvc;
    char      text[20];
    int       i;
    AFile    *curnode;

	if (playList==NULL)
		return;

    lvc.mask=LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
    lvc.fmt=LVCFMT_LEFT;
    lvc.cx=245;
    lvc.pszText=text;
    lvc.iSubItem=0;
    lstrcpy(text,"Audio File Title");
    ListView_InsertColumn(playList,0,&lvc);

    lvc.fmt=LVCFMT_RIGHT;
    lvc.cx=35;
    lvc.iSubItem=1;
    lstrcpy(text,"Time");
    ListView_InsertColumn(playList,1,&lvc);

	lvc.fmt=LVCFMT_LEFT;
    lvc.iSubItem=2;
    lstrcpy(text,"AF Type");
    ListView_InsertColumn(playList,2,&lvc);

    lvc.iSubItem=3;
    lstrcpy(text,"Resource File Name     ");
    ListView_InsertColumn(playList,3,&lvc);

    lvc.iSubItem=4;
    lstrcpy(text,"RF Type");
    ListView_InsertColumn(playList,4,&lvc);

    lvc.iSubItem=5;
    lstrcpy(text,"RF Data String");
    ListView_InsertColumn(playList,5,&lvc);

	lvc.fmt=LVCFMT_RIGHT;
    lvc.iSubItem=6;
    lstrcpy(text,"    File Start");
    ListView_InsertColumn(playList,6,&lvc);

    lvc.iSubItem=7;
    lstrcpy(text,"    File Length");
    ListView_InsertColumn(playList,7,&lvc);

    ListView_SetColumnWidth(playList,2,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(playList,3,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(playList,4,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(playList,5,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(playList,6,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(playList,7,LVSCW_AUTOSIZE_USEHEADER);

    curnode=list_start;
    cursel=curNode;
    i=0;
    while (curnode!=NULL)
    {
		AddNodeToListView(curnode);
		if (curnode!=cursel)
		{
			ListView_SetItemState(playList,i,0,LVIS_SELECTED | LVIS_FOCUSED);
		}
		else
		{
			ListView_EnsureVisible(playList,i,FALSE);
			ListView_SetItemState(playList,i,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
		}
		curnode=curnode->next;
		i++;
	}
    if (GetCurrentSelection()==NO_SELECTION)
		SetCurrentSelection(0);
}

int GetCurrentSelection(void)
{
    if (playList==NULL)
		return NO_SELECTION;

	 return (ListView_GetNextItem(playList,-1,LVNI_ALL | LVNI_SELECTED));
}

void SetCurrentSelection(int item)
{
    int i,count;

    if (playList==NULL)
		return;

    count=ListView_GetItemCount(playList);
    for (i=0;i<count;i++)
    {
		if (i!=item)
		{
			ListView_SetItemState(playList,i,0,LVIS_SELECTED | LVIS_FOCUSED);
		}
		else
		{
			ListView_EnsureVisible(playList,i,FALSE);
			ListView_SetItemState(playList,i,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
			cursel=GetListViewNode(i);
		}
    }
}

void InsertNodeIntoListView(int index, AFile *node)
{
    LV_ITEM lvi;
    char str[100];

	if (
		(playList==NULL) || 
		(node==NULL) || 
		(index==-1)
	   )
		return;

    lvi.mask=LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
    lvi.iItem=index;
    lvi.iSubItem=0;
    lvi.state=0;
    lvi.stateMask=LVIS_SELECTED;
    lvi.pszText=node->afName;
    lvi.lParam=(LPARAM)(DWORD)node;
    ListView_InsertItem(playList,&lvi);
	if (node->afTime==-1)
		lstrcpy(str,"N/A");
    else
		GetShortTimeString((node->afTime)/1000,str);
    ListView_SetItemText(playList,index,1,str);
    ListView_SetItemText(playList,index,2,node->afID);
    ListView_SetItemText(playList,index,3,node->rfName);
    ListView_SetItemText(playList,index,4,node->rfID);
    ListView_SetItemText(playList,index,5,node->rfDataString);
    wsprintf(str,"%lu",node->fsStart);
    ListView_SetItemText(playList,index,6,str);
    wsprintf(str,"%lu",node->fsLength);
    ListView_SetItemText(playList,index,7,str);
	Selection(playlistWnd,ListView_GetSelectedCount(playList));
    UpdateWindow(playList);
}

void AddNodeToListView(AFile *node)
{
	if (playList==NULL)
		return;

    InsertNodeIntoListView(ListView_GetItemCount(playList),node);
}

void UpdatePlaylistNodeTime(AFile* node)
{
	int  index;
	char str[20];

	if ((playList==NULL) || (node==NULL))
		return;

	index=GetListViewIndex(node);
	if (index!=-1)
	{
		if (node->afTime==-1)
			lstrcpy(str,"N/A");
		else
			GetShortTimeString((node->afTime)/1000,str);
		ListView_SetItemText(playList,index,1,str);
	}
}
