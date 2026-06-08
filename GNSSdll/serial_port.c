/*
 * serial_port.c —— 跨平台串口抽象层实现（GNSSdll 副本）
 *
 * 架构说明：
 *   ┌─────────────────────────────────────────┐
 *   │  上层调用（C API / Qt C++）              │
 *   ├─────────────────────────────────────────┤
 *   │  serial_port.h（统一接口）               │
 *   ├──────────────────┬──────────────────────┤
 *   │  Windows 实现     │  Linux 实现           │
 *   │  CreateFile/DCB   │  open/termios        │
 *   │  ReadFile/Write   │  read/write          │
 *   └──────────────────┴──────────────────────┘
 */

#include "serial_port.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
/*
 * ╔═══════════════════════════════════════════════════╗
 * ║           Windows 串口实现                         ║
 * ║  使用 Win32 API: CreateFile / DCB / COMMTIMEOUTS  ║
 * ╚═══════════════════════════════════════════════════╝
 */

/*
 * 打开串口并配置：
 *   1. CreateFile 打开 "\\.\COMx"（支持 COM10+）
 *   2. GetCommState + DCB 设置 8N1、指定波特率、使能 DTR/RTS
 *   3. COMMTIMEOUTS 设为非阻塞（ReadIntervalTimeout=MAXDWORD）
 *   4. PurgeComm 清空收发缓冲
 */
serial_handle_t serial_open(const char *portName, int baudRate) {
    if (!portName) return SERIAL_INVALID_HANDLE;

    // 构造完整设备路径，如 "\\.\COM6"
    char fullName[64];
    snprintf(fullName, sizeof(fullName), "\\\\.\\%s", portName);

    // 步骤 1：以独占模式打开串口
    HANDLE hCom = CreateFileA(
        fullName,
        GENERIC_READ | GENERIC_WRITE,
        0,          /* 独占访问 */
        NULL,       /* 默认安全属性 */
        OPEN_EXISTING,
        0,          /* 非重叠 I/O */
        NULL
    );

    if (hCom == INVALID_HANDLE_VALUE) {
        return SERIAL_INVALID_HANDLE;
    }

    // 步骤 2：配置串口参数（8N1 + 指定波特率）
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hCom, &dcb)) {
        CloseHandle(hCom);
        return SERIAL_INVALID_HANDLE;
    }

    dcb.BaudRate = (DWORD)baudRate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hCom, &dcb)) {
        CloseHandle(hCom);
        return SERIAL_INVALID_HANDLE;
    }

    // 步骤 3：设置为非阻塞模式
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;
    SetCommTimeouts(hCom, &timeouts);

    // 步骤 4：清空残留数据
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return hCom;
}

/* 关闭串口句柄 */
void serial_close(serial_handle_t handle) {
    if (handle != SERIAL_INVALID_HANDLE) {
        CloseHandle(handle);
    }
}

/* 判断句柄是否有效 */
int serial_is_open(serial_handle_t handle) {
    return handle != SERIAL_INVALID_HANDLE;
}

/* 非阻塞读取：ReadFile 立即返回已有数据，无数据时返回 0 */
int serial_read(serial_handle_t handle, char *buf, int len) {
    if (handle == SERIAL_INVALID_HANDLE || !buf || len <= 0) return -1;

    DWORD bytesRead = 0;
    if (!ReadFile(handle, buf, (DWORD)len, &bytesRead, NULL)) {
        return -2;
    }
    return (int)bytesRead;
}

/* 阻塞写入 */
int serial_write(serial_handle_t handle, const char *buf, int len) {
    if (handle == SERIAL_INVALID_HANDLE || !buf || len <= 0) return -1;

    DWORD bytesWritten = 0;
    if (!WriteFile(handle, buf, (DWORD)len, &bytesWritten, NULL)) {
        return -2;
    }
    return (int)bytesWritten;
}

/* 清空接收缓冲 */
void serial_flush_rx(serial_handle_t handle) {
    if (handle != SERIAL_INVALID_HANDLE) {
        PurgeComm(handle, PURGE_RXCLEAR);
    }
}

#else
/*
 * ╔═══════════════════════════════════════════════════╗
 * ║           Linux 串口实现（Jetson / ARM64）        ║
 * ║  使用 POSIX 接口: open / termios / read / write   ║
 * ╚═══════════════════════════════════════════════════╝
 */

