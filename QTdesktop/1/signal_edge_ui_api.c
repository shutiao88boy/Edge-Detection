#define SIGNAL_EDGE_UI_API_EXPORTS
#include "signal_edge_ui_api.h"
#include <string.h>
#include <stdio.h>

static char g_lastError[1024] = {0};
static int g_serverConnected = 0;

static void setLastError(const char *error) {
    strncpy(g_lastError, error, sizeof(g_lastError) - 1);
    g_lastError[sizeof(g_lastError) - 1] = '\0';
}

SIGNAL_EDGE_UI_API char *ZD_GetAPIVersion(void) {
    static char version[] = "1.0.1";
    return version;
}

SIGNAL_EDGE_UI_API char *ZD_GetLastError(void) {
    return g_lastError;
}

SIGNAL_EDGE_UI_API int ZD_ServerCreate(char *device, char* ip) {
    if (!device || !ip) {
        setLastError("Invalid device or IP parameter");
        return -1;
    }
    
    if (g_serverConnected) {
        setLastError("Server already connected");
        return -2;
    }
    
    g_serverConnected = 1;
    setLastError("");
    return 0;
}

SIGNAL_EDGE_UI_API void ZD_ServerDestroy(void) {
    g_serverConnected = 0;
    setLastError("");
}

SIGNAL_EDGE_UI_API int ZD_GetGnssData(int channelId, char *data, int acqLen, char *dataInfo) {
    if (!g_serverConnected) {
        setLastError("Server not connected");
        return -1;
    }
    
    if (!data || !dataInfo) {
        setLastError("Invalid data or dataInfo parameter");
        return -2;
    }
    
    if (channelId < 0) {
        setLastError("Invalid channel ID");
        return -3;
    }
    
    memset(data, 0, acqLen);
    snprintf(dataInfo, 1024, "GNSS data for channel %d, length: %d", channelId, acqLen);
    setLastError("");
    return 0;
}

SIGNAL_EDGE_UI_API int ZD_GetInsData(int channelId, char *data, int acqLen, char *dataInfo) {
    if (!g_serverConnected) {
        setLastError("Server not connected");
        return -1;
    }
    
    if (!data || !dataInfo) {
        setLastError("Invalid data or dataInfo parameter");
        return -2;
    }
    
    if (channelId < 0) {
        setLastError("Invalid channel ID");
        return -3;
    }
    
    memset(data, 0, acqLen);
    snprintf(dataInfo, 1024, "INS data for channel %d, length: %d", channelId, acqLen);
    setLastError("");
    return 0;
}

SIGNAL_EDGE_UI_API int ZD_GetImageData(int channelId, char *data, int acqLen, char *dataInfo) {
    if (!g_serverConnected) {
        setLastError("Server not connected");
        return -1;
    }
    
    if (!data || !dataInfo) {
        setLastError("Invalid data or dataInfo parameter");
        return -2;
    }
    
    if (channelId < 0) {
        setLastError("Invalid channel ID");
        return -3;
    }
    
    memset(data, 0, acqLen);
    snprintf(dataInfo, 1024, "Image data for channel %d, length: %d", channelId, acqLen);
    setLastError("");
    return 0;
}
