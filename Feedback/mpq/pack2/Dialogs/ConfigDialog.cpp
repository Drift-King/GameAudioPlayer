/*****************************************************************************/
/* ConfigDialog.cpp                          Copyright Project Software 2000 */
/*                Configuration dialog for MPQ resource plugin               */
/*****************************************************************************/

#include <windows.h>

#include "..\..\plugins.h"
#include "..\RF_MPQ.h"
#include "..\resource.h"

static void InitDialog(HWND hdlg, TMPQConfig * config)
{
    SendDlgItemMessage(hdlg, IDC_RADIO1 + config->detection, BM_SETCHECK, TRUE, 0);

    if(config->detection == RF_FILELIST)
        EnableWindow(GetDlgItem(hdlg, IDC_FILELISTTITLE), TRUE);

    SendDlgItemMessage(hdlg, IDC_RADIO4 + config->fileList, BM_SETCHECK, TRUE, 0);
}

static void SaveDialog(HWND hdlg, TMPQConfig * config)
{
    for(int i = IDC_RADIO1; i <= IDC_RADIO3; i++)
    {
        if(SendDlgItemMessage(hdlg, i, BM_GETCHECK, 0, 0))
            config->detection = i - IDC_RADIO1;
    }

    for(i = IDC_RADIO4; i <= IDC_RADIO5; i++)
    {
        if(SendDlgItemMessage(hdlg, i, BM_GETCHECK, 0, 0))
            config->fileList = i - IDC_RADIO4;
    }
}

static void SetDefaultConfig(HWND hdlg)
{
    SendDlgItemMessage(hdlg, IDC_RADIO1, BM_SETCHECK, FALSE, 0);
    SendDlgItemMessage(hdlg, IDC_RADIO2, BM_SETCHECK, TRUE,  0);
    SendDlgItemMessage(hdlg, IDC_RADIO3, BM_SETCHECK, FALSE, 0);

    EnableWindow(GetDlgItem(hdlg, IDC_FILELISTTITLE), FALSE);
    SendDlgItemMessage(hdlg, IDC_RADIO4, BM_SETCHECK, FALSE, 0);
    SendDlgItemMessage(hdlg, IDC_RADIO5, BM_SETCHECK, TRUE, 0);
}

static void UpdateDialog(HWND hdlg, TMPQConfig * config)
{
    BOOL fileList = (BOOL)SendDlgItemMessage(hdlg, IDC_RADIO3, BM_GETCHECK, 0, 0);

    EnableWindow(GetDlgItem(hdlg, IDC_FILELISTTITLE), fileList);
    EnableWindow(GetDlgItem(hdlg, IDC_RADIO4), fileList);
    EnableWindow(GetDlgItem(hdlg, IDC_RADIO5), fileList);
}

static BOOL DialogProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static TMPQConfig * config;

    if(message == WM_COMMAND)
    {
        int notify = HIWORD(wParam);
        int ctrl   = LOWORD(wParam);

        if(notify == BN_CLICKED)
        {
            // If the OK button was pressed
            if(ctrl == IDOK)
            {
                SaveDialog(hdlg, config);    
                EndDialog(hdlg, ctrl);
            }
            // If the Cancel button was pressed
            if(ctrl == IDCANCEL)
                EndDialog(hdlg, ctrl);

            // If the Default button was pressed
            if(ctrl == IDDEFAULT)
                SetDefaultConfig(hdlg);

            // If "Use file list" radio button was changed
            UpdateDialog(hdlg, config);
            return TRUE;
        }
    }
    if(message == WM_INITDIALOG)
    {
        config = (TMPQConfig *)lParam;
        InitDialog(hdlg, config);
        UpdateDialog(hdlg, config);
        return TRUE;
    }
    return FALSE;                             
}

int ConfigDialog(HWND owner, TMPQConfig * config)
{
    return DialogBoxParam(Plugin.hDllInst, MAKEINTRESOURCE(IDD_CONFIG), owner, (DLGPROC)DialogProc, (LPARAM)config);
}
