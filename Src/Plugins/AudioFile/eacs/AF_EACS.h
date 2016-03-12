#ifndef _AF_EACS_H
#define _AF_EACS_H

#include <windows.h>

#define IDSTR_SCHl "SCHl"
#define IDSTR_SCCl "SCCl"
#define IDSTR_SCDl "SCDl"
#define IDSTR_SCLl "SCLl"
#define IDSTR_SCEl "SCEl"
#define IDSTR_PT   "PT\0\0"
#define IDSTR_PFDx "PFDx"

#define SC_EA_ADPCM (0x07)
#define SC_NONE     (0)

#define SWAPDWORD(x) ((((x)&0xFF)<<24)+(((x)>>24)&0xFF)+(((x)>>8)&0xFF00)+(((x)<<8)&0xFF0000))

#pragma pack(1)

typedef struct tagASFBlockHeader
{
	char  id[4];
	DWORD size;
} ASFBlockHeader;

typedef struct tagMAPHeader
{
	char id[4];
	BYTE unknown1;
	BYTE firstSection;
	BYTE numSections;
	BYTE recordSize;
	BYTE unknown2[3];
	BYTE numRecords;
} MAPHeader;

typedef struct tagMAPSectionDefRecord
{
	BYTE unknown;
	BYTE magic;
	BYTE section;
} MAPSectionDefRecord;

typedef struct tagMAPSectionDef
{
	BYTE index;
	BYTE numRecords;
	BYTE id[2];
	MAPSectionDefRecord records[8];
} MAPSectionDef;

#pragma pack()

#endif
