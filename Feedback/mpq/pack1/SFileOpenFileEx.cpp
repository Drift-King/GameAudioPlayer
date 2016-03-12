/*****************************************************************************/
/* SFileOpenFileEx.cpp                       Copyright Project Software 1999 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*                         File functions of Storm.dll                       */
/*****************************************************************************/

#include <string.h>
#include <windows.h>

#include "Storm.h"

//-----------------------------------------------------------------------------
// Globals

//  [1500BB20] - Thread function for decompress
//  [150102B0] - SMemAllocMemory
//  [15010690] - SMemFreeMemory
//  [15020170] - strchr
//  [15020920] - toupper
//  [15020DD0] - strncpy
//  [1502D598] - TCHAR * XXX = '\\'
//  [150315B0] - char moduleName[MAX_PATH];
//  [150315B0] - Stored archive handle
//  [150316BC] - hEvent - Stored hEvent for synchronize with decompressing thread
//  [150316C0] - TWorkStruct * prev
//  [150316C8] - Something
//  [150316D8] - Previous opened file
//  [150316DC] - DWORD * stormBuffer
//  [150316E0] - Something 
//  [15034B28] - Critical section

/*****************************************************************************/
/* Local variables                                                           */
/*****************************************************************************/

extern DWORD * stormBuffer;             // Decryption engine buffer (In SFileOpenArchive)

/*****************************************************************************/
/* Local functions                                                           */
/*****************************************************************************/

