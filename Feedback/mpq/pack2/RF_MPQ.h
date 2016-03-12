/*****************************************************************************/
/* RF_MPQ.h                                  Copyright Project Software 2000 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*  Resource plugin for MPQ archives - header file                           */
/*****************************************************************************/

#ifndef __RF_MPQ_H_
#define __RF_MPQ_H_

//-----------------------------------------------------------------------------
// Defines

#define RF_NODETECT       0             // Don't detect file type
#define RF_AUTODETECT     1             // Autodetect file name by StormLib
#define RF_FILELIST       2             // Use file list for file names

#define RF_DEFFILELIST    0             // Use default file list
#define RF_ASKUSER        1             // Ask user for file list

#define MAGIC_DUMMY       0x12345678    // Number of identify special rfDataString

// Macro for file handle from resource handle
#define FILEHANDLE(rf) (HANDLE)((rf)->rfHandleData)

//-----------------------------------------------------------------------------
// Structures

// MPQ plugin configuration
typedef struct
{
    DWORD detection;                    // File type detection mode
    DWORD fileList;                     // File list type
} TMPQConfig;

// Resource data string if detected file name
typedef struct
{
    CHAR  fileName[13];                 // Plain detected file name (8.3)
    DWORD dummy;                        // Always set to zero (for tell from file name)
    DWORD fileIndex;                    // File index
} TMPQDataString;

//-----------------------------------------------------------------------------
// Global variables

extern RFPlugin Plugin;                 // Pointer to plugin's info

//-----------------------------------------------------------------------------
// Dialogs

typedef struct
{
    RFile * list;                       // Linked file list
    char  * archiveName;                // Archive name
    DWORD   fileCount;                  // Number of files in the archive
    DWORD   detection;                  // File detection method
    char    fileList[MAX_PATH+1];       // File list name
} TFileListInfo;

int AboutDialog(HWND owner);
int ConfigDialog(HWND owner, TMPQConfig * config);
int FileListNameDialog(HWND owner, char * fileName);
int FileListDialog(HWND owner, TFileListInfo * info);


#endif // __RF_MPQ_H_
