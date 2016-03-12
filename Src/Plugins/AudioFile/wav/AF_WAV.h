#ifndef _AF_WAV_H
#define _AF_WAV_H

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

typedef struct tagFMTChunk
{
    WORD  tag;
    WORD  channels;
    DWORD rate;
    DWORD bps;
    WORD  align;
    WORD  bits;
} FMTChunk;

#pragma pack()

#endif
