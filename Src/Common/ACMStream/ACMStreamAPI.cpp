/*
 * ACM decompression engine source code: DLL API stuff
 *
 * Copyright (C) 2000 ANX Software
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

#include "ACMStreamUnpack.h"

#define ID_ACM (0x01032897)

#pragma pack(1)

typedef struct tagACMHeader
{
	DWORD magic;
	DWORD size;
	WORD  channels;
	WORD  rate;
	WORD  attrs;
} ACMHeader;

#pragma pack()

extern "C" __declspec(dllexport) BOOL __stdcall ACMStreamGetInfo
(
	DWORD  readFunc,
	DWORD  fileHandle,
	DWORD *rate,
	WORD  *channels,
	WORD  *bits,
	DWORD *size
)
{
	ACMHeader acmhdr;
	DWORD     read;

	((FileReadFunction)readFunc)(fileHandle,&acmhdr,sizeof(ACMHeader),&read);
	if ((read<sizeof(ACMHeader)) || (acmhdr.magic!=ID_ACM))
		return FALSE;
    *rate=acmhdr.rate;
	*size=2*acmhdr.size;
	*channels=acmhdr.channels;
	*bits=16; // ???

	return TRUE;
}

extern "C" __declspec(dllexport) DWORD __stdcall ACMStreamInit
(
	DWORD readFunc,
	DWORD fileHandle
)
{
	CACMUnpacker *acm;
	
	if ((!readFunc) || (!fileHandle))
		return 0L;

	acm = new CACMUnpacker ((FileReadFunction)readFunc, (unsigned long)fileHandle);

	return (DWORD)acm;
}

extern "C" __declspec(dllexport) void __stdcall ACMStreamShutdown
(
	DWORD acm
)
{
	delete ((CACMUnpacker*)acm);
}

extern "C" __declspec(dllexport) DWORD __stdcall ACMStreamReadAndDecompress
(
	DWORD  acm,
	char  *buff,
	DWORD  count
)
{
	DWORD ret=0L,read;
	CACMUnpacker *pacm=(CACMUnpacker*)acm;

	if (!acm)
		return 0L;

	while ((pacm->canRead()) && (count)) {
		read = pacm->readAndDecompress ((unsigned short*)buff, count);
		ret += read;
		buff += read;
		count -= read;
	}

	return ret;
}
