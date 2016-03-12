/*****************************************************************************/
/* SelectFilesDialog.cpp                     Copyright Project Software 2000 */
/*                     Dialog for refine file selection                      */
/*****************************************************************************/

#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "..\..\plugins.h"
#include "..\RF_MPQ.h"
#include "..\resource.h"
#include "..\Storm.h"

//-----------------------------------------------------------------------------
// Local functions

// Reads line from text file. The file must be opened in text mode
// Returns -1 if line too long, otherwise number of characters read
int readLine(FILE * fp, char * buffer, int maxChars)
{
    int count = 0;
    int ch;

    if(feof(fp))
        return -1;
    
    while(!feof(fp) && (ch = fgetc(fp)) != -1)
    {
        if(ch == '\n')
            break;
        if(0 <= ch && ch <= 32 && ch != 9)
            ch = 32;

        *buffer++ = (char)ch;
        if(++count == maxChars)
        {
            // Skip line
            while(!feof(fp) && fgetc(fp) != '\n');
            *buffer = 0;
            return -1;                  // Line is to long
        }
    }
    *buffer = 0;
    return count;
}

// Adds Resource node to a list.
static void AddRNode(RFile ** first, RFile ** last, const CHAR * fileName)
{
    RFile * node;                       // Resource file node

    node = (RFile*)GlobalAlloc(GPTR, sizeof(RFile));
    
    // Fill-in file name
    lstrcpy(node->rfDataString, fileName);

    node->next = NULL;

    if(*first == NULL)
		*first = node;
    else
		(*last)->next = node;
    (*last) = node;
}

static void AddFileToList(HWND hdlg, char * fileName)
{
    HWND  hCombo = GetDlgItem(hdlg, IDC_FILTER);
    HWND  hList  = GetDlgItem(hdlg, IDC_FILELIST);
    char  fileMask[16] = "*";
    char *fileExt;

    // Add file name to the list
    ListBox_AddString(hList, fileName);

    // Add mask to combo box, if not yet
    if((fileExt = strrchr(fileName, '.')) != NULL)
    {
        strcat(fileMask, fileExt);

        if(ComboBox_FindStringExact(hCombo, 0, fileMask) == CB_ERR)
            ComboBox_AddString(hCombo, fileMask);
    }
}

static BOOL GetFiles(HWND hdlg, HANDLE hArchive, BOOL detect)
{
    HANDLE  hFile;
    RFile * list = NULL;                // File list
    RFile * last = NULL;                // The last item
    DWORD   index = 0;
    DWORD   nFiles = ((TMPQArchive *)hArchive)->header->nFiles;

    for(index = 0; index < nFiles; index++)
    {
        if(SFileOpenFileEx(hArchive, (CHAR *)index, 0, &hFile) == TRUE)
        {
            char fileName[MAX_PATH+1];

            if(detect == TRUE)
                SFileGetFileName(hFile, fileName);
            else
                sprintf(fileName, "File%04u.xxx", index);
    
            AddFileToList(hdlg, fileName);
            SFileCloseFile(hFile);
        }
        SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETPOS, index, 0L);
    }
    return TRUE;
}

static BOOL GetFilesFromFileList(HWND hdlg, HANDLE hArchive, CHAR * listFile)
{
    FILE * fp;
    DWORD count = 0;

    // Try to open file list
    if((fp = fopen(listFile, "rt")) != NULL)
    {
        while(!feof(fp))
        {
            HANDLE hFile;
            char   fileName[MAX_PATH+1];        // For file name

            if(readLine(fp, fileName, MAX_PATH) == -1)
                continue;
            if(fileName[0] == 0)
                continue;

            if(SFileOpenFileEx(hArchive, fileName, 0, &hFile) == TRUE)
            {
        		AddFileToList(hdlg, fileName);
                SFileCloseFile(hFile);
                SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETPOS, count++, 0L);
            }
        }
        fclose(fp);
        return TRUE;
    }
    else
        MessageBox(hdlg, "Cannot open file list", "Error", MB_OK | MB_ICONEXCLAMATION);

    return FALSE;
}

