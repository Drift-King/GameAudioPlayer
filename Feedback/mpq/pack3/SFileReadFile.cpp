/*****************************************************************************/
/* SFileReadFile.cpp                         Copyright Project Software 1999 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*                     File I/O functions of Storm.dll                       */
/*****************************************************************************/

#include <stdio.h>
#include <windows.h>

#include "Storm.h"
#include "Pkware.h"

//-----------------------------------------------------------------------------
// Defines        

#define BLOCKTYPE_BLOCKPOS    1         // Block is DWORD array of block positions
#define BLOCKTYPE_WAVEHDR     2         // Block is header of WAVE file

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
        seed2 += stormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = *block ^ (seed1 + seed2);

        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;
        *block++ = ch;
    }
}

//-----------------------------------------------------------------------------
// Functions tries to get file decryption key. The trick comes from block
// positions which are stored at the begin of each compressed file. We know the
// File size, that means we know number of blocks that means we know the first
// DWORD value in block position. And if we know encrypted and decrypted value,
// we can find the decryption key !!!
//
// hf    - MPQ file handle
// block - DWORD array of block positions
// ch    - Decrypted value of the first block pos

static DWORD DetectFileSeed(TMPQFile * hf, DWORD * block, DWORD decrypted, DWORD blockType)
{
    DWORD temp = *block ^ decrypted;    // temp = seed1 + seed2
                                        // temp = seed1 + stormBuffer[0x400 + (seed1 & 0xFF)] + 0xEEEEEEEE
    temp -= 0xEEEEEEEE;                 // temp = seed1 + stormBuffer[0x400 + (seed1 & 0xFF)]
    
    for(int i = 0; i < 0x100; i++)      // Try all 255 possibilities
    {
        DWORD blockCopy[4];             // The first four DWORDS of block
        DWORD seed1;
        DWORD seed2 = 0xEEEEEEEE;
        DWORD ch;

        // Try the first DWORD (We exactly know the value)
        seed1  = temp - stormBuffer[0x400 + i];
        seed2 += stormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = block[0] ^ (seed1 + seed2);

        if(ch != decrypted)
            continue;

        // Now we have the predicted seed and we'll try to decrypt the first four DWORDs.
        memcpy(blockCopy, block, sizeof(blockCopy));
        DecryptMPQBlock(blockCopy, sizeof(blockCopy), seed1);

        if(blockType == BLOCKTYPE_BLOCKPOS && (blockCopy[1] & 0xFFFF0000) == 0)
            return seed1 + 1;

        if(blockType == BLOCKTYPE_WAVEHDR && blockCopy[2] == ID_WAVE2)
            return seed1;
    }
    return 0;
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
        {
            // If we don't know the file seed, try to find it.
            if(hf->seed1 == 0)
                hf->seed1 = DetectFileSeed(hf, hf->blockPos, (hf->nBlocks+1) * sizeof(DWORD), BLOCKTYPE_BLOCKPOS);

            // If we don't know the file seed, sorry but we cannot extract the file.
            if(hf->seed1 == 0)
                return 0;

            // Decrypt block positions
            DecryptMPQBlock(hf->blockPos, nRead, hf->seed1 - 1);
        }

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
        {
            // Try to decrypt file seed as it were a WAVE file
            if(hf->seed1 == 0)
                hf->seed1 = DetectFileSeed(hf, (DWORD *)inputBuffer, ID_WAVE, BLOCKTYPE_WAVEHDR);
                
            // If we don't know the seed, we cannot read file
            if(hf->seed1 == 0)
                return 0;

            DecryptMPQBlock((DWORD *)(inputBuffer + blockStart), blockLength, hf->seed1 + index);
        }

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

BOOL WINAPI SFileReadFile(HANDLE hFile, VOID * buffer, DWORD toRead, DWORD * read, int)
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
    bytes = ReadMPQFile(hf, hf->filePos, (BYTE *)buffer, toRead);

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

BOOL WINAPI SFileGetFileName(HANDLE hFile, CHAR * fileName)
{
    TMPQFile * hf = (TMPQFile *)hFile;  // MPQ File handle
    DWORD firstBytes;                   // The first 4 bytes of the file
    DWORD filePos;

    // Check valid parameters
    if(hf == NULL || fileName == NULL)
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return FALSE;
    }

    // If file name is not filled yet, try to detect it.
    if(*hf->fileName == 0)
    {
        char * ext;

        if(hf->fileIndex == -1)
            return FALSE;

        firstBytes = 0;

        filePos = SFileSetFilePointer(hf, 0, NULL, FILE_CURRENT);   
        SFileReadFile(hFile, &firstBytes, sizeof(firstBytes), NULL);
        SFileSetFilePointer(hf, filePos, NULL, FILE_BEGIN);

        switch(firstBytes)
        {
            case ID_WAVE:
                ext = "wav";
                break;

            case ID_SMK:
                ext = "smk";
                break;

            case ID_MPQ:
                ext = "mpq";
                break;

            case ID_PCX:
                ext = "pcx";
                break;

            default:
                ext = "xxx";
                break;
        }
        sprintf(hf->fileName, "File%04u.%s", hf->fileIndex, ext);
    }

    if((hf->block->flags & MPQ_FILE_ENCRYPTED) && hf->seed1 == 0)
        return FALSE;

    strcpy(fileName, hf->fileName);
    return TRUE;
}
