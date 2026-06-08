#ifndef SIGNAL_EDGE_UI_API_H
#define SIGNAL_EDGE_UI_API_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
  #ifdef SIGNAL_EDGE_UI_STATIC
    #define SIGNAL_EDGE_UI_API
  #elif defined(SIGNAL_EDGE_UI_API_EXPORTS)
    #define SIGNAL_EDGE_UI_API __declspec(dllexport)
  #else
    #define SIGNAL_EDGE_UI_API __declspec(dllimport)
  #endif
#else
  #ifdef SIGNAL_EDGE_UI_STATIC
    #define SIGNAL_EDGE_UI_API
  #else
    #define SIGNAL_EDGE_UI_API __attribute__((visibility("default")))
  #endif
#endif

SIGNAL_EDGE_UI_API char *ZD_GetAPIVersion(void);

SIGNAL_EDGE_UI_API char *ZD_GetLastError(void);

SIGNAL_EDGE_UI_API int ZD_ServerCreate(char *device, char* ip);

SIGNAL_EDGE_UI_API void ZD_ServerDestroy(void);

SIGNAL_EDGE_UI_API int ZD_GetGnssData(int channelId, char *data, int acqLen, char *dataInfo);

SIGNAL_EDGE_UI_API int ZD_GetInsData(int channelId, char *data, int acqLen, char *dataInfo);

SIGNAL_EDGE_UI_API int ZD_GetImageData(int channelId, char *data, int acqLen, char *dataInfo);

#ifdef __cplusplus
}
#endif

#endif