static void SetStatus(HWND hdlg, CHAR * text = NULL)
{
    if(text == NULL)
        text = "";
    
    SendDlgItemMessage(hdlg, IDC_STATUS, SB_SETTEXT, 0, (LPARAM)text);
}

static void SelectAll(HWND hdlg, DWORD fileCount)
{
    HWND hList  = GetDlgItem(hdlg, IDC_FILELIST);
    int  top = ListBox_GetTopIndex(hList);

    SendMessage(hList, WM_SETREDRAW, FALSE, 0);
    for(DWORD index = 0; index < fileCount; index++)
        ListBox_SetSel(hList, TRUE, index);
    ListBox_SetTopIndex(hList, top);
    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, TRUE);
}

static void UnselectAll(HWND hdlg, DWORD fileCount)
{
    HWND hList  = GetDlgItem(hdlg, IDC_FILELIST);

    for(DWORD index = 0; index < fileCount; index++)
        ListBox_SetSel(hList, FALSE, index);
}

static void InvertSelection(HWND hdlg, DWORD fileCount)
{
    HWND hList  = GetDlgItem(hdlg, IDC_FILELIST);
    int  top = ListBox_GetTopIndex(hList);

    SendMessage(hList, WM_SETREDRAW, FALSE, 0);
    for(DWORD index = 0; index < fileCount; index++)
    {
        BOOL selected = ListBox_GetSel(hList, index);
        ListBox_SetSel(hList, !selected, index);
    }
    ListBox_SetTopIndex(hList, top);
    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, TRUE);
}

static void SelectFileTypes(HWND hdlg, DWORD fileCount)
{
    HWND  hCombo = GetDlgItem(hdlg, IDC_FILTER);
    HWND  hList  = GetDlgItem(hdlg, IDC_FILELIST);
    DWORD index  = ComboBox_GetCurSel(hCombo);
    CHAR  fileType[16];
    CHAR  fileName[MAX_PATH+1];
    CHAR * fileExt;
    CHAR * maskExt;
    int  top = ListBox_GetTopIndex(hList);

    // Get filter
    ComboBox_GetLBText(hCombo, index, fileType);
    maskExt = strrchr(fileType, '.');

    SendMessage(hList, WM_SETREDRAW, FALSE, 0);
    for(DWORD i = 0; i < fileCount; i++)
    {
        BOOL select = FALSE;

        ListBox_GetText(hList, i, fileName);
        
        if((fileExt = strrchr(fileName, '.')) != NULL)
        {
            if(!stricmp(fileExt, maskExt) || !stricmp(maskExt, ".*"))
                select = TRUE;
        }

        ListBox_SetSel(hList, select, i);
    }
    ListBox_SetTopIndex(hList, top);
    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, TRUE);
}
    
// Load files into file list
static BOOL LoadFiles(HWND hdlg, TFileListInfo * info)
{
    HANDLE hArchive;
    BOOL   result = FALSE;

    // Open archive
    if(SFileOpenArchive(info->archiveName, 0, 0, &hArchive) == TRUE)
    {
        // Get number of files
        info->fileCount = ((TMPQArchive *)hArchive)->header->nFiles;

        // Initialize progress control
        SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, info->fileCount));
        SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETPOS, 0, 0L);

        // Determine file type detection
        switch(info->detection)
        {
            case RF_NODETECT:
                result = GetFiles(hdlg, hArchive, FALSE);
                break;

            case RF_AUTODETECT:
                result = GetFiles(hdlg, hArchive, TRUE);
                break;

            case RF_FILELIST:
                result = GetFilesFromFileList(hdlg, hArchive, info->fileList);
                break;
        }
        SFileCloseArchive(hArchive);
    }
    else
        MessageBox(hdlg, "Cannot open archive file", "Error", MB_OK | MB_ICONEXCLAMATION);

    return result;
}

