/*
 * Game Audio Player source code: playlist nodes
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

#include "..\Plugins\AudioFile\AFPlugin.h"

#include "Misc.h"
#include "ListView.h"
#include "Progress.h"
#include "PlayDialog.h"
#include "Playlist.h"
#include "Player.h"
#include "Plugins.h"
#include "Errors.h"
#include "Playlist_NodeOps.h"
#include "resource.h"

AFile *list_start,*cursel,*list_end,*curNode;
int    list_size;
BOOL   list_changed,opNoTime;
char   list_filename[MAX_PATH];

void Clear(HWND hwnd)
{
    int    i;
	AFile *node,*tmpnode;
    char   str[MAX_PATH+100];
	HWND   pwnd;

    Stop();
    if (list_size==0)
		return;
    pwnd=OpenProgress(hwnd,PDT_SINGLE,"Playlist clearing");
	EnableWindow(GetDlgItem(pwnd,ID_CANCELALL),FALSE);
    wsprintf(str,"Clearing %s",GetFileTitleEx(list_filename));
    ShowProgressHeaderMsg(str);
    i=0;
    ShowProgress(i,list_size);
	node=list_start;
    while ((node!=NULL) && (i<list_size)) // ???
    {
		ListView_DeleteItem(playList,0);
		tmpnode=node->next;
		GlobalFree(node);
		node=tmpnode;
		i++;
		wsprintf(str,"Playlist nodes remained: %d",list_size-i);
		ShowProgressStateMsg(str);
		ShowProgress(i,list_size);
    }
    CloseProgress();
	list_start=NULL;
	list_end=NULL;
	list_size=0;
	list_changed=FALSE;
	lstrcpy(list_filename,"");
    cursel=NULL;
	curNode=NULL;
	Selection(playlistWnd,ListView_GetSelectedCount(playList));
	UpdateWholeWindow(playList);
    ShowStats();
}

void DeleteNode(int index)
{
    AFile *node;

	node=GetListViewNode(index);
    if (node==NULL)
		return;

    if (node->prev==NULL)
		list_start=node->next;
    else
		node->prev->next=node->next;
    if (node->next==NULL)
		list_end=node->prev;
    else
		node->next->prev=node->prev;
    list_size--;
    if (curNode==node)
    {
		if (node->next!=NULL)
			curNode=node->next;
		else
			curNode=node->prev;
    }
    GlobalFree(node);
    ListView_DeleteItem(playList,index);
    if (ListView_GetItemCount(playList)!=index)
		SetCurrentSelection(index);
    else
		SetCurrentSelection(index-1);
    ShowStats();
    if (list_size!=0)
		list_changed=TRUE;
    else
    {
		list_changed=FALSE;
		lstrcpy(list_filename,"");
    }
	Selection(playlistWnd,ListView_GetSelectedCount(playList));
    UpdateWindow(playList);
}

void AddNode(AFile *node)
{
    if (node==NULL)
		return;

    if (list_start==NULL)
    {
		list_start=node;
		list_end=node;
		curNode=node;
		node->prev=NULL;
		node->next=NULL;
    }
    else
    {
		list_end->next=node;
		node->prev=list_end;
		list_end=node;
		node->next=NULL;
    }
    list_size++;
    node->afPlugin=GetAFPlugin(node->afID);
    node->rfPlugin=GetRFPlugin(node->rfID);
	node->afTime=-1;
	if (!opNoTime)
	{
	    if (node->afPlugin!=NULL)
	    {
			if (AFPLUGIN(node)->GetTime!=NULL)
		    node->afTime=AFPLUGIN(node)->GetTime(node);
	    }
	}
    AddNodeToListView(node);
    if (GetCurrentSelection()==NO_SELECTION)
		SetCurrentSelection(0);
    ShowStats();
    list_changed=TRUE;
}

void SwapNodes(int index)
{
    AFile *node1,*node2;

	node1=GetListViewNode(index);
	node2=GetListViewNode(index+1);
    if ((node1==NULL) || (node2==NULL))
		return;

    if (node1->prev==NULL)
		list_start=node2;
    else
		node1->prev->next=node2;
    if (node2->next==NULL)
		list_end=node1;
    else
		node2->next->prev=node1;
    node1->next=node2->next;
    node2->prev=node1->prev;
    node1->prev=node2;
    node2->next=node1;
    list_changed=TRUE;
}

void MoveNodeUp(int index)
{
    AFile* node;

    if (index==0)
		return;
	node=GetListViewNode(index);
    if (node==NULL)
		return;

    SwapNodes(index-1);
    ListView_DeleteItem(playList,index);
    InsertNodeIntoListView(index-1,node);
    SetCurrentSelection(index-1);
    ShowStats();
    UpdateWindow(playList);
}

void MoveNodeDown(int index)
{
    AFile* node;

	if (playList==NULL)
		return;
    if (ListView_GetItemCount(playList)-1==index)
		return;
    node=GetListViewNode(index);
    if (node==NULL)
		return;

    SwapNodes(index);
    ListView_DeleteItem(playList,index);
    InsertNodeIntoListView(index+1,node);
    SetCurrentSelection(index+1);
    ShowStats();
    UpdateWindow(playList);
}

AFile* Duplicate(AFile *node)
{
    AFile* newnode;

    if (node==NULL)
		return NULL;

    newnode=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
	CopyMemory(newnode,node,sizeof(AFile));
    newnode->prev=node->prev;
    newnode->next=node;
    if (newnode->prev==NULL)
		list_start=newnode;
    else
		newnode->prev->next=newnode;
    node->prev=newnode;
    if (curNode==node)
		curNode=newnode;
    list_size++;
    list_changed=TRUE;

    return newnode;
}

void DuplicateNode(int index)
{
    AFile *node;

	node=GetListViewNode(index);
    if (node==NULL)
		return;

    InsertNodeIntoListView(index,Duplicate(node));
    SetCurrentSelection(index);
    ShowStats();
    UpdateWindow(playList);
}

AFile* GetNodeByIndex(int index)
{
    AFile *node=list_start;
    int    i=0;

    while ((node!=NULL) && (i<index))
    {
		i++;
		node=node->next;
    }

    return node;
}

int GetNodePos(AFile* node)
{
    int    i=0;
    AFile* curnode=list_start;

    while (curnode!=NULL)
    {
		if (curnode==node)
			return i;
		curnode=curnode->next;
		i++;
    }

    return -1;
}
