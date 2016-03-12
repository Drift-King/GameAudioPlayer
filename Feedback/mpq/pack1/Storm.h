/*****************************************************************************/
/* Storm.h                                   Copyright Project Software 1999 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*                    Storm.dll file functions (header file)                 */
/*****************************************************************************/

#ifndef __STORM_H_
#define __STORM_H_

//-----------------------------------------------------------------------------
// Defines

#define ID_MPQ              0x1A51504D  // MPQ archive header ID ("MPQ\x1A")
#define STORM_BUFFER_SIZE   0x500       // Size of decryption engine buffer (in DWORDs)


// Constants for TBlock::flags
#define MPQ_FILE_COMPRESS1  0x00000100  // The first type of compression
#define MPQ_FILE_COMPRESS2  0x00000200  // The second type of compression
#define MPQ_FILE_COMPRESSED 0x0000FF00  // File is compressed by any type of compression
#define MPQ_FILE_ENCRYPTED  0x00010000  // Indicates whether file is encrypted 
#define MPQ_FILE_FIXSEED    0x00020000  // File decrypt seed has to be fixed
#define MPQ_FILE_MUSTBESET  0x80000000  // Must be set - tested at file open

//-----------------------------------------------------------------------------
// Structures

// Hash entry. All files in the archive are searched by their hashes.
typedef struct
{
    DWORD name1;                        // 00 : The first two DWORDs  
    DWORD name2;                        // 04 : are the encrypted file name
    DWORD control;                      // 08 : Control DWORD
    DWORD blockIndex;                   // 0C : Index to file description block
} TMPQHash;

// File description block contains informations about the file
typedef struct
{
    DWORD filePos;                      // 00 : Block file starting position in the archive
    DWORD csize;                        // 04 : Compressed file size
    DWORD fsize;                        // 08 : Uncompressed file size
    DWORD flags;                        // 0C : Flags
} TMPQBlock;

// MPQ file header
typedef struct
{
    DWORD id;                           // 00 : The 0x1A51504D ('MPQ\x1A') signature
    DWORD dataOffs;                     // 04 : Offset of the first file (Relative to MPQ start)
    DWORD archiveSize;                  // 08 : Size of MPQ archive
    WORD  offs0C;                       // 0C : 0000 for SC and BW
    WORD  blockSize;                    // 0E : Size of file block is (0x200 << blockSize)
    DWORD hashTablePos;                 // 10 : File position of hashTable
    DWORD blockTablePos;                // 14 : File position of blockTable. Each entry has 16 bytes
    DWORD hashTableSize;                // 18 : Size of hashTable (in 16-byte paragraphs)
    DWORD nFiles;                       // 1C : Number of files/blocks in archive (Each file has one block)
} TMPQHeader;

// Archive handle structure used by Diablo 1.00
struct TMPQFile;

struct TMPQArchive
{
    char          fileName[MAX_PATH];   // 000 Opened archive file name
    HANDLE        hFile;                // 104 File handle
    BOOL          fromCD;               // 108 TRUE is open from CD
    DWORD         flags;                // 10C (Flags)
    TMPQFile    * lastFile;             // 110 Lastly accessed file handle
    DWORD         blockPos;             // 114 Position of loaded block in the file
    DWORD         blockSize;            // 118 Size of file block
    BYTE        * blockBuffer;          // 11C Buffer (cache) for file block
    DWORD         buffPos;              // 120 Position in block buffer
    DWORD         mpqPos;               // 124 MPQ archive position in the file
    TMPQBlock   * block;                // 128 Block table
    TMPQHeader  * header;               // 12C MPQ file header
    TMPQHash    * hash;                 // 130 Hash table
    DWORD         filePos;              // 134 Current file pointer
    TMPQArchive * prev;                 // 138 Pointer to previously opened archive
};

// File handle structure used by Diablo 1.00 (0x38 bytes)
struct TMPQFile
{
    HANDLE       hFile;                 // 00 : File handle
    TMPQArchive* hArchive;              // 04 : Archive handle
    TMPQBlock  * block;                 // 08 : File block pointer
    DWORD        seed1;                 // 0C : Seed used for file decrypt
    DWORD        filePos;               // 10 : Current file position
    DWORD        offs14;                // 14 : 
    DWORD        nBlocks;               // 18 : Number of blocks in the file (incl. the last noncomplete one)
    DWORD      * blockPos;              // 1C : Position of each file block (only for compressed files)
    BOOL         blockPosLoaded;        // 20 : TRUE if block positions loaded
    DWORD        offs24;                // 24 : (Number of bytes somewhere ?)
//  BYTE       * buffer;                // 28 : Decompress buffer (Size is always 0x1000 bytes)
//  DWORD        buffPos;               // 2C : Position in decompress buffer (compressed file only)
//  DWORD        buffBytes;             // 30 : Number of bytes in decompress buffer
    TMPQFile   * prev;                  // 34 : Previous file handle pointer
};

//-----------------------------------------------------------------------------
// Functions

BOOL  WINAPI SFileOpenArchive(char * archiveName, int flags, int, HANDLE * hArchive);
BOOL  WINAPI SFileCloseArchive(HANDLE hArchive);

BOOL  WINAPI SFileOpenFileEx(HANDLE hArchive, char * fileName, int, HANDLE * hFile);
BOOL  WINAPI SFileCloseFile(HANDLE hFile);

DWORD WINAPI SFileGetFileSize(HANDLE hFile, DWORD * fileSizeHigh = NULL);
DWORD WINAPI SFileSetFilePointer(HANDLE hFile, LONG filePos, LONG * filePosHigh, int method);
BOOL  WINAPI SFileReadFile(HANDLE hFile, BYTE * buffer, DWORD toRead, DWORD * read = NULL, int = 0);

#endif  // __STORM_H_