// Initializes dialog box with starting values
static void InitDialog(HWND hdlg, TFileListInfo * info)
{
    HWND    hList  = GetDlgItem(hdlg, IDC_FILELIST);
    HWND    hCombo = GetDlgItem(hdlg, IDC_FILTER);
    RECT    r;
    WORD    count = 0;
    int     pt[1];

    if(info == NULL)
        return;

    // Initialize status bar
	GetClientRect(GetDlgItem(hdlg, IDC_STATUS), &r);
    pt[0] = r.right;
	SendDlgItemMessage(hdlg, IDC_STATUS, SB_SETPARTS, 1, (LPARAM)pt);

    // Show the entire dialog
    ShowWindow(hdlg, SW_SHOW);
    UpdateWindow(hdlg);

    // Init file list and filter
    SetStatus(hdlg, "Initializing file list ...");
    SendMessage(hList, WM_SETREDRAW, FALSE, 0);
    SendMessage(hCombo, WM_SETREDRAW, FALSE, 0);

    if(LoadFiles(hdlg, info) == FALSE)
        EndDialog(hdlg, IDCANCEL);
    
    // Select all items by default
    SelectAll(hdlg, info->fileCount);
    ComboBox_AddString(hCombo, "*.*");

    // Redraw list box and combo box
    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    SendMessage(hCombo, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, TRUE);
    InvalidateRect(hCombo, NULL, TRUE);
    SetStatus(hdlg);

    // Clear progres box
    SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETPOS, 0, 0L);
}

// Stores dialog's settings to file list
static void SaveDialog(HWND hdlg, TFileListInfo * info)
{
    HWND    hList = GetDlgItem(hdlg, IDC_FILELIST);
    RFile * list  = NULL;
    RFile * last  = NULL;
    char    fileName[MAX_PATH+1];
    DWORD   index;

    SetStatus(hdlg, "Saving file list");

    // Pass through file list and invalidate all unselected items
    for(index = 0; index < info->fileCount; index++)
    {
        // If not selected, skip
        if(ListBox_GetSel(hList, index) == TRUE)
        {
            // Get text from list box
            ListBox_GetText(hList, index, fileName);

            // Add file name
            AddRNode(&list, &last, fileName);
        }
        // Update progress window
        SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETPOS, index, 0L);
    }

    SetStatus(hdlg);
    SendDlgItemMessage(hdlg, IDC_PROGRESS, PBM_SETPOS, 0, 0L);
    info->list = list;
}

static BOOL DialogProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static TFileListInfo * info;        // File list

    if(message == WM_COMMAND)
    {
        int notify = HIWORD(wParam);
        int ctrl   = LOWORD(wParam);

        if(notify == BN_CLICKED)
        {
            switch(ctrl)
            {
                case IDOK:
                    SaveDialog(hdlg, info);
                    EndDialog(hdlg, ctrl);
                    break;

                case IDCANCEL:
                    EndDialog(hdlg, ctrl);
                    break;

                case IDSELECTALL:
                    SelectAll(hdlg, info->fileCount);
                    break;

                case IDUNSELECTALL:
                    UnselectAll(hdlg, info->fileCount);
                    break;

                case IDINVERT:
                    InvertSelection(hdlg, info->fileCount);
                    break;
            }
        }
        if(ctrl == IDC_FILTER && notify == CBN_SELCHANGE)
            SelectFileTypes(hdlg, info->fileCount);

        return TRUE;
    }
    if(message == WM_INITDIALOG)
    {
        info = (TFileListInfo *)lParam;
        InitDialog(hdlg, info);
        return TRUE;
    }
    return FALSE;
}

int FileListDialog(HWND owner, TFileListInfo * info)
{
    return DialogBoxParam(Plugin.hDllInst, MAKEINTRESOURCE(IDD_FILELIST), owner, (DLGPROC)DialogProc, (LPARAM)info);
}
