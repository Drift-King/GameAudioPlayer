/*
 * fDAT Plug-in source code: FileList-related functions
 *
 * Copyright (C) 2000 Alexander Belyakov
 * E-mail: abel@krasu.ru
 *
 * Fallout 2 DAT header parsing is based on f2re utility by Borg Locutus
 *  (dystopia@iname.com).
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

// DataString ::= <Tag>/<FileName>
// where fileName is the name found in DAT-archive,
//     FileName ::= [path1\..\pathN\]name  in f2Dat
// and FileName ::= name  in f1Dat.
// Tag ::= <hdrPos>:<ver>, hdrPos is the hexadecimal integer specifying
//   position of file description in DAT archive, and ver is version of
//   DAT archive.

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

#include "FileList.h"
#include "resource.h"

#include "..\RFPlugin.h"
#include "..\..\..\Common\TreeSel/TreeSel.h"


// Tests if a file name is valid (contains only printable symbols).
int testName (unsigned char* name) {
	if (!name) return 0;
	while ( *name ) {
		if (*name < 0x20 || *name > 0x7E)
			return 0;
		name++;
	}
	return 1;
}

struct namePair {
	char* name;
	namePair* sub;
	int value;
};

// binary search
int findName (const char* str, namePair* index, int count) {
	if (!str)
		return -1;
	int b = 0, e = count, m, r;
	int ob, oe;
	int i = -1;
	while (i == -1) {
		m = (b + e)/2;
		ob = b; oe = e;
		r = strcmpi (str, (const char*)index[m]. name);
		// do not try to use lstrcmpi, it works differently
		if (r < 0)
			e = m - 1;
		else if (r > 0)
			b = m + 1;
		else
			i = m;
		if ( b < ob || b > oe || e < ob || e > oe)
			break;
	}
	return i;
}

void killFileIndex (namePair* &files, int fileCnt) {
	if (!files) return;
	for (int i=0; i<fileCnt; i++)
		free (files[i]. name);
	delete (files);
	files = NULL;
}

void killDirIndex (namePair* &dirs, int dirCnt) {
	if (!dirs) return;
	for (int i=0; i<dirCnt; i++) {
		free (dirs[i]. name);
		killFileIndex (dirs[i]. sub, dirs[i]. value);
	}
	delete (dirs);
	dirs = NULL;
}


// Attribute fields in f1Dat archives are in reversed form.
long readRevDD (FILE* file) {
	char res[4], s;
	fread (&res, 1, 4, file);
	s = res[0]; res[0] = res[3]; res[3] = s;
	s = res[1]; res[1] = res[2]; res[2] = s;
	return *(long*)&res;
}


// Parser of Fallout1 DAT header.
// It is hard to determine if a file is f1Dat archive. But I'll try to do it.
RFile* GetFileList_f1Dat_oldOne (const char* resname) {
	RFile* result = NULL;
	FILE* file = fopen (resname, "rb");
	if (file != NULL) {
		fseek (file, 0, SEEK_END);
		long archSize = ftell (file);
		rewind (file);
		long dirCount = readRevDD (file),
			unknown2 = readRevDD (file),
			unknown3 = readRevDD (file),
			unknown4 = readRevDD (file);
		// test for f1Dat structure, not guaranteed to work well
		// 1. 2nd RevDWord is GE than dirCount, and 3rd RevDWord is zero (I hope, always)
		if (dirCount <= unknown2 && !unknown3) {
			HDIRSEL dirSel = CreateDirSelector (NULL);

			namePair* dirList = new namePair [dirCount];
			memset (dirList, 0, dirCount*sizeof(namePair));
			int res = 0, i;
			for (i = 0; i < dirCount && !res; i++) {
				unsigned char nameLen = fgetc (file);
				dirList[i]. name = (char*)calloc (nameLen+1, 1);
				fread (dirList[i]. name, 1, nameLen, file);
				// 2. dirName must contain only good symbols
				if ( !testName ((unsigned char*)dirList[i]. name) )
					res = -1;
				AddDirectory (dirSel, dirList[i]. name);
			}
			for (i = 0; i < dirCount && !res; i++) {
				long fileCount = readRevDD (file);
				unknown2 = readRevDD (file);
				unknown3 = readRevDD (file);
				unknown4 = readRevDD (file);
				// 3. 2nd RevDWord is GE than filesCount, and 3rd RevDWord is 0x10 (I hope, always)
				if (fileCount > unknown2 || unknown3 != 0x10)
					res = -1;
				dirList[i]. sub = new namePair [fileCount];
				dirList[i]. value = fileCount;
				memset (dirList[i]. sub, 0, fileCount*sizeof(namePair));
				for (int j = 0; j < fileCount && !res; j++) {
					dirList[i]. sub[j]. value = ftell (file);
					unsigned char nameLen = fgetc (file);
					dirList[i]. sub[j]. name = (char*)calloc (nameLen+1, 1);
					fread (dirList[i]. sub[j]. name, 1, nameLen, file);
					// 4. fileName must contain only good symbols
					if ( !testName ((unsigned char*)dirList[i]. sub[j]. name) ) {
						res = -1;
						break;
					}
					long attr = readRevDD (file),
						pos = readRevDD (file),
						fileSize = readRevDD (file),
						packedSize = readRevDD (file);
					// 5. attr can be only 0x10, 0x20 and 0x40
					if ( attr != 0x10 && attr != 0x20 && attr != 0x40) {
						res = -1;
						break;
					}
					// 6. if attr=0x20 then packedSize is zero (I think)
					if (attr == 0x20 && packedSize) {
						res = -1;
						break;
					}
					// 7. fileEnd must be within archive
					int fileEnd = pos + ((attr==0x20)? fileSize: packedSize);
					if (fileEnd > archSize) {
						res = -1;
						break;
					}
					AddFile (dirSel, (const char*)dirList[i]. sub[j]. name, fileSize, i);
				}
			}

			if (res == 0)
				if (ShowDialog (dirSel) == IDOK) {
					result = (RFile*)CreateNameList (dirSel, INCLUDE_SELECTED);
					// place tag prefixes
					RFile* item = result;
					while (item) {
						char *path = item->rfDataString,
							*name = strrchr (path, '\\');
						if (name) {
							*name = '\0';
							name ++;
						} else
							name = NULL;
						int j = -1;
						i = findName (path, dirList, dirCount);
						if (i != -1) {
							j = findName (name, dirList[i]. sub, dirList[i]. value);
						};

						if (j != -1) {
							char temp[256] = "";
							long tag = dirList[i]. sub[j]. value;
							wsprintf (temp, "%08lX:1/%s", tag, name);
							lstrcpy (item->rfDataString, temp);
						}
						item = item->next;
					}
				}

			DestroyDirSelector (dirSel);
			killDirIndex (dirList, dirCount);
		}
		fclose (file);
	}
	return result;
}


RFile* GetFileList_f1Dat_newOne (const char* resname) {
// New version.
// Old one does not work well with MSVCRT40. I do not where exactly the catch is,
// but it appears to be, that free functions become very slow, when there is a
// lot of allocated blocks in the heap. Need to group them into a few large pieces.
	RFile* result = NULL;
	FILE* file = fopen (resname, "rb");
	if (file != NULL) {
		fseek (file, 0, SEEK_END);
		long archSize = ftell (file);
		rewind (file);
		long dirCount = readRevDD (file),
			unknown2 = readRevDD (file),
			unknown3 = readRevDD (file),
			unknown4 = readRevDD (file);
		// test for f1Dat structure, not guaranteed to work well
		// 1. 2nd RevDWord is GE than dirCount, and 3rd RevDWord is zero (I think)
		if (dirCount <= unknown2 && !unknown3) {
			HDIRSEL dirSel = CreateDirSelector (NULL);

//			namePair* dirList = new namePair [dirCount];
//			memset (dirList, 0, dirCount*sizeof(namePair));
			namePair* dirList = (namePair*)calloc (dirCount, sizeof(namePair));
			int res = 0, i;
			for (i = 0; i < dirCount && !res; i++) {
				unsigned char nameLen = fgetc (file);
				dirList[i]. name = (char*)calloc (nameLen+1, 1);
				fread (dirList[i]. name, 1, nameLen, file);
				// 2. dirName must contain only good symbols
				if ( !testName ((unsigned char*)dirList[i]. name) )
					res = -1;
				AddDirectory (dirSel, dirList[i]. name);
			}
			// pointers to fileNames of every directory
			char** dirFiles = (char**)calloc (dirCount, sizeof(void*)); // filled with NULL 
			for (i = 0; i < dirCount && !res; i++) {
				long fileCount = readRevDD (file);
				unknown2 = readRevDD (file);
				unknown3 = readRevDD (file);
				unknown4 = readRevDD (file);
				// 3. 2nd RevDWord is GE than filesCount, and 3rd RevDWord is 0x10 (I think)
				if (fileCount > unknown2 || unknown3 != 0x10)
					res = -1;
//				dirList[i]. sub = new namePair [fileCount];
//				memset (dirList[i]. sub, 0, fileCount*sizeof(namePair));
				dirList[i]. sub = (namePair*)calloc  (fileCount, sizeof(namePair));
				dirList[i]. value = fileCount;
				// making place for fileNames, they are short (8.3), so if we alloc 13 bytes, it will be enough (I hope)
				char* namePtr = dirFiles[i] = (char*)calloc (fileCount, 13);
				// namePtr is used only for small speed optimization
				for (int j = 0; j < fileCount && !res; j++) {
					namePair &itemAlias = dirList[i]. sub[j];
					itemAlias. value = ftell (file);
					unsigned char nameLen = fgetc (file);
					itemAlias. name = namePtr;
					fread (namePtr, 1, nameLen, file);
					// 4. fileName must contain only good symbols
					if ( !testName ((unsigned char*)namePtr) ) {
						res = -1;
						break;
					}
					long attr = readRevDD (file),
						pos = readRevDD (file),
						fileSize = readRevDD (file),
						packedSize = readRevDD (file);
					// 5. attr can be only 0x10, 0x20 and 0x40
					if ( attr != 0x10 && attr != 0x20 && attr != 0x40) {
						res = -1;
						break;
					}
					// 6. if attr=0x20 then packedSize is zero (I think)
					if (attr == 0x20 && packedSize) {
						res = -1;
						break;
					}
					// 7. fileEnd must be within archive
					int fileEnd = pos + ((attr==0x20)? fileSize: packedSize);
					if (fileEnd > archSize) {
						res = -1;
						break;
					}
					AddFile (dirSel, (const char*)namePtr, fileSize, i);
					namePtr += 13; // 8.3 and '\0'
				}
			}

			if (res == 0)
				if (ShowDialog (dirSel) == IDOK) {
					result = (RFile*)CreateNameList (dirSel, INCLUDE_SELECTED);
					// place tag prefixes
					RFile* item = result;
					while (item) {
						char *path = item->rfDataString,
							*name = strrchr (path, '\\');
						if (name) {
							*name = '\0';
							name ++;
						} else
							name = NULL;
						int j = -1;
						i = findName (path, dirList, dirCount);
						if (i != -1) {
							j = findName (name, dirList[i]. sub, dirList[i]. value);
						};

						if (j != -1) {
							char temp[256] = "";
							long tag = dirList[i]. sub[j]. value;
							wsprintf (temp, "%08lX:1/%s", tag, name);
							lstrcpy (item->rfDataString, temp);
						}
						item = item->next;
					}
				}

			DestroyDirSelector (dirSel);
//			killDirIndex (dirList, dirCount);
			if (dirList) {
				for (i=0; i<dirCount; i++) {
					free (dirList[i]. name);
					free (dirList[i]. sub);
				}
			}
			free (dirList);
			if (dirFiles) {
				for (i=0; i<dirCount; i++)
					free (dirFiles[i]);
				free (dirFiles);
			}
		}
		fclose (file);
	}
	return result;
}


// Parser of Fallout2 DAT header.
RFile* GetFileList_f2Dat (const char* resname) {
	int file;
	long fsize, hsize; // stored file- and header- sizes
	unsigned char *header, *ptr; // pointer to memoty copy of header and temp variable
	long hdrPos, real_hsize; // position of Header in DAT archive;  real size of header (actual bytes read from DAT)
	int nameLen; // name length of particular file
	char isPacked; // packed? flag
	long fileSize, packedSize, pos; // attributes of file
	int res = 0; // temp variable used to break this function if the file is not a Fallout2 DAT

	int fileCount, // count of files in archive
		realFCount = 0; // real count of files (how many files found in header)
	namePair* nameIndex; // index array of names in archive

	file = open (resname, _O_BINARY | _O_RDONLY);
	if (file == -1)
		return NULL;

	// test if given file has f2DAT structure
	// 1. last dword of file equals the filesize
	lseek (file, -8, SEEK_END);
	read (file, &hsize, 4);
	read (file, &fsize, 4);
	if (fsize != tell(file) || hsize > fsize) {
		close (file);
		return NULL;
	}
	ptr = header = new unsigned char [hsize];
	hdrPos = fsize-hsize-8; // since archive size = data size + hdrsize + 2*dword
	lseek (file, hdrPos, SEEK_SET);
	real_hsize = read (file, header, hsize);

	fileCount = *(long*)ptr; // first dword of header has count of files
	ptr += 4;
	nameIndex = new namePair [fileCount];

	HDIRSEL dirSel = CreateDirSelector (NULL);

	int i = 0;
	while (ptr < header+real_hsize) {
		nameLen = (unsigned long)*ptr;
		isPacked = (char)ptr[nameLen+4];
		ptr[nameLen+4] = '\0';
		// 2. fileName must contain only good symbols
		if ( !testName (ptr+4) ) {
			res = -1;
			break;
		}
		fileSize = *(long*)(ptr+nameLen+1+4);
		packedSize = *(long*)(ptr+nameLen+1+8);
		pos  = *(long*)(ptr+nameLen+1+12);
		// 3. pos+packedSize must be LE than hdrPos
		if ( pos+packedSize > hdrPos ) {
			res = -1;
			break;
		}
		// 4. if file is not packed, fileSize should be equal to packedSize
		if ( !isPacked && packedSize != fileSize ) {
			res = -1;
			break;
		}
		AddFile (dirSel, (const char*)ptr+4, fileSize, -1);
		nameIndex [i++]. name = (char*)ptr+4;
		ptr += (nameLen+1+16);
		realFCount++;
	}

	RFile* result = NULL;

	if (res == 0)
		if (ShowDialog (dirSel) == IDOK) {
			result = (RFile*)CreateNameList (dirSel, INCLUDE_SELECTED);
			// place tag prefixes
			RFile* item = result;
			while (item) {
				i = findName (item->rfDataString, nameIndex, realFCount); //fileCount);
				if (i != -1) {
					char temp[256] = "";
					long tag = nameIndex[i]. name - (char*)header - 4 + hdrPos;
					wsprintf (temp, "%08lX:2/%s", tag, item->rfDataString);
					lstrcpy (item->rfDataString, temp);
				}
				item = item->next;
			}
		}

	DestroyDirSelector (dirSel);
	if (nameIndex) delete (nameIndex);
	if (header) delete (header);
	close (file);

	return result;
}


RFile* __stdcall PluginGetFileList (const char* resname) {
	RFile* result = GetFileList_f2Dat (resname);
	if (!result)
		result = GetFileList_f1Dat_newOne (resname);
	if (!result)
		SetLastError (PRIVEC (IDS_NOTFDAT));
	return result;
}

BOOL __stdcall PluginFreeFileList (RFile* list) {
	DestroyNameList ((NLItem*)list);
	return TRUE;
}