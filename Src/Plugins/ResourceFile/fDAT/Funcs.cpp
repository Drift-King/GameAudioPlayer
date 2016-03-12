/*
 * fDAT Plug-in source code: file-related functions
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

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <limits.h>

#include "Funcs.h"
#include "CFile.h"

#include "..\RFPlugin.h"


extern RFPlugin Plugin;

long readRevDD (int file) {
	char res[4], s;
	_read (file, &res, 4);
	s = res[0]; res[0] = res[3]; res[3] = s;
	s = res[1]; res[1] = res[2]; res[2] = s;
	return *(long*)&res;
}

RFHandle* __stdcall PluginOpenFile (const char* resname, LPCSTR rfDataString) {
	char rfDStr[256];
	lstrcpy (rfDStr, rfDataString);

	char *tag = rfDStr,
		*name = strchr (rfDStr, '/'),
		*ver = strchr (rfDStr, ':');
	if (name) {
		*name = '\0';
		name++;
	} else
		return NULL;
	if (ver) {
		*ver = '\0';
		ver++;
	} else
		return NULL;
	unsigned long hdrPos = strtoul (tag, NULL, 16);
	if ( hdrPos == 0 || hdrPos == ULONG_MAX )
		return NULL;
	if (*ver != '1' && *ver != '2')
		return NULL;

	int file = open (resname, _O_BINARY | _O_RDONLY);
	if (file == -1)
		return NULL;

	lseek (file, hdrPos, SEEK_SET);

	CFile* handler = NULL;
	char* fileName;
	int nameEQ;
	long fileSize, packedSize, offset;

	if (*ver == '1') {
		unsigned char nameLen; _read (file, &nameLen, 1);
		fileName = (char*) calloc (nameLen+1, 1);
		_read (file, fileName, nameLen);
		nameEQ = (strcmpi (fileName, name) == 0);
		free (fileName);
		if (nameEQ) {
			long attr = readRevDD (file);
			offset = readRevDD (file);
			fileSize = readRevDD (file);
			packedSize = readRevDD (file);

			if (attr == 0x20 && !packedSize)
				handler = new CPlainFile (file, offset, fileSize);
			else if (attr == 0x40)
				handler = new C_LZ_BlockFile (file, offset, fileSize, packedSize);
			// attr 0x10 is not suppoted
		}
	} else {
		long nameLen; _read (file, &nameLen, 4);
		if (nameLen < 256) {
			fileName = (char*) calloc (nameLen+1, 1);
			_read (file, fileName, nameLen);
			nameEQ = (strcmpi (fileName, name) == 0);
			free (fileName);
			if (nameEQ) {
				unsigned char flag; _read (file, &flag, 1);
				_read (file, &fileSize, 4);
				_read (file, &packedSize, 4);
				_read (file, &offset, 4);
				if (!flag && packedSize == fileSize)
					handler = new CPlainFile (file, offset, fileSize);
				else if (flag == 1)
					handler = new C_Z_PackedFile (file, offset, fileSize, packedSize);
			}
		}
	}

	RFHandle* result = NULL;

	if (handler) {
		result = new RFHandle;
		result->rfPlugin = &Plugin;
		result->rfHandleData = (void*)handler;
	}

	if (!result)
		close (file);
	return result;
}

BOOL  __stdcall PluginCloseFile (RFHandle* rf) {
	if (!rf)
		return FALSE;
	CFile* handler = (CFile*) rf->rfHandleData;
	if (handler)
		delete (handler);
	delete (rf);
	return TRUE;
}

DWORD __stdcall PluginGetFileSize (RFHandle* rf) {
	if (!rf)
		return 0;
	CFile* handler = (CFile*) rf->rfHandleData;
	if (handler)
		return handler->getSize();
	else
		return 0;
}

DWORD __stdcall PluginGetFilePointer (RFHandle* rf) {
	if (!rf)
		return 0;
	CFile* handler = (CFile*) rf->rfHandleData;
	if (handler)
		return handler->tell();
	else
		return 0;
}

BOOL  __stdcall PluginEndOfFile (RFHandle* rf) {
	if (!rf)
		return TRUE;
	CFile* handler = (CFile*) rf->rfHandleData;
	if (handler)
		return handler->eof();
	else
		return TRUE;
}

DWORD __stdcall PluginSetFilePointer (RFHandle* rf, LONG lDistanceToMove,DWORD dwMoveMethod) {
	if (!rf)
		return 0xFFFFFFFF;
	CFile* handler = (CFile*) rf->rfHandleData;
	if (handler)
		return handler->seek (lDistanceToMove, dwMoveMethod);
	else
		return 0xFFFFFFFF;
}

BOOL  __stdcall PluginReadFile (RFHandle* rf, LPVOID lpBuffer,DWORD toRead,LPDWORD read) {
	if (!rf)
		return FALSE;
	CFile* handler = (CFile*) rf->rfHandleData;
	if (handler)
		return handler->read (lpBuffer, toRead, (long*)read);
	else
		return FALSE;
}