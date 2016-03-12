/*****************************************************************************/
/* SFileReadFile.cpp                         Copyright Project Software 1999 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*                     File I/O functions of Storm.dll                       */
/*****************************************************************************/

#include <windows.h>

#include "Storm.h"
#include "Pkware.h"

// Decryption engine buffer (In SFileOpenArchive)
extern DWORD * stormBuffer;             

//-----------------------------------------------------------------------------
// DecryptMPQBlock
//

static void DecryptMPQBlock(DWORD * block, DWORD length, DWORD seed1)
{
    DWORD seed2 = 0xEEEEEEEE;
    DWORD ch;

    // Round to DWORDs
    length >>= 2;

    while(length-- > 0)
    {
        seed2 += stormBuffer[(seed1 & 0xFF) + 0x400];
        ch     = *block ^ (seed1 + seed2);

        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;
        *block++ = ch;
    }
}

//-----------------------------------------------------------------------------
// ReadMPQBlock
//
//  buffer    - Pointer to target buffer to store blocks.
//  blockSize - Number of bytes to read. Must be multiplier of block size.
//  hf        - MPQ File handle.
//  blockPos  - Position of block in the file.
//
//  Returns number of bytes read.

static DWORD WINAPI ReadMPQBlocks(TMPQFile * hf, DWORD blockPos, BYTE * buffer, DWORD blockSize)
{
    TMPQArchive * ha = hf->hArchive;    // Archive handle
    BYTE  * inputBuffer;                // Buffer for reading compressed data
    DWORD   readPos;                    // Reading position from the file
    DWORD   toRead;                     // Number of bytes to read
    DWORD   blockNum;                   // Block number (needed for decrypt)
    DWORD   bytesRead = 0;              // Total number of bytes read
    DWORD   nBlocks;                    // Number of blocks to load
    DWORD   i;

    // Test parameters. Block position and block size must be block-aligned, block size nonzero
    if((blockPos & (ha->blockSize - 1)) || blockSize == 0)
        return 0;

    // Check the end of file
    if((blockPos + blockSize) > hf->block->fsize)
        blockSize = hf->block->fsize - blockPos;

    blockNum = blockPos  / ha->blockSize;
    nBlocks  = blockSize / ha->blockSize;
    if(blockSize % ha->blockSize)
        nBlocks++;

    // If file has variable block positions, we have to load them
    if((hf->block->flags & MPQ_FILE_COMPRESSED) && hf->blockPosLoaded == FALSE)
    {
        DWORD nRead;

        if(hf->block->filePos != ha->filePos)
            SetFilePointer(ha->hFile, hf->block->filePos, NULL, FILE_BEGIN);

        // Read block positions from begin of file.
        ReadFile(ha->hFile, hf->blockPos, (hf->nBlocks+1) * sizeof(DWORD), &nRead, NULL);

        // Decrypt loaded block positions if necessary
        if(hf->block->flags & MPQ_FILE_ENCRYPTED)
            DecryptMPQBlock(hf->blockPos, nRead, hf->seed1 - 1);

        // Update hArchive's variables
        hf->blockPosLoaded = TRUE;
        ha->filePos        = hf->block->filePos + nRead;
    }

    // Get file position and number of bytes to read
    readPos = blockPos;
    toRead  = blockSize;
    if(hf->block->flags & MPQ_FILE_COMPRESSED)
    {
        readPos = hf->blockPos[blockNum];
        toRead  = hf->blockPos[blockNum + nBlocks] - readPos;
    }
    readPos += hf->block->filePos;

    // Get work buffer for store read data
    inputBuffer = buffer;
    if(hf->block->flags & MPQ_FILE_COMPRESSED)
    {
        if((inputBuffer = new BYTE[toRead]) == NULL)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return 0;
        }
    }

    // Set file pointer, if necessary 
    if(ha->filePos != readPos)
        ha->filePos = SetFilePointer(ha->hFile, readPos, NULL, FILE_BEGIN);

    // Read all requested blocks
    ReadFile(ha->hFile, inputBuffer, toRead, &bytesRead, NULL);
    ha->filePos = readPos + bytesRead;

    // Block processing part.
    DWORD blockStart  = 0;              // Index of block start in work buffer
    DWORD blockLength = min(blockSize, ha->blockSize);
    DWORD index       = blockNum;       // Current block index

    bytesRead = 0;                      // Clear read byte counter

    // Walk through all blocks
    for(i = 0; i < nBlocks; i++, index++)
    {
        DWORD outLength = 0x1000;

        // Get current block length
        if(hf->block->flags & MPQ_FILE_COMPRESSED)
            blockLength = hf->blockPos[index+1] - hf->blockPos[index];

        // If block is encrypted, we have to decrypt it.
        if(hf->block->flags & MPQ_FILE_ENCRYPTED)
            DecryptMPQBlock((DWORD *)(inputBuffer + blockStart), blockLength, hf->seed1 + index);

        // If block is compressed, can have two types of compression
        if(blockLength < blockSize)
        {
            if(hf->block->flags & MPQ_FILE_COMPRESS1)
                Decompression4((BYTE *)buffer, &outLength, inputBuffer + blockStart, blockLength);

            if(hf->block->flags & MPQ_FILE_COMPRESS2)
                Decompress((BYTE *)buffer, &outLength, inputBuffer + blockStart, blockLength);

            bytesRead += outLength;
        }
        else
        {
            if(buffer != inputBuffer)
                memcpy(buffer, inputBuffer, blockLength);

            bytesRead += blockLength;
        }
        buffer     += outLength;
        blockStart += blockLength;
    }

    // Delete input buffer, if necessary
    if(hf->block->flags & MPQ_FILE_COMPRESSED)
        delete inputBuffer;

    return bytesRead;
}

