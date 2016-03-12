/*****************************************************************************/
/* RF_MPQ.cpp                                Copyright Project Software 2000 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : zezula@volny.cz                                                  */
/* WWW    : www.volny.cz/zezula                                              */
/*---------------------------------------------------------------------------*/
/*  Resource plugin for MPQ archives                                         */
/*****************************************************************************/

#include <stdio.h>

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include "..\plugins.h"

#include "resource.h"
#include "RF_MPQ.h"
#include "Storm.h"

//-----------------------------------------------------------------------------
// Local variables

// Optimization data. The old version opened and closed archive file each time
// when a file was open. But this takes much time on MPQ archiver which are not
// at the begin of the file StormLib must srerch the file for MPQ header each
// time if any file is open.
static TMPQArchive * ha = NULL;

// Plugin configuration
static TMPQConfig config;

//-----------------------------------------------------------------------------
// Public plugin functions

void __stdcall Init(void)
{
    if(Plugin.szINIFileName == NULL)
    {
		config.detection = RF_FILELIST;
        config.fileList  = RF_ASKUSER;
		return;
    }

    config.detection = GetPrivateProfileInt(Plugin.Description, "Detection", RF_FILELIST, Plugin.szINIFileName);
    config.fileList  = GetPrivateProfileInt(Plugin.Description, "FileList",  RF_ASKUSER,  Plugin.szINIFileName);
}

void __stdcall Quit(void)
{
    char buffer[16];

    if(Plugin.szINIFileName == NULL)
        return;

    sprintf(buffer, "%u", config.detection);
    WritePrivateProfileString(Plugin.Description, "Detection", buffer, Plugin.szINIFileName);
    sprintf(buffer, "%u", config.fileList);
    WritePrivateProfileString(Plugin.Description, "FileList", buffer, Plugin.szINIFileName);
}

void __stdcall About(HWND hwnd)
{
    AboutDialog(hwnd);
}

void __stdcall Config(HWND hwnd)
{
    ConfigDialog(hwnd, &config);
}

BOOL __stdcall PluginFreeFileList(RFile* list)
{
    RFile *rnode,*tnode;

    rnode=list;
    while (rnode!=NULL)
    {
		tnode=rnode->next;
		GlobalFree(rnode);
		rnode=tnode;
    }

    return TRUE;
}

RFile * __stdcall PluginGetFileList(const char * mpqName)
{
    TFileListInfo info;
    char fname[_MAX_FNAME];

    // Create/get filelist name
    _splitpath(mpqName, NULL, NULL, fname, NULL);
    _makepath(info.fileList, NULL, NULL, fname, ".txt");

    // If enabled, ask user for the file list
    if(config.detection == RF_FILELIST && config.fileList == RF_ASKUSER)
    {
        if(FileListNameDialog(GetFocus(), info.fileList) != IDOK)
            return NULL;
    }

    info.archiveName = (char *)mpqName;
    info.detection = config.detection;
    info.fileCount = 0;
    info.list = NULL;

    if(FileListDialog(GetFocus(), &info) != IDOK)
        return NULL;

    return info.list;
}

RFHandle* __stdcall PluginOpenFile(const char* resname, LPCSTR rfDataString)
{
    RFHandle * rf;
    HANDLE hArchive = (HANDLE)ha;
    HANDLE hFile;
    CHAR * fileName;

    // Optimization : Check if archive is already open and the same like required
    if(ha == NULL || stricmp(resname, ha->fileName))
    {
        // Not the same archive required or archive not open
        SFileCloseArchive(ha);
        
        // Open the new archive. If failed, return NULL.
        if(SFileOpenArchive((char *)resname, 0, 0, &hArchive) == FALSE)
        {
            SetLastError(PRIVEC(IDS_OPENARCFAILED));
            return NULL;
        }
        ha = (TMPQArchive *)hArchive;
    }

    // Get file name from data string
    TMPQDataString * data = (TMPQDataString *)rfDataString;
    fileName = (CHAR *)rfDataString;
    if(data->dummy == MAGIC_DUMMY)
        fileName = (CHAR *)data->fileIndex;

    // Try to open file
    if(SFileOpenFileEx(hArchive, fileName, 0, &hFile) == TRUE)
    {
        // Try to allocate memory for resource file handle    
        if((rf = new RFHandle) != NULL)
        {
            rf->rfPlugin = &Plugin;
            rf->rfHandleData = hFile;

            return rf;
        }
        else
            SetLastError(PRIVEC(IDS_NOMEM));
        SFileCloseFile(hFile);
    }
    else
        SetLastError(PRIVEC(IDS_OPENFILEFAILED));

    return NULL;
}

BOOL __stdcall PluginCloseFile(RFHandle* rf)
{
    HANDLE     hArchive;
    TMPQFile * hFile = (TMPQFile *)(rf->rfHandleData);

    // Check the right parameters
    if(hFile == NULL)
		return FALSE;

    // Archive handle is stored in file handle, but will not be accessible after
    // Close file.
    hArchive = hFile->hArchive;

    delete rf;

    // Don't close archive here !!
    return (SFileCloseFile(hFile) == TRUE);
}

DWORD __stdcall PluginGetFilePointer(RFHandle* rf)
{
    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return (DWORD)-1;

    return SFileSetFilePointer(FILEHANDLE(rf), 0, NULL, FILE_CURRENT);
}

DWORD __stdcall PluginGetFileSize(RFHandle* rf)
{
    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return (DWORD)-1;

    return SFileGetFileSize(FILEHANDLE(rf), NULL);
}

DWORD __stdcall PluginSetFilePointer(RFHandle* rf, LONG pos, DWORD method)
{
    DWORD filePtr;                      // New file pointer

    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return (DWORD)-1;

    // Move to the new position and check if at the end of file
    filePtr = SFileSetFilePointer(FILEHANDLE(rf), pos, NULL, method);
    
    return filePtr;
}

BOOL __stdcall PluginEndOfFile(RFHandle* rf)
{
    DWORD fileSize;
    DWORD filePtr;

    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return FALSE;

    fileSize = SFileGetFileSize(FILEHANDLE(rf));
    filePtr  = SFileSetFilePointer(FILEHANDLE(rf), 0, NULL, FILE_CURRENT);

    return (BOOL)(filePtr >= fileSize);
}

BOOL __stdcall PluginReadFile(RFHandle* rf, LPVOID lpBuffer, DWORD ToRead, LPDWORD Read)
{
    // Check the right parameters
    if(rf == NULL || FILEHANDLE(rf) == NULL)
		return FALSE;

    // Read file
    return SFileReadFile(FILEHANDLE(rf), (BYTE *)lpBuffer, ToRead, Read);
}

RFPlugin Plugin=
{
    RFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX MPQ Resource File Plug-In",
    "v1.00",
    "Blizzard MPQ Archives (*.MPQ)\0*.mpq\0"
	"EXE Files (may contain MPQ) (*.EXE)\0*.exe\0",
    "MPQ",
    Config,
    About,
    Init,
    Quit,
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

//-----------------------------------------------------------------------------
// Exported functions

// Trick for right function export name 
extern "C"
{
    __declspec(dllexport) RFPlugin* __stdcall GetResourceFilePlugin(void)
    {
        return &Plugin;
    }
}
