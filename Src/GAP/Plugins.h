#ifndef _GAP_PLUGINS_H
#define _GAP_PLUGINS_H

#include <windows.h>

#include "..\Plugins\ResourceFile\RFPlugin.h"
#include "..\Plugins\AudioFile\AFPlugin.h"

char afPluginDir[MAX_PATH];
char rfPluginDir[MAX_PATH];

typedef struct tagPluginNode {
    struct tagPluginNode *prev,*next;
    LPVOID plugin;
	LPVOID data;
    char  pluginFileName[MAX_PATH];
} PluginNode;

#define AFNODE(p) ((AFPlugin*)(p->plugin))
#define RFNODE(p) ((RFPlugin*)(p->plugin))

extern PluginNode* afFirstPlugin;
extern PluginNode* rfFirstPlugin;

void InitPlugins(void);
void ShutdownPlugins(void);
AFPlugin* GetAFPlugin(const char* id);
RFPlugin* GetRFPlugin(const char* id);

#endif