static DWORD decryptHashIndex(char * fileName, DWORD * stormBuffer)
{
    BYTE * key   = (BYTE *)fileName;
    DWORD  seed1 = 0x7FED7FED;
    DWORD  seed2 = 0xEEEEEEEE;
    int    ch;

    while(*key != 0)
    {
        ch = toupper(*key++);

        seed1 = stormBuffer[ch + 0x000] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}

static DWORD decryptName1(char * fileName, DWORD * stormBuffer)
{
    BYTE * key   = (BYTE *)fileName;
    DWORD  seed1 = 0x7FED7FED;
    DWORD  seed2 = 0xEEEEEEEE;
    int    ch;

    while(*key != 0)
    {
        ch = toupper(*key++);

        seed1 = stormBuffer[ch + 0x100] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}

static DWORD decryptName2(char * fileName, DWORD * stormBuffer)
{
    BYTE * key   = (BYTE *)fileName;
    DWORD  seed1 = 0x7FED7FED;
    DWORD  seed2 = 0xEEEEEEEE;
    int    ch;

    while(*key != 0)
    {
        ch = toupper(*key++);

        seed1 = stormBuffer[ch + 0x200] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}

static DWORD decryptFileSeed(char * fileName, DWORD * stormBuffer)
{
    BYTE * key   = (BYTE *)fileName;
    DWORD  seed1 = 0x7FED7FED;          // EBX
    DWORD  seed2 = 0xEEEEEEEE;          // ESI
    int    ch;

    while(*key != 0)
    {
        ch = toupper(*key++);           // ECX

        seed1 = stormBuffer[ch + 0x300] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}

// Decrypts file name and finds for block index.
// If fileName is given by number (less than 0x10000),
// file is searched by its block index, not by name
static DWORD getBlockIndex(TMPQArchive * ha, char * fileName)
{
    TMPQHash * hash;                    // File hash entry
    DWORD index = (DWORD)fileName;      // Hash index
    DWORD name1;
    DWORD name2;

    // Try if filename given by index
    if(index <= ha->header->nFiles)
        return index;

    // Decrypt name and block index
    index = decryptHashIndex(fileName, stormBuffer) & (ha->header->hashTableSize - 1);
    name1 = decryptName1(fileName, stormBuffer);
    name2 = decryptName2(fileName, stormBuffer);
    
    // Look for hash index
    for(hash = ha->hash + index; index < ha->header->hashTableSize; index++, hash++)
    {
        // If invalid hash, stop search
        if(hash->blockIndex == 0xFFFFFFFF)
            return -1;
        
        if(hash->name1 == name1 && hash->name2 == name2 && hash->blockIndex != 0xFFFFFFFE)
            return hash->blockIndex;
    }
    return -1;
}

/*****************************************************************************/
/* Public functions                                                          */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// SFileOpenFileEx
//
//   fileName - MPQ archive file name to open
//   flags    - ???
//   hArchive - Pointer to store open archive handle

BOOL WINAPI SFileOpenFileEx(HANDLE hArchive, char * fileName, int something, HANDLE * hFile)
{
    TMPQArchive * ha = (TMPQArchive *)hArchive;
    TMPQFile    * hf;
    TMPQBlock   * block;                // File block
    DWORD         blockIndex;           // Found table index

    if(hFile != NULL)
        *hFile = NULL;

    if(hArchive == NULL || fileName == NULL || *fileName == 0 || hFile == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // If opened file is simple PLAIN file
    if(ha->header->id != ID_MPQ)
    {
        // Allocate file handle
        if((hf = new TMPQFile) == NULL)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }

        // Initialize file handle
        memset(hf, 0, sizeof(TMPQFile));
        hf->hFile    = ha->hFile;
        hf->hArchive = ha;

        *hFile = hf;
    }

    // Get block index from file name
    blockIndex = getBlockIndex(ha, fileName);

    // If index was not found, or is greater than number of files, exit.
    if(blockIndex == -1 || blockIndex > ha->header->nFiles)
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    // Get block and test it
    block = ha->block + blockIndex;
    if(block->fsize == 0 || (block->flags & 0x80000000) == 0)
    {
        SetLastError(ERROR_BAD_FORMAT);
        return FALSE;
    }

    // Skip ':' in the file name
    while(strchr(fileName, '\\') != NULL)
        fileName = strchr(fileName, '\\') + 1;

    // Allocate file handle (ESI = hFile);
    // EBP - block
    if((hf = new TMPQFile) == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Initialize file handle
    memset(hf, 0, sizeof(TMPQFile));
    hf->hFile    = INVALID_HANDLE_VALUE;
    hf->hArchive = ha;
    hf->block    = block;
    hf->nBlocks  = (hf->block->fsize + ha->blockSize - 1) / ha->blockSize;
    hf->seed1    = decryptFileSeed(fileName, stormBuffer);

    if(hf->block->flags & MPQ_FILE_FIXSEED)
        hf->seed1 = (hf->seed1 + hf->block->filePos - ha->mpqPos) ^ hf->block->fsize;

    // Allocate buffers for decompression.
    if(hf->block->flags & MPQ_FILE_COMPRESSED)
    {
        // Buffer for decompressed data. Always must be 0x1000 bytes length,
        // because this is length of decompressed block required by PKWARE data compression library.
//      hf->buffer = new BYTE[0x1000];
        
        // Allocate buffer for block positions. At the begin of file are stored
        // DWORDs holding positions of each block relative from begin of file in the archive
        if((hf->blockPos = new DWORD[hf->nBlocks + 1]) == NULL)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }
    *hFile = hf;
    return TRUE;
}

//-----------------------------------------------------------------------------
// BOOL SFileCloseFile(HANDLE hFile);
//

BOOL WINAPI SFileCloseFile(HANDLE hFile)
{
    TMPQFile * hf;
    
    if((hf = (TMPQFile *)hFile) == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Close file, if present
    if(hf->hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);

    // Set ??? in hArchive
    if(hf->hArchive != NULL)
        hf->hArchive->lastFile = NULL;

    // Delete buffers
    if(hf->blockPos != NULL)
        delete hf->blockPos;
//  if(hf->buffer != NULL)
//      delete hf->buffer;
    
    // Delete file handle
    delete hf;
    
    // OK, all good
    return TRUE;
}
