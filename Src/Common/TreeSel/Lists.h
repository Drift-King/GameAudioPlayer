#ifndef _TREESEL_LISTS_H
#define _TREESEL_LISTS_H

#include <string.h>

#include "General.h"
#include "TreeSel.h"


// generic List consts
#define REBUILD_INDEX 1
#define NO_REBUILD_INDEX 0
#define DELETE_ITEM 1
#define NO_DELETE_ITEM 0

// File and Dir Items ownership constants
#define NOT_OWN_FILE 0
#define OWN_FILE 1

class CList;

class CListItem {
friend class CList;
protected:
	CListItem *prev, *next,
		*parent;
	char* name;
	long value;
public:
	CListItem():
		prev(NULL), next(NULL), parent(NULL),
		name(NULL), value(0) {};
	CListItem (const char* str, long val = 0):
		prev(NULL), next(NULL), parent(NULL),
		name (strdup(str)), value(val) {};
	virtual ~CListItem() {if (name)
		free (name);};
	virtual const char* asString() {return name;};
	virtual long getValue() {return value;};
	virtual void setValue (long val) {if (val != value) value = val;};
	void setParent (CListItem* par) {if (par!=parent) parent = par;};
	CListItem* getParent() {return parent;};
};

class CList {
protected:
	CListItem *head, *tail, *iterator;
	long count;
	CListItem** index;
	void setCount(long newCnt) {if (newCnt != count) {
		reindex(NO_REBUILD_INDEX);
		count = newCnt;} };
public:
	CList():head(NULL),
		tail(NULL),
		count(0),
		index(NULL) {};
	virtual ~CList() {clear();};

	long getCount() {return count;};
	void clear (int deleteItem = DELETE_ITEM);
	void addToHead (CListItem* item, CListItem* parent = NULL);
	void addToTail (CListItem* item, CListItem* parent = NULL);
	void beginEnum() {iterator = head;};
	CListItem* getNextItem ();
	CListItem* getAt (long num);
	void removeNode (CListItem* node, int deleteItem = DELETE_ITEM);
	CListItem* operator[] (long num) {return getAt(num);};
	void reindex(int buildNew);
	CListItem* findName (const char* name, long& ind);
	CListItem* findName (const char* name);
};

typedef void (__stdcall* ItemNotification) (CListItem* item, int sender, long param);

class CFileItem: public CListItem {
protected:
	long size;
	int selected;
public:
	CFileItem():
		size(FILE_SIZE_UNKNOWN),
		selected(0) {};
	CFileItem (const char* str, long fSize = FILE_SIZE_UNKNOWN):
	CListItem (str),
		size (fSize),
		selected (0) {};
	long getSize() {return size;};
	int getSelected() {return selected;};
	void setSelected (int sel, ItemNotification onChange = NULL, long param = 0);
};

class CDirItem: public CListItem {
protected:
	CList *files, *subdirs;
	long selCount, selSize;
	long totCount, totSize;
	long curSize, curSelCount, curSelSize;
	char* fullName;
public:
	CDirItem():
		files(NULL), subdirs(NULL),
		selCount(0), selSize(0),
		totCount(0), totSize(0),
		curSize(0), curSelCount(0), curSelSize(0),
		fullName(NULL) {};
	CDirItem (const char* str):
		CListItem (str),
		files (NULL), subdirs (NULL),
		selCount (0), selSize (0),
		totCount (0), totSize (0),
		curSize(0), curSelCount(0), curSelSize(0),
		fullName(NULL) {};
	virtual ~CDirItem() {if (files) delete (files);
		if (subdirs) delete (subdirs);
		if (fullName) free (fullName);};
	long getSelSize() {return selSize;};
	long getSelCount() {return selCount;};
	int getSelType();	// 0 - none selected
						// 1 - everything
						// 2 - partial selection
	long getTotalSize() {return totSize;};
	long getTotalCount() {return totCount;};
	long getCurCount() { return (files)?files->getCount():0; };
	long getCurSize() {return curSize;};
	long getCurSelSize() {return curSelSize;};
	long getCurSelCount() {return curSelCount;};
	void addSelected (long cnt, long size, int own = NOT_OWN_FILE);
	void addTotal (long cnt, long size, int own = NOT_OWN_FILE);
	CFileItem* addFile (const char* name, long size = FILE_SIZE_UNKNOWN);
	CFileItem* addFile (CFileItem* item);
	void invertSel (ItemNotification onChange = NULL, long param = 0);
	void selectAll (int sel, int recursive = RECURSIVE,
		ItemNotification onChange = NULL, long param = 0,
		RegexpTester match = NULL, const char* regexp = NULL, int testpath = 1);
	void buildIndexes();
	CList* getFiles() {return files;};
	CList* getSubdirs() {return subdirs;};
	CDirItem* addSubDir (const char* path);
	CDirItem* addSubDir (CDirItem* item);
	const char* getFullName () {return fullName;};
	void setParentName (const char* parName, int addSelf = 1);
};

class CSimpleList {
protected:
	NLItem *first, *last;
public:
	CSimpleList():
		first (NULL), last(NULL) {};
	NLItem* getHead() {return first;};
	void addToTail (const char* str);
};

#endif