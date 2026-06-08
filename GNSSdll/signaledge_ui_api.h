/*
 * signaledge_ui_api.h —— 边缘计算组件 C 语言 API 接口
 *
 * 本动态库（DLL/SO）仅对外暴露以下 7 个函数：
 *   1. ZD_GetAPIVersion()                                    — 获取 API 版本号
 *   2. ZD_GetLastError()                                     — 获取最近一次错误描述
 *   3. ZD_ServerCreate(device, ip, comPort, baudRate)        — 创建连接，打开串口
 *   4. ZD_ServerDestroy()                                    — 销毁连接，关闭串口
 *   5. ZD_GetGnssData(channelId, data, len, info)            — 获取 GNSS 原始数据
 *   6. ZD_GetInsData(channelId, data, len, info)             — 获取 INS 原始数据
 *   7. ZD_GetImageData(channelId, data, len, info)           — 获取图像原始数据
 *
 * 注：串口参数（端口名、波特率）由调用方传入，库不负责配置文件管理。
 */

#ifndef SIGNAL_EDGE_UI_API_H
#define SIGNAL_EDGE_UI_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 平台相关的导入导出宏 ──────────────────────────── */
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

/* ── 版本与错误查询 ────────────────────────────────── */

/** @brief 获取 API 版本号 */
SIGNAL_EDGE_UI_API char *ZD_GetAPIVersion(void);

/** @brief 获取最近一次错误描述字符串 */
SIGNAL_EDGE_UI_API char *ZD_GetLastError(void);

/* ── 连接管理 ──────────────────────────────────────── */

/**
 * @brief 创建与载荷设备的连接，初始化串口
 * @param device 设备型号（如 "SRP-Jeston"）
 * @param ip     载荷 IP 地址（如 "Localhost"）
 * @return 0=成功，非0=失败（通过 ZD_GetLastError 获取错误描述）
 *
 * 内部流程：读取 config.txt 获取 COM_PORT / BAUD_RATE → 打开串口
 */
SIGNAL_EDGE_UI_API int ZD_ServerCreate(char *device, char *ip);

/** @brief 销毁连接，关闭串口释放资源 */
SIGNAL_EDGE_UI_API void ZD_ServerDestroy(void);

/* ── 数据采集 ──────────────────────────────────────── */

/**
 * @brief 获取 GNSS 卫星导航原始数据
 * @param channelId 通道编号（0-7）
 * @param data      输出缓冲区（存放 NMEA 原始报文）
 * @param acqLen    缓冲区大小
 * @param dataInfo  输出信息（格式: "GNSS|ch=X|bytes=N|ts=时间"）
 * @return 0=成功，非0=失败
 */
SIGNAL_EDGE_UI_API int ZD_GetGnssData(int channelId, char *data,
                                      int acqLen, char *dataInfo);

/**
 * @brief 获取惯性导航原始数据
 * @param channelId 通道编号（0-7）
 * @param data      输出缓冲区
 * @param acqLen    缓冲区大小
 * @param dataInfo  输出信息（格式: "INS|ch=X|bytes=N|ts=时间"）
 * @return 0=成功，非0=失败
 */
SIGNAL_EDGE_UI_API int ZD_GetInsData(int channelId, char *data,
                                     int acqLen, char *dataInfo);

/**
 * @brief 获取图像原始数据
 * @param channelId 通道编号（0-7）
 * @param data      输出缓冲区
 * @param acqLen    缓冲区大小
 * @param dataInfo  输出信息（格式: "IMG|ch=X|bytes=N|ts=时间"）
 * @return 0=成功，非0=失败
 */
SIGNAL_EDGE_UI_API int ZD_GetImageData(int channelId, char *data,
                                       int acqLen, char *dataInfo);

#ifdef __cplusplus
}
#endif

#endif
