/*****************************************************************************/
/* FileListDialog.cpp                        Copyright Project Software 2000 */
/*                   Dialog for selecting file list                          */
/*****************************************************************************/

#include <windows.h>

#include "..\..\plugins.h"
#include "..\RF_MPQ.h"
#include "..\resource.h"

static char * filter = "Filelists (*.txt)\x0*.txt\x0"
                       "All files (*.*)\x0*.*\x0"
                       "\x0\x0";

static BOOL Browse(HWND owner, char * fileName)
{
    OPENFILENAME open;

    memset(&open, 0, sizeof(OPENFILENAME));
    open.lStructSize     = sizeof(OPENFILENAME);  // Size of the OPENFILENAME structure  
    open.hwndOwner       = owner;                 // Dialog owner
    open.hInstance       = Plugin.hDllInst;       // Instance
    open.lpstrFilter     = filter;                // Search files types
    open.nFilterIndex    = 1;                     // Default filter types
    open.lpstrFile       = (LPSTR)fileName;       // Buffer for storing file name
    open.nMaxFile        = MAX_PATH;              // Max. file name length
    open.lpstrInitialDir = NULL;                  // Initial directory for search
    open.lpstrTitle      = "Select filelist";
    open.Flags           = OFN_HIDEREADONLY | OFN_EXPLORER | OFN_LONGNAMES | OFN_NOTESTFILECREATE;
    open.lpstrDefExt     = "txt";

    return GetOpenFileName(&open);
}

static BOOL DialogProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static char * fileName;
    
    if(message == WM_COMMAND)
    {
        int notify = HIWORD(wParam);
        int ctrl   = LOWORD(wParam);

        if(notify == BN_CLICKED)
        {
            if(ctrl == IDBROWSE)
            {
                if(Browse(hdlg, fileName) == TRUE)
                    SetDlgItemText(hdlg, IDC_EDIT1, fileName);
                return TRUE;
            }
            if(ctrl == IDOK)
                GetDlgItemText(hdlg, IDC_EDIT1, fileName, MAX_PATH);
            EndDialog(hdlg, ctrl);
            return TRUE;
        }
    }
    if(message == WM_INITDIALOG)
    {
        fileName = (char *)lParam;
        SendMessage(GetDlgItem(hdlg, IDC_EDIT1), EM_LIMITTEXT, MAX_PATH, 0);
        SetDlgItemText(hdlg, IDC_EDIT1, fileName);
        return TRUE;
    }
    return FALSE;
}

int FileListNameDialog(HWND owner, char * fileName)
{
    return DialogBoxParam(Plugin.hDllInst, MAKEINTRESOURCE(IDD_FILELISTNAME), owner, (DLGPROC)DialogProc, (LPARAM)fileName);
}