#include <errno.h>    /* errno, EAGAIN, EWOULDBLOCK */
#include <fcntl.h>     /* open, O_RDWR, O_NOCTTY, O_NONBLOCK */
#include <sys/types.h> /* ssize_t（ARM64 需显式包含） */
#include <termios.h>   /* struct termios, tcgetattr, tcsetattr, cfsetospeed */
#include <unistd.h>    /* read, write, close */

/*
 * 将整型波特率转换为 termios 的 speed_t 常量
 * 支持常见速率：9600 ~ 921600
 */
static speed_t baud_to_speed(int baudRate) {
    switch (baudRate) {
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 921600:  return B921600;
    default:      return B115200;
    }
}

/*
 * 打开串口并配置为原始模式（raw mode）：
 *   1. open() 以非阻塞方式打开设备节点
 *   2. tcgetattr() 获取当前属性
 *   3. cfsetospeed/cfsetispeed 设置波特率
 *   4. 配置 c_cflag:  8N1, 忽略调制解调器, 禁用硬件流控
 *   5. 配置 c_lflag:  原始模式（无回显/信号/行处理）
 *   6. 配置 c_iflag:  禁用软件流控, 不做输入转换
 *   7. 配置 c_cc[VMIN/VTIME]: 非阻塞读取, 等待 0.1 秒
 *   8. tcsetattr() 应用配置, tcflush() 清空缓冲
 */
serial_handle_t serial_open(const char *portName, int baudRate) {
    if (!portName) return SERIAL_INVALID_HANDLE;

    // 步骤 1：非阻塞打开（不等待 DCD 信号，不成为控制终端）
    int fd = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return SERIAL_INVALID_HANDLE;
    }

    // 步骤 2：获取当前串口属性
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return SERIAL_INVALID_HANDLE;
    }

    // 步骤 3：设置输入/输出波特率
    cfsetospeed(&tty, baud_to_speed(baudRate));
    cfsetispeed(&tty, baud_to_speed(baudRate));

    // 步骤 4：控制模式 — 8N1, 本地连接, 接收使能, 无硬件流控
    tty.c_cflag |= (CLOCAL | CREAD);   /* 忽略调制解调器控制线，使能接收 */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;                /* 8 数据位 */
    tty.c_cflag &= ~PARENB;            /* 无校验 */
    tty.c_cflag &= ~CSTOPB;            /* 1 停止位 */
    tty.c_cflag &= ~CRTSCTS;           /* 禁用硬件流控 (RTS/CTS) */

    // 步骤 5：本地模式 — 原始模式（不处理特殊字符）
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* 原始模式 */

    // 步骤 6：输入模式 — 不做任何字符转换和流控
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);          /* 禁用软件流控 (XON/XOFF) */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // 步骤 7：输出模式 — 原始输出
    tty.c_oflag &= ~OPOST;

    // 步骤 8：非阻塞读取配置（至少 0 字节，等待 0.1 秒）
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return SERIAL_INVALID_HANDLE;
    }

    // 清空收发缓冲中残留的数据
    tcflush(fd, TCIOFLUSH);

    return fd;
}

/* 关闭文件描述符 */
void serial_close(serial_handle_t handle) {
    if (handle >= 0) {
        close(handle);
    }
}

/* 判断文件描述符是否有效 */
int serial_is_open(serial_handle_t handle) {
    return handle >= 0;
}

/*
 * 非阻塞读取：
 *   - 有数据时返回读取字节数
 *   - 无数据时（EAGAIN/EWOULDBLOCK）返回 0
 *   - 错误时返回 -2
 */
int serial_read(serial_handle_t handle, char *buf, int len) {
    if (handle < 0 || !buf || len <= 0) return -1;

    ssize_t n = read(handle, buf, (size_t)len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* 无数据可读，非错误 */
        }
        return -2;
    }
    return (int)n;
}

/* 阻塞写入 */
int serial_write(serial_handle_t handle, const char *buf, int len) {
    if (handle < 0 || !buf || len <= 0) return -1;

    ssize_t n = write(handle, buf, (size_t)len);
    if (n < 0) {
        return -2;
    }
    return (int)n;
}

/* 清空接收缓冲（丢弃未读数据） */
void serial_flush_rx(serial_handle_t handle) {
    if (handle >= 0) {
        tcflush(handle, TCIFLUSH);
    }
}

#endif
