/*****************************************************************************/
/* RF_Plain.cpp                         Copyright (C) 1998-1999 ANX Software */
/*               Game Audio Player PLAIN plugin source file                  */
/*****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <windows.h>

#include "..\rf.h"
#include "resource.h"

#define MEMORY_MAPPED_FILE          // Define if you want to use MMF

// Mapped block size (at least 2*allocGran !!!)
#define MAPPED_BLOCK_SIZE  (allocGran*16)    

extern RFPlugin plugin;

#ifdef MEMORY_MAPPED_FILE
typedef struct
{
    HANDLE hFile;                   // File handle
    HANDLE hMap;                    // Mapping handle
    char * file;                    // Mapped file content
    DWORD  startBlock;              // Start of mapped area
    DWORD  endBlock;                // End of mapped area
    DWORD  filePos;                 // Absolute file position
    DWORD  fileSize;                // Total file fileSize
} TFileInfo;

static DWORD allocGran = 0x10000;   // Allocation granularity (65536 bytes on Intel machines)

static DWORD getAllocationGranularity()
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return info.dwAllocationGranularity;
}    
#endif // MEMORY_MAPPED_FILE

static LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    if(umsg == WM_COMMAND && LOWORD(wparm) == IDOK)
        EndDialog(hwnd, LOWORD(wparm));

    return FALSE;
}

void __stdcall PluginAbout(HWND hwnd)
{
    DialogBox(plugin.hDllInst, "About", hwnd, AboutDlgProc);
}

RFile* __stdcall PluginGetFileList(const char * resname)
{
    RFile* list;

    // Allocale memory and fill with zeros
    list = (RFile *)AllocMemory(sizeof(RFile));
    return list;
}

BOOL __stdcall PluginFreeFileList(RFile* list)
{
    RFile * temp;

    while(list != NULL)
    {
		temp = list;
        list = list->next;
		FreeMemory(temp);
    }
    return TRUE;
}

//-----------------------------------------------------------------------------
// Version with memory-mapped file

#ifdef MEMORY_MAPPED_FILE

static int forward_seeks  = 0;
static int backward_seeks = 0;

RFHandle * __stdcall PluginOpenFile(const char * resName, LPCSTR rfDataString)
{
    RFHandle  * rf   = (RFHandle *) AllocMemory(sizeof(RFHandle));
    TFileInfo * info = (TFileInfo *)AllocMemory(sizeof(TFileInfo));
    DWORD       blockSize;

        // Check if allocation was OK
    if(info != NULL || rf != NULL)
    {
        // Open requested file. Assign no buffering because we have our own buffer management
        if((info->hFile = CreateFile(resName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL)) != INVALID_HANDLE_VALUE)
        {
            info->fileSize = GetFileSize(info->hFile, NULL);

            // Create file mapping
            if((info->hMap = CreateFileMapping(info->hFile, NULL, PAGE_READONLY, 0, 0, resName)) != NULL)
            {
                // Calculate mapped block fileSize
                blockSize = MAPPED_BLOCK_SIZE;
                if(info->fileSize < MAPPED_BLOCK_SIZE)
                    blockSize = info->fileSize;
                
                // Map view of file into memory
                if((info->file = (char *)MapViewOfFile(info->hMap, FILE_MAP_READ, 0, 0, blockSize)) != NULL)
                {
                    // Initial settings
                    info->startBlock = 0;
                    info->endBlock   = blockSize;
                    info->filePos    = 0;
                    
                    rf->rfPlugin     = &plugin;
                    rf->rfHandleData = info;

                    return rf;
                }
                CloseHandle(info->hMap);
            }
       		CloseHandle(info->hFile);
        }
    }
    FreeMemory(info);
    FreeMemory(rf);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
    TFileInfo * info = (TFileInfo *)rf->rfHandleData;

    return info->filePos;
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG filePos, DWORD method)
{
    TFileInfo * info = (TFileInfo *)rf->rfHandleData;
    DWORD multi  = MAPPED_BLOCK_SIZE;
    DWORD mask   = MAPPED_BLOCK_SIZE - 1;
    DWORD newPos = filePos;
    DWORD blockSize;

    // Get absolute file position
    if(method == FILE_CURRENT)
        newPos = info->filePos + filePos;
    if(method == FILE_END)
        newPos = info->fileSize + filePos;

    // Check if pointer is within mapped range
    if(info->startBlock <= newPos && newPos < info->endBlock)
    {
        info->filePos = newPos;
        return newPos;
    }

    // Check if not after end of file
    if(newPos > info->fileSize)
        return info->filePos;

    if(newPos <  info->startBlock)
        backward_seeks++;
    if(newPos >= info->endBlock)
        forward_seeks++;

    // Re-map new block into memory
    UnmapViewOfFile(info->file);
    
    // Calculate begin of mapped block and check end of file
    info->startBlock = (newPos & ~mask) - allocGran;
    if((LONG)info->startBlock < 0)
        info->startBlock = 0;

    info->endBlock   = info->startBlock + MAPPED_BLOCK_SIZE;
    info->filePos    = newPos;
    if((info->startBlock + MAPPED_BLOCK_SIZE) > info->fileSize)
        info->endBlock = info->fileSize;

    // Update mapped block
    blockSize  = info->endBlock - info->startBlock;
    info->file = (char *)MapViewOfFile(info->hMap, FILE_MAP_READ, 0, info->startBlock, blockSize);

    return newPos;
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
    TFileInfo * info = (TFileInfo *)rf->rfHandleData;

    return info->fileSize;
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
    TFileInfo * info = (TFileInfo *)rf->rfHandleData;

    return (BOOL)(info->filePos >= info->fileSize);
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD toRead, LPDWORD lpRead)
{
    TFileInfo * info = (TFileInfo *)rf->rfHandleData;
    char * buffer = (char *)lpBuffer;
    DWORD read = 0;
    BOOL  result = FALSE;

    // Loop until all was read
    while(toRead > 0)
    {
        // Number of bytes remaining in mapped block
        DWORD remains = min(info->endBlock, info->fileSize) - info->filePos;      
        DWORD temp;

        // If end of file found, break loop
        if(info->filePos >= info->fileSize)
            break;

        // Store number of bytes available
        temp = min(remains, toRead);

        // Read available block and update variables
        memcpy(buffer, info->file + (info->filePos - info->startBlock), temp);
        info->filePos += temp;
        buffer += temp;
        toRead -= temp;
        read   += temp;
        result = TRUE;

        // If we are at the end of block, map next block
        if(info->filePos == info->endBlock)
        {
            if(info->endBlock < info->fileSize)
            {
                DWORD blockSize;
                
                // Check end of file
                UnmapViewOfFile(info->file);
                info->startBlock += MAPPED_BLOCK_SIZE;
                info->endBlock   += MAPPED_BLOCK_SIZE;
                if((info->startBlock + MAPPED_BLOCK_SIZE) > info->fileSize)
                    info->endBlock = info->fileSize;

                // Map next file block
                blockSize  = info->endBlock - info->startBlock;
                info->file = (char *)MapViewOfFile(info->hMap, FILE_MAP_READ, 0, info->startBlock, blockSize);

                // For sure ...
//              assert(info->file != NULL);
            }
        }
    }
    // Store number of bytes read endBlock exit
    *lpRead = read;
    return result;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
    TFileInfo * info = (TFileInfo *)rf->rfHandleData;

    // Close file mapping and close file
    UnmapViewOfFile(info->file);
    CloseHandle(info->hMap);
    CloseHandle(info->hFile);

    // Free memory
    FreeMemory(info);
    FreeMemory(rf);

    return TRUE;
}

//-----------------------------------------------------------------------------
// Version using Win32 file functions
#else
RFHandle * __stdcall PluginOpenFile(const char * resName, LPCSTR rfDataString)
{
    RFHandle  * rf = (RFHandle *) AllocMemory(sizeof(RFHandle));
    HANDLE hFile;
    
    if(rf != NULL)
    {
        // Use the "sequential scan" flag if opening the file.
        if((hFile = CreateFile(resName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE)
        {
            rf->rfPlugin     = &plugin;
            rf->rfHandleData = hFile;

            return rf;
        }
        FreeMemory(rf);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }
    return NULL;
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
    return SetFilePointer((HANDLE)(rf->rfHandleData), 0, NULL, FILE_CURRENT);
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG filePos, DWORD method)
{
    return SetFilePointer((HANDLE)(rf->rfHandleData), filePos, NULL, method);
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
    return GetFileSize((HANDLE)(rf->rfHandleData),NULL);
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
    return (PluginGetFilePointer(rf) >= PluginGetFileSize(rf));
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD toRead, LPDWORD lpRead)
{
    return ReadFile((HANDLE)(rf->rfHandleData), lpBuffer, toRead, lpRead, NULL);
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
    return (CloseHandle((HANDLE)(rf->rfHandleData)) && (FreeMemory(rf) != 0));
}

#endif // MEMORY_MAPPED_FILE

//-----------------------------------------------------------------------------
// Public functions and plugin structure

static RFPlugin plugin =
{
    0,
    0,
	0,
    NULL,
    "ANX Plain Resource File Plug-In",
    "v1.00",
    NULL,// "All files (*.*)\0*.*\0", ??
    "PLAIN",
    NULL,
    PluginAbout,
    NULL,
    NULL,
    PluginGetFileList,
    PluginFreeFileList,
    PluginOpenFile,
    PluginCloseFile,
    PluginGetFileSize,
    PluginGetFilePointer,
    PluginEndOfFile,
    PluginSetFilePointer,
    PluginReadFile
};

__declspec(dllexport) RFPlugin * __stdcall GetResourceFilePlugin(void)
{
    return &plugin;
}

// DLL entry point (DllMain)
#ifdef _DLL
BOOL WINAPI _DllMainCRTStartup (HANDLE instance, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    // Remember page fileSize and DLL instance
#ifdef MEMORY_MAPPED_FILE
    allocGran = getAllocationGranularity();
#endif // MEMORY_MAPPED_FILE
    plugin.hDllInst = instance;
    return TRUE;
}
#else
// For testing purposes only
void main(int argc, char * argv[])
{
    RFHandle * handle;
    FILE     * fp;
    char buffer[1025];
    DWORD read;

#ifdef MEMORY_MAPPED_FILE
    allocGran = getAllocationGranularity();
#endif // MEMORY_MAPPED_FILE

    if((handle = PluginOpenFile("BroodWar.txt", NULL)) != NULL)
    {
        fp = fopen("CroodWar.txt", "wb");
        
        while(PluginEndOfFile(handle) == FALSE)
        {
            memset(buffer, 0, sizeof(buffer));
            PluginReadFile(handle, buffer, 1024, &read);
            printf(buffer);
            fprintf(fp, buffer);
        }
        PluginCloseFile(handle);
        fclose(fp);
    }
}
#endif // _DLL
