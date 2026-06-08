/*
 * signaledge_ui_api.cpp —— 边缘计算组件 C API 实现（独立 DLL/SO 构建版本）
 *
 * 对外暴露 7 个 API 函数：
 *   1. ZD_GetAPIVersion()
 *   2. ZD_GetLastError()
 *   3. ZD_ServerCreate(device, ip)   — 内部读取 config.txt 获取串口参数
 *   4. ZD_ServerDestroy()
 *   5. ZD_GetGnssData()
 *   6. ZD_GetInsData()
 *   7. ZD_GetImageData()
 */

#include "signaledge_ui_api.h"

/* SIGNAL_EDGE_UI_API_EXPORTS 在独立构建 DLL 时由编译器命令行定义 */
#include "serial_port.h"
#include "config_file.h"   /* 内部私有：读取 config.txt */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════
 * 内部状态
 * ═══════════════════════════════════════════════════════ */

static serial_handle_t g_serialHandle = SERIAL_INVALID_HANDLE;
static char g_lastError[1024] = {0};

static void setLastError(const char *error) {
    strncpy(g_lastError, error, sizeof(g_lastError) - 1);
    g_lastError[sizeof(g_lastError) - 1] = '\0';
}

/* ═══════════════════════════════════════════════════════
 * 版本与错误查询
 * ═══════════════════════════════════════════════════════ */

SIGNAL_EDGE_UI_API char *ZD_GetAPIVersion(void) {
    static char version[] = "1.0.1";
    return version;
}

SIGNAL_EDGE_UI_API char *ZD_GetLastError(void) {
    return g_lastError;
}

/* ═══════════════════════════════════════════════════════
 * 连接管理
 * ═══════════════════════════════════════════════════════ */

/*
 * ZD_ServerCreate —— 创建与载荷设备的连接
 *
 * 参数：
 *   device - 设备型号标识（如 "SRP-Jeston"）
 *   ip     - 载荷 IP 地址（如 "Localhost"）
 *
 * 内部流程：
 *   1. 参数校验（device 非空、串口未重复打开）
 *   2. 从 config.txt 读取 COM_PORT 和 BAUD_RATE
 *   3. 解析波特率为整数
 *   4. 调用 serial_open() 打开串口
 *   5. 成功返回 0，失败返回负数错误码
 */
SIGNAL_EDGE_UI_API int ZD_ServerCreate(char *device, char *ip) {
    if (!device) {
        setLastError("Invalid device name (NULL)");
        return -1;
    }

    if (serial_is_open(g_serialHandle)) {
        setLastError("Server already connected, call ZD_ServerDestroy first");
        return -2;
    }

    // 从 config.txt 加载串口参数
    char comPort[64] = {0};
    char baudStr[16] = {0};

    if (config_read_str(NULL, "COM_PORT", comPort, sizeof(comPort)) != 0) {
        setLastError("Config: COM_PORT not found in config.txt");
        return -3;
    }
    if (config_read_str(NULL, "BAUD_RATE", baudStr, sizeof(baudStr)) != 0) {
        setLastError("Config: BAUD_RATE not found in config.txt");
        return -4;
    }

    int baudRate = 115200;
    if (baudStr[0] != '\0') {
        char *endptr = NULL;
        long val = strtol(baudStr, &endptr, 10);
        if (endptr != baudStr && *endptr == '\0' && val > 0) {
            baudRate = (int)val;
        } else {
            setLastError("Config: invalid BAUD_RATE value");
            return -5;
        }
    }

    g_serialHandle = serial_open(comPort, baudRate);
    if (!serial_is_open(g_serialHandle)) {
        setLastError("Failed to open serial port");
        return -6;
    }

    setLastError("");
    return 0;
}

SIGNAL_EDGE_UI_API void ZD_ServerDestroy(void) {
    if (serial_is_open(g_serialHandle)) {
        serial_close(g_serialHandle);
        g_serialHandle = SERIAL_INVALID_HANDLE;
    }
    setLastError("");
}

/* ═══════════════════════════════════════════════════════
 * 数据采集（内部通用实现）
 * ═══════════════════════════════════════════════════════ */

static int readSerialData(int channelId, char *data, int acqLen, char *dataInfo,
                          const char *dataType) {
    if (!serial_is_open(g_serialHandle)) {
        setLastError("Server not connected");
        return -1;
    }
    if (!data || !dataInfo) {
        setLastError("Invalid data or dataInfo parameter");
        return -2;
    }
    if (channelId < 0 || channelId > 7) {
        setLastError("Invalid channel ID (valid: 0-7)");
        return -3;
    }
    if (acqLen <= 0) {
        setLastError("Invalid acquisition length");
        return -4;
    }

    memset(data, 0, (size_t)acqLen);
    int bytesRead = serial_read(g_serialHandle, data, acqLen);
    if (bytesRead < 0) {
        setLastError("Serial read error");
        return -5;
    }

    time_t now = time(NULL);
    struct tm tmbuf;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tmbuf, &now);
#else
    localtime_r(&now, &tmbuf);
#endif
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmbuf);

    snprintf(dataInfo, 1024,
             "%s|ch=%d|bytes=%d|ts=%s",
             dataType, channelId, bytesRead, timeStr);

    setLastError("");
    return 0;
}

/* ═══════════════════════════════════════════════════════
 * 数据采集（公开 API）
 * ═══════════════════════════════════════════════════════ */

SIGNAL_EDGE_UI_API int ZD_GetGnssData(int channelId, char *data, int acqLen, char *dataInfo) {
    return readSerialData(channelId, data, acqLen, dataInfo, "GNSS");
}

SIGNAL_EDGE_UI_API int ZD_GetInsData(int channelId, char *data, int acqLen, char *dataInfo) {
    return readSerialData(channelId, data, acqLen, dataInfo, "INS");
}

SIGNAL_EDGE_UI_API int ZD_GetImageData(int channelId, char *data, int acqLen, char *dataInfo) {
    return readSerialData(channelId, data, acqLen, dataInfo, "IMG");
}
