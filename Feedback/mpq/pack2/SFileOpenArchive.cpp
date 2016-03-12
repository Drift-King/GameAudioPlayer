/*****************************************************************************/
/* SFileOpenArchive.cpp                      Copyright Project Software 1999 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*                       Archive functions of Storm.dll                      */
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
//  [150316D4] - Buffer for something, size 0x3134 bytes
//  [150316D8] - Previous opened file
//  [150316DC] - DWORD * stormBuffer
//  [150316E0] - Something 
//  [15034B28] - Critical section

/*****************************************************************************/
/* Local variables                                                           */
/*****************************************************************************/

DWORD * stormBuffer = NULL;             // Decryption engine buffer

/*****************************************************************************/
/* Local functions                                                           */
/*****************************************************************************/

static void prepareStormBuffer(DWORD * buffer)
{
    DWORD seed   = 0x00100001;
    DWORD index1 = 0;
    DWORD index2 = 0;
    int   i;

    for(index1 = 0; index1 < 0x100; index1++)
    {
        for(index2 = index1, i = 0; i < 5; i++, index2 += 0x100)
        {
            DWORD temp1, temp2;

            seed  = (seed * 125 + 3) % 0x2AAAAB;
            temp1 = (seed & 0xFFFF) << 0x10;

            seed  = (seed * 125 + 3) % 0x2AAAAB;
            temp2 = (seed & 0xFFFF);

            buffer[index2] = (temp1 | temp2);
        }
    }
}

