#ifndef _GAP_PROGRESS_H
#define _GAP_PROGRESS_H

#include <windows.h>

#include "..\Plugins\ResourceFile\RFPlugin.h"

#define PDT_SIMPLE  (0)
#define PDT_SINGLE  (1)
#define PDT_SINGLE2 (2)
#define PDT_DOUBLE  (3)

BOOL IsCancelled(void);
BOOL IsAllCancelled(void);
void ShowMasterProgress(DWORD cur, DWORD full);
void ShowProgress(DWORD cur, DWORD full);
void ShowFileProgress(RFHandle* f);
void ShowMasterProgressHeaderMsg(const char* msg);
void ShowProgressHeaderMsg(const char* msg);
void ShowProgressStateMsg(const char* msg);
HWND OpenProgress(HWND hwnd, DWORD type, const char* title);
void CloseProgress(void);
void ResetCancelFlag(void);
void SetCancelFlag(void);
void ResetCancelAllFlag(void);
void SetCancelAllFlag(void);

#endif
