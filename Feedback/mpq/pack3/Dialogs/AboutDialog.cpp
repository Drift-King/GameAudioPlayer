/*****************************************************************************/
/* AboutDialog.cpp                           Copyright Project Software 2000 */
/*                   About Dialog for MPQ resource plugin                    */
/*****************************************************************************/

#include <windows.h>

#include "..\..\plugins.h"
#include "..\RF_MPQ.h"
#include "..\resource.h"

static const char * thanksText =
	"This plug-in wouldn't be possible without the help of the following people whom I'd like to thank:\n"
	"(brainspin@hanmail.net)\n"
    "\tfor the STORMING source code,\n"
	"Ted Lyngmo (ted@lyncon.se)\n"
    "\tfor the StarCrack mailing list,\n"
	"King Arthur (KingArthur@warzone.com),\nUnknown Mnemonic (zorohack@hotmail.com)\n"
	"\tfor sending me the filelists for many MPQ-games,\n"
	"Sean Mims (smims@hotmail.com),\nDavid \"Splice\" James (beta@rogers.wave.ca)\n"
	"\tfor the invaluable help with the STORM.DLL API.\n";

static BOOL DialogProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(message == WM_COMMAND)
    {
        int notify = HIWORD(wParam);
        int ctrl   = LOWORD(wParam);

        if(notify == BN_CLICKED)
        {
            EndDialog(hdlg, ctrl);
            return TRUE;
        }
    }
    if(message == WM_INITDIALOG)
    {
        SetDlgItemText(hdlg, IDC_THANKS, thanksText);
        return TRUE;
    }
    return FALSE;
}

int AboutDialog(HWND owner)
{
    return DialogBox(Plugin.hDllInst, MAKEINTRESOURCE(IDD_ABOUT), owner, (DLGPROC)DialogProc);
}