static void decryptHashTable(DWORD * table, DWORD * stormBuffer, BYTE * key, DWORD length)
{
    DWORD seed1 = 0x7FED7FED;
    DWORD seed2 = 0xEEEEEEEE;
    int   ch;                           // One key character

    // Prepare seeds
    while(*key != 0)
    {
        ch = toupper(*key++);

        seed1 = stormBuffer[0x300 + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }

    // Decrypt it
    seed2 = 0xEEEEEEEE;
    while(length-- > 0)
    {
        seed2 += stormBuffer[0x400 + (seed1 & 0xFF)]; // 0x2866A8A1
        ch     = *table ^ (seed1 + seed2);          // 0xFFFFFFFF, 0xFFFFFFFF

        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;
        *table++ = ch;
    }
}

static void decryptBlockTable(DWORD * table, DWORD * stormBuffer, BYTE * key, DWORD length)
{
    DWORD seed1 = 0x7FED7FED;
    DWORD seed2 = 0xEEEEEEEE;
    int   ch;                           // One key character

    // Prepare seeds
    while(*key != 0)
    {
        ch = toupper(*key++);

        seed1 = stormBuffer[0x300 + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }

    // Decrypt it
    seed2 = 0xEEEEEEEE;
    while(length-- > 0)
    {
        seed2 += stormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = *table ^ (seed1 + seed2);

        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;
        *table++ = ch;
    }
}

/*****************************************************************************/
/* Public functions                                                          */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// SFileOpenArchive
//
//   fileName - MPQ archive file name to open
//   flags    - ???
//   hArchive - Pointer to store open archive handle

BOOL WINAPI SFileOpenArchive(char * fileName, int flags, int, HANDLE * hArchive)
{
    TMPQArchive * ha;                   // Archive handle
    TMPQHash    * hash;                 // For hash processing loops
    TMPQBlock   * block;                // For block processing loops
    HANDLE        hFile;                // Opened archive file handle
    DWORD         read;                 // Number of bytes read
    DWORD         i;

    if(hArchive != NULL)
        *hArchive = NULL;
    
    // Check the right parameters
    if(fileName == NULL || *fileName == 0 || hArchive == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Check is storm buffer allocated, otherwise return FALSE
    if(stormBuffer == NULL)
    {
        // Cannot allocate - exit
        if((stormBuffer = new DWORD[STORM_BUFFER_SIZE]) == NULL)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
        // Prepare encryption buffer
        prepareStormBuffer(stormBuffer);
    }

    // Open MPQ archive file
    if((hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
        return FALSE;

    // Allocate handle
    if((ha = new TMPQArchive) == NULL)
    {
        CloseHandle(hFile);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    // Initialize handle structure
    memset(ha, 0, sizeof(TMPQArchive));
    strncpy(ha->fileName, fileName, strlen(fileName));
    ha->hFile       = hFile;
    ha->fromCD      = FALSE;
    ha->flags       = flags;

    // Allocate structure for MPQ header
    if((ha->header = new TMPQHeader) == NULL)
    {
        SFileCloseArchive(ha);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Find MPQ header offset
    do
    {
        SetFilePointer(hFile, ha->mpqPos, NULL, FILE_BEGIN);
        ReadFile(hFile, ha->header, sizeof(TMPQHeader), &read, NULL);

        if(read != sizeof(TMPQHeader))
        {
            SFileCloseArchive(ha);
            SetLastError(ERROR_BAD_FORMAT);
            return FALSE;
        }
        // If header not found, add 0x200 to filepos
        if(ha->header->id != ID_MPQ)
            ha->mpqPos += 0x200;
    }
    while(ha->header->id != ID_MPQ);

    // If no MPQ signature found, open as normal (PLAIN) resource file
    if(ha->header->id != ID_MPQ)
    {
        ha->mpqPos = 0xFFFFFFFF;
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        ReadFile(hFile, ha->header, sizeof(TMPQHeader), &read, NULL);

        *hArchive = ha;
        return TRUE;
    }

    // Relocate hash table position
    ha->header->hashTablePos  += ha->mpqPos;
    ha->header->blockTablePos += ha->mpqPos;

    // Allocate buffer
    ha->blockSize = (0x200 << ha->header->blockSize);
    if((ha->blockBuffer = new BYTE[ha->blockSize]) == NULL)
    {
        SFileCloseArchive(ha);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Allocate space for hash table
    if((ha->hash = new TMPQHash[ha->header->hashTableSize]) == NULL)
    {
        SFileCloseArchive(ha);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Read hash table
    SetFilePointer(hFile, ha->header->hashTablePos, NULL, FILE_BEGIN);
    ReadFile(hFile, ha->hash, ha->header->hashTableSize * 16, &read, NULL);
    if(read != ha->header->hashTableSize * 16)
    {
        SFileCloseArchive(ha);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Decrypt hash table
    decryptHashTable((DWORD *)ha->hash, stormBuffer, (BYTE *)"(hash table)", (ha->header->hashTableSize * 4));

    // Check hash table if is correctly decrypted
    for(i = 0, hash = ha->hash; i < ha->header->hashTableSize; i++, hash++)
    {
        if((hash->control & 0xFFFF0000) != 0x00000000 && hash->control != 0xFFFFFFFF)
        {
            SFileCloseArchive(ha);
            SetLastError(ERROR_BAD_FORMAT);
            return FALSE;
        }
    }

    // Allocate space for block table
    if((ha->block = new TMPQBlock[ha->header->nFiles]) == NULL)
    {
        SFileCloseArchive(ha);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Read block table
    SetFilePointer(hFile, ha->header->blockTablePos, NULL, FILE_BEGIN);
    ReadFile(hFile, ha->block, ha->header->nFiles * 16, &read, NULL);

    // Decrypt block table.
    // Some MPQs don't have decrypted block table, e.g. cracked Diablo version
    // We have to check if block table is already decrypted
    if(ha->header->dataOffs != ha->block->filePos)
        decryptBlockTable((DWORD *)ha->block, stormBuffer, (BYTE *)"(block table)", (ha->header->nFiles * 4));

    // Check if block table is correctly decrypted
    DWORD archiveSize = ha->header->archiveSize + ha->mpqPos;
    for(i = 0, block = ha->block; i < ha->header->nFiles; i++, block++)
    {
        // Relocate block table
        if(block->filePos != 0)
            block->filePos += ha->mpqPos;
        
        if(block->filePos > archiveSize || block->fsize > archiveSize)
        {
            SFileCloseArchive(ha);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }        
    *hArchive = ha;
    return TRUE;
}

//-----------------------------------------------------------------------------
// BOOL SFileCloseArchive(HANDLE hArchive);
//

BOOL WINAPI SFileCloseArchive(HANDLE hArchive)
{
    TMPQArchive * ha;
    
    if((ha = (TMPQArchive *)hArchive) == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(ha->block != NULL)
        delete ha->block;
    if(ha->hash != NULL)
        delete ha->hash;
    if(ha->blockBuffer != NULL)
        delete ha->blockBuffer;
    if(ha->header != NULL)
        delete ha->header;

    CloseHandle(ha->hFile);
    delete ha;
    return TRUE;
}

