/*
 * serial_port.h —— 跨平台串口抽象层头文件
 *
 * 提供统一的串口操作接口，屏蔽 Windows（CreateFile/ReadFile）和
 * Linux（termios/open/read）的差异。
 *
 * 句柄类型：
 *   - Windows: HANDLE（CreateFile 返回）
 *   - Linux:   int（open 返回的文件描述符）
 */

#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 跨平台串口句柄类型定义 ────────────────────────── */
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
typedef HANDLE serial_handle_t;
#define SERIAL_INVALID_HANDLE INVALID_HANDLE_VALUE
#else
typedef int serial_handle_t;
#define SERIAL_INVALID_HANDLE (-1)
#endif

/* ── 串口生命周期管理 ──────────────────────────────── */

/**
 * @brief 打开串口，配置波特率 / 8N1 / 非阻塞模式
 * @param portName Windows: "COM3", Linux: "/dev/ttyTHS1"
 * @param baudRate 波特率（如 115200）
 * @return 有效句柄，失败返回 SERIAL_INVALID_HANDLE
 */
serial_handle_t serial_open(const char *portName, int baudRate);

/** @brief 关闭串口 */
void serial_close(serial_handle_t handle);

/** @return 1=已打开, 0=未打开 */
int serial_is_open(serial_handle_t handle);

/* ── 数据收发 ──────────────────────────────────────── */

/**
 * @brief 非阻塞读取
 * @return 实际字节数，0=暂无数据，<0=错误
 */
int serial_read(serial_handle_t handle, char *buf, int len);

/**
 * @brief 阻塞写入
 * @return 实际写入字节数，<0=错误
 */
int serial_write(serial_handle_t handle, const char *buf, int len);

/* ── 缓冲管理 ──────────────────────────────────────── */

/** @brief 清空接收缓冲区 */
void serial_flush_rx(serial_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_PORT_H */
