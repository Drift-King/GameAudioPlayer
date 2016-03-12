/*
 * Directory Tree Selector Library source code: list classes
 *
 * Copyright (C) 2000 Alexander Belyakov
 * E-mail: abel@krasu.ru
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

#include <string.h>
#include <stdlib.h>

#include "Lists.h"
#include "General.h"
#include "TreeSel.h"


void CList::addToHead (CListItem* item, CListItem* parent)
{
	if (!item) return;
	if (head) {
		item->prev = head->prev;
		head->prev = item;
	} else {
		item->prev = NULL;
		tail = item;
	}
	item->next = head;
	item->setParent (parent);
	setCount (getCount()+1);
	head = item;
}
void CList::addToTail (CListItem* item, CListItem* parent)
{
	if (!item) return;
	item->prev = tail;
	if (tail) {
		item->next = tail->next;
		tail->next = item;
	} else {
		item->next = NULL;
		head = item;
	}
	item->setParent (parent);
	setCount (getCount()+1);
	tail= item;
}
void CList::clear (int deleteItem) {
	CListItem *pos;
	while (head) {
		pos = head->next;
		if (head && deleteItem) delete (head);
		head = pos;
		setCount (getCount()-1);
	}
	tail = NULL;
}
CListItem* CList::getNextItem() {
	CListItem *res = iterator;
	if (iterator) iterator = iterator -> next;
	return res;
}
CListItem* CList::getAt (long num) {
	if (num >= getCount() || num < 0)
		return NULL;
	if (!index) {
		CListItem *pos;
		long i;
		for (i=0, pos=head; pos && i<num; i++, pos = pos->next);
		return pos;
	} else {
		return index[num];
	}
}
void CList::removeNode (CListItem* node, int deleteItem)
{
	if (node) {
		setCount (getCount()-1);
		if (!node->next)
			tail = node->prev;
		else
			(node->next)->prev = node->prev;
		if (!node->prev)
			head = node->next;
		else
			(node->prev)->next = node->next;
		if (deleteItem)
			delete (node);
	}
}
void CList::reindex (int buildNew) {
	if (!buildNew) {
		if (index) {
			delete (index);
			index = NULL;
		}
	} else if (!index) {
		index = new CListItem*[getCount()];
		CListItem *pos = head;
		long i = 0;
		while (pos) {
			index[i] = pos;
			pos = pos->next;
			i++;
		}
	}
}
CListItem* CList::findName (const char* name, long& ind) {
	CListItem* pos;
	for (pos=head, ind=0; pos && strcmpi(name, pos->asString()); pos = pos->next, ind++);
	return pos;
}
CListItem* CList::findName (const char* name) {
	long dummy;
	return findName (name, dummy);
}


void CFileItem::setSelected (int sel, ItemNotification onChange, long param) {
	if (sel != selected) {
		selected = sel;
		if (parent)
			((CDirItem*)parent)->addSelected (selected?1:-1, size, OWN_FILE);
		if (onChange)
			onChange (this, FILE_NOTIFICATION, param);
	}
}


void CDirItem::addSelected (long cnt, long size, int own) {
	selCount += cnt;
	if (size != FILE_SIZE_UNKNOWN) selSize += (cnt>0)?size:-size;
	if (own) {
		curSelCount += cnt;
		if (size != FILE_SIZE_UNKNOWN) curSelSize += (cnt>0)?size:-size;
	}
	if (parent)
	      	((CDirItem*)parent)->addSelected (cnt, size);
}
void CDirItem::addTotal (long cnt, long size, int own) {
	totCount += cnt;
	if (size != FILE_SIZE_UNKNOWN) {
		totSize += size;
		if (own) curSize += size;
	}
	if (parent)
	      	((CDirItem*)parent)->addTotal (cnt, size);
}
CFileItem* CDirItem::addFile (CFileItem* item) {
	if (!files)
		files = new CList;
	if (files) {
		files -> addToTail (item, this);
		addTotal (1, item->getSize(), OWN_FILE);
	}
	return item;
}
CFileItem* CDirItem::addFile (const char* name, long size) {
	return addFile (new CFileItem (name, size));
}
int CDirItem::getSelType() {
	if (selCount) {
		return ( (selCount==totCount)?ITEM_SELECT_ALL:ITEM_SELECT_PART );
	} else
		return ITEM_SELECT_NONE;
}
void CDirItem::selectAll (int sel, int recursive, ItemNotification onChange,
		long param, RegexpTester match, const char* regexp, int testpath) {
	long i;
	char tmpname[260];
	if (sel == getSelType()) return;
       	// if we have already [de]selected everything we need not do it again
	if (files) {
		files -> reindex (REBUILD_INDEX);
		for (i = 0; i<files->getCount(); i++) {
			CFileItem* file = (CFileItem*)(*files)[i];
			if ( file->getSelected() != sel ) {
			// select file only if it is not selected
				int res = 1;
				if (match && regexp) {
					*tmpname = '\0';
					if (recursive && testpath && getFullName()) strcat (tmpname, getFullName());
					if (file->asString()) strcat (tmpname, file->asString());
					res = match (tmpname, (char*)regexp);
				}
				if (res)
					file->setSelected (sel, onChange, param);
			}
		}
	}
	if (recursive && subdirs) {
		subdirs -> reindex (REBUILD_INDEX);
		for (i = 0; i<subdirs->getCount(); i++)
			((CDirItem*)(*subdirs)[i])->selectAll (sel, recursive, onChange, param, match, regexp, testpath);
	}
	if (onChange)
		onChange (this, DIR_NOTIFICATION, param);
}
void CDirItem::invertSel (ItemNotification onChange, long param) {
	if (!getSelType())
		selectAll (ITEM_SELECT_ALL, RECURSIVE, onChange, param);
	else
		selectAll (ITEM_SELECT_NONE, RECURSIVE, onChange, param);
}
void CDirItem::buildIndexes() {
	if (files) files -> reindex (REBUILD_INDEX);
	if (subdirs) {
		subdirs -> reindex (REBUILD_INDEX);
		for (long i = 0; i<subdirs->getCount(); i++)
			((CDirItem*)(*subdirs)[i]) -> buildIndexes();
	}
}
CDirItem* CDirItem::addSubDir (CDirItem* item) {
	if (!subdirs)
		subdirs = new CList;
	if (subdirs) {
		subdirs -> addToTail (item, this);
		addTotal (item->getTotalCount(), item->getTotalSize());
	}
	return item;
}
CDirItem* CDirItem::addSubDir (const char* path) {
	if (!subdirs)
		subdirs = new CList;
	CDirItem* item;
	char *pos, *path1 = strdup (path), *path2;
	pos = strchr (path1, '\\');
	if (pos) {
		path2 = pos + 1;
		*pos = '\0';
	}
	item = (CDirItem*)subdirs -> findName (path1);
	if (!item) {
		item = new CDirItem (path1);
		subdirs->addToTail (item, this);
	}
	if (pos)
		item = item -> addSubDir (path2);
	free (path1);
	return item;
}
void CDirItem::setParentName (const char* parName, int addSelf) {
	if (addSelf) {
		long len = 2;
		if (name) len += strlen (name);
		if (parName) len += strlen (parName);
		*(fullName = (char*)malloc (len)) = '\0';
		if (parName) strcat (fullName, parName);
		if (name) {
			strcat (fullName, name);
			*((short*)(fullName + len-2)) = '\\\0';
		}
	} else
		fullName = NULL;
	if (subdirs)
		for (long i=0; i<subdirs->getCount(); i++)
			((CDirItem*)(*subdirs)[i]) -> setParentName (fullName);
}


void CSimpleList::addToTail (const char* str) {
	NLItem* item = new NLItem;
	strcpy (item->str, str);
	item->next = NULL;
	if (first)
		last->next = item;
	else
		first = item;
	last = item;
}