//-----------------------------------------------------------------------------
// ReadMPQFile

static DWORD WINAPI ReadMPQFile(TMPQFile * hf, DWORD filePos, BYTE * buffer, DWORD toRead)
{
    TMPQArchive * ha    = hf->hArchive; 
    TMPQBlock   * block = hf->block;    // Pointer to file block
    DWORD         bytesRead = 0;        // Number of bytes read from the file
    DWORD         blockPos;             // Position in the file aligned to the whole blocks

    // File position is greater or equal to file size ?
    if(filePos >= block->fsize)
        return bytesRead;

    // If too much bytes in the file remaining, cut them
    if((block->fsize - filePos) < toRead)
        toRead = (block->fsize - filePos);

    // Block position in the file
    blockPos = filePos & ~(ha->blockSize - 1);  // Position in the block

    // Load the first block, if noncomplete. It may be loaded in the cache buffer.
    // We have to check if this block is loaded. If not, load it.
    if((filePos % ha->blockSize) != 0)
    {
        // Number of bytes remaining in the buffer
        DWORD toCopy;
        DWORD temp = ha->blockSize;

        // Check if data are loaded in the cache
        if(hf != ha->lastFile || blockPos != ha->blockPos)
        {
            // Load one MPQ block into archive buffer
            temp = ReadMPQBlocks(hf, blockPos, ha->blockBuffer, ha->blockSize);

            // Save lastly accessed file and block position for later use
            ha->lastFile = hf;
            ha->blockPos = blockPos;
            ha->buffPos  = 0;
        }
        toCopy = temp - ha->buffPos;
        if(toCopy > toRead)
            toCopy = toRead;

        // Copy data from block buffer into target buffer
        memcpy(buffer, ha->blockBuffer + ha->buffPos, toCopy);
    
        // Update pointers
        toRead    -= toCopy;
        bytesRead += toCopy;
        buffer    += toCopy;
        blockPos  += ha->blockSize;
        ha->buffPos += toCopy;

        // If all, return.
        if(toRead == 0)
            return bytesRead;
    }

    // Load the whole ("middle") blocks only if there are more or equal one block
    if(toRead > ha->blockSize && toRead > ha->blockSize)
    {
        DWORD blockSize = toRead & ~(ha->blockSize - 1);
        DWORD nBlocks   = blockSize / ha->blockSize;
        DWORD temp      = ReadMPQBlocks(hf, blockPos, buffer, blockSize);

        // Update pointers
        toRead    -= temp;
        bytesRead += temp;
        buffer    += temp;
        blockPos  += temp;

        // If all, return.
        if(toRead == 0)
            return bytesRead;
    }

    // Load the terminating block
    if(toRead > 0)
    {
        DWORD temp = ha->blockSize;

        // Check if data are loaded in the cache
        if(hf != ha->lastFile || blockPos != ha->blockPos)
        {
            // Load one MPQ block into archive buffer
            temp = ReadMPQBlocks(hf, blockPos, ha->blockBuffer, ha->blockSize);

            // Save lastly accessed file and block position for later use
            ha->lastFile = hf;
            ha->blockPos = blockPos;
            ha->buffPos  = 0;
        }

        // Check number of bytes read
        if(temp > toRead)
            temp = toRead;

        memcpy(buffer, ha->blockBuffer, temp);
        bytesRead  += temp;
        ha->buffPos = temp;
    }
    
    // Return what we've read
    return bytesRead;
}

//-----------------------------------------------------------------------------
// SFileReadFile

BOOL WINAPI SFileReadFile(HANDLE hFile, BYTE * buffer, DWORD toRead, DWORD * read, int)
{
    TMPQFile    * hf = (TMPQFile *)hFile;
    TMPQArchive * ha = hf->hArchive;
    DWORD         bytes;                // Number of bytes (for everything)

    // Init number of bytes read to zero.
    if(read != NULL)
        *read = 0;

    // Check valid parameters
    if(hf == NULL || buffer == NULL || toRead == 0)
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return FALSE;
    }

    // If direct access to the file, use Win32 for reading
    if(hf->hFile != INVALID_HANDLE_VALUE)
    {
        ReadFile(hf->hFile, buffer, toRead, &bytes, NULL);
        if(read != NULL)
            *read = bytes;
        
        return(bytes == toRead) ? TRUE : FALSE;
    }

    // Read all the bytes available in the buffer (If any)
    bytes = ReadMPQFile(hf, hf->filePos, buffer, toRead);

    hf->filePos += bytes;
    if(read != NULL)
        *read += bytes;

    // Check number of bytes read. If not OK, return FALSE.
    if(bytes < toRead)
    {
        SetLastError(ERROR_HANDLE_EOF);
        return FALSE;
    }
    return TRUE;
}

//-----------------------------------------------------------------------------
// SFileGetFileSize
//

DWORD WINAPI SFileGetFileSize(HANDLE hFile, DWORD * fileSizeHigh)
{
    TMPQFile * hf = (TMPQFile *)hFile;
    
    if(fileSizeHigh != NULL)
        *fileSizeHigh = 0;

    if(hf == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    // If opened as plain file, ...
    if(hf->hFile != INVALID_HANDLE_VALUE)
        return GetFileSize(hf->hFile, fileSizeHigh);

    // If opened from archive, return file size
    return hf->block->fsize;
}

DWORD WINAPI SFileSetFilePointer(HANDLE hFile, LONG filePos, LONG * filePosHigh, int method)
{
    TMPQArchive * ha;
    TMPQFile * hf = (TMPQFile *)hFile;

    if(hf == NULL || (filePosHigh != NULL && *filePosHigh != 0))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    ha = hf->hArchive;
    if(hf->hFile != INVALID_HANDLE_VALUE)
        return SetFilePointer(hf->hFile, filePos, filePosHigh, method);

    switch(method)
    {
        case FILE_BEGIN:
            hf->filePos = filePos;
            break;

        case FILE_CURRENT:
            // Cannot set pointer before begin of file
            if(-filePos > (LONG)hf->filePos)
                hf->filePos = 0;
            else
                hf->filePos += filePos;
            break;

        case FILE_END:
            // Cannot set file position before begin of file
            if((DWORD)(-filePos) >= hf->block->fsize)
                hf->filePos = 0;
            else
                hf->filePos = hf->block->fsize + filePos;
            break;

        default:
            return ERROR_INVALID_PARAMETER;
    }

//  hf->buffPos   = 0;
//  hf->buffBytes = 0;
    if(hf == ha->lastFile && (hf->filePos & ~(ha->blockSize - 1)) == ha->blockPos)
        ha->buffPos = hf->filePos & (ha->blockSize - 1);
    else
        ha->lastFile = (TMPQFile *)-1;

    return hf->filePos;
}

