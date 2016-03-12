#ifndef _GAP_MESSAGES_H
#define _GAP_MESSAGES_H

#include <windows.h>

// progress messages
#define WM_GAP_ISCANCELLED              (WM_USER+1)
#define WM_GAP_ISALLCANCELLED           (WM_USER+2)
#define WM_GAP_SHOWPROGRESS             (WM_USER+3)
#define WM_GAP_SHOWFILEPROGRESS         (WM_USER+4)
#define WM_GAP_SHOWPROGRESSHEADER       (WM_USER+5)
#define WM_GAP_SHOWPROGRESSSTATE        (WM_USER+6)
#define WM_GAP_SHOWMASTERPROGRESS       (WM_USER+7)
#define WM_GAP_SHOWMASTERPROGRESSHEADER (WM_USER+8)

// playback messages
#define WM_GAP_END_OF_PLAYBACK          (WM_USER+9)
#define WM_GAP_SHOW_PLAYBACK_TIME       (WM_USER+10)
#define WM_GAP_SHOW_PLAYBACK_POS        (WM_USER+11)

#endif
