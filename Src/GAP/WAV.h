#ifndef _GAP_WAV_H
#define _GAP_WAV_H

#include <windows.h>

#define IDSTR_RIFF "RIFF"
#define IDSTR_WAVE "WAVE"
#define IDSTR_fmt  "fmt "
#define IDSTR_data "data"
#define IDSTR_fact "fact"

#pragma pack(1)

typedef struct tagRIFFHeader
{
    char  riffid[4];
    DWORD riffsize;
    char  rifftype[4];
} RIFFHeader;

typedef struct tagChunkHeader
{
    char  id[4];
    DWORD size;
} ChunkHeader;

#pragma pack()

#endif
