# 无人机端边缘计算组件调试界面 — 项目讲解

## 1. 项目概述

本程序是一个基于 **Qt 5 (C++ / qmake)** 的桌面调试工具，用于实时监视无人机搭载的边缘计算组件数据。

界面采用 **2×2 四象限布局**：

```
┌──────────────────┬──────────────────┐
│  卫星导航 (GNSS)  │  惯性导航 (INS)   │
│  u-blox NEO-M8P  │  模拟数据         │
├──────────────────┼──────────────────┤
│  图像第1路        │  图像第2路        │
│  模拟视频         │  模拟视频         │
└──────────────────┴──────────────────┘
```

每个象限有独立的**开关按钮**和**状态指示**（绿色=运行中，灰色=已停止）。

---

## 2. 目录结构

```
QTdesktop/
├── main.cpp                  # 程序入口
├── mainwindow.h              # 主窗口声明
├── mainwindow.cpp            # 主窗口实现（UI逻辑、模块管理）
├── mainwindow.ui             # Qt Designer UI 布局文件
├── UavEdgeMonitor.pro        # qmake 工程文件
│
├── src/
│   ├── api/                  # ══════ C API 头文件声明层 ══════
│   │   └── signal_edge_ui_api.h      # 7 个 C API 函数声明（纯头文件）
│   │
│   └── gnss/                 # ══════ GNSS 业务层（C++/Qt） ══════
│       ├── gnss_types.h              # 数据类型定义（GnssFix、FixType、NeoM8pConfig）
│       ├── nmea_parser.h / .cpp      # NMEA 0183 协议解析器
│       └── neo_m8p_receiver.h / .cpp # NEO-M8P 接收机封装（定时轮询）
│
GNSSdll/                      # ══════ 独立 DLL/SO 构建版本 ══════
├── serial_port.h / .c        # 跨平台串口驱动
├── config_file.h / .c        # 配置文件读取（库内部私有，不暴露为 API）
├── signaledge_ui_api.h       # C API 头文件（与 QTdesktop 同步）
├── signaledge_ui_api.cpp     # C API 实现（7 个公开函数）
└── signalledge_ui_api.pro    # 独立构建的 qmake 工程文件
```

---

## 3. 代码分层架构

```
┌─────────────────────────────────────────────┐
│              UI 层 (Qt C++)                 │
│  mainwindow.cpp — 按钮事件、定时器、显示更新  │
│  ├─ config.txt 写入 (QFile + QTextStream)   │
│  └─ "保存配置"按钮 → 编辑 config.txt         │
├─────────────────────────────────────────────┤
│           GNSS 业务层 (Qt C++)               │
│  neo_m8p_receiver.cpp — 定时轮询、缓冲分帧    │
│  nmea_parser.cpp — NMEA 协议解析、校验和验证  │
│  gnss_types.h — FixType/GnssFix 数据结构     │
├─────────────────────────────────────────────┤
│         C API 头文件（声明层）                │
│  signal_edge_ui_api.h — 7 个 API 函数声明     │
│     │                                       │
│     │ 动态库边界（DLL/SO 导入）               │
│     ▼                                       │
├─────────────────────────────────────────────┤
│         动态库 GNSSdll（独立进程）            │
│  编译为 libsignaledge_ui_api.so / .dll      │
│  ├─ signaledge_ui_api.cpp — 连接/采集实现    │
│  │    ZD_ServerCreate() 内部读取 config.txt   │
│  ├─ serial_port.c — 跨平台串口驱动           │
│  └─ config_file.c — 配置文件读取（私有）     │
├─────────────────────────────────────────────┤
│         硬件 / 操作系统 / 配置文件            │
│  Windows: COM1-COM12 (CreateFile)            │
│  Linux:   /dev/ttyTHS1, /dev/ttyUSB0 (termios) │
│  config.txt — 可执行文件同目录                │
└─────────────────────────────────────────────┘
```

### 3.1 编译模式

**动态链接**：C API 编译为独立的共享库（动态库），Qt 主程序在运行时加载。

```
构建步骤：
  1. GNSSdll/ → 编译 → libsignaledge_ui_api.so（Linux）或 .dll（Windows）
  2. QTdesktop/ → 编译 → 链接 libsignaledge_ui_api
```

| 构建目标 | 定义宏 | SIGNAL_EDGE_UI_API 展开 |
|----------|--------|--------------------------|
| GNSSdll（构建库） | `SIGNAL_EDGE_UI_API_EXPORTS` | `__declspec(dllexport)` (Win) / `visibility("default")` (Linux) |
| QTdesktop（使用库） | 无 | `__declspec(dllimport)` (Win) / `visibility("default")` (Linux) |

`UavEdgeMonitor.pro` 中通过 `LIBS += -L$$PWD/../GNSSdll -lsignaledge_ui_api` 指定动态库路径。

---

## 4. 各模块详解

### 4.1 C API 层 (`src/api/`)

#### 4.1.1 serial_port.c — 跨平台串口抽象

| 平台 | 实现方式 | 关键 API |
|------|----------|----------|
| Windows | `CreateFile("\\.\COM6")` + `DCB` + `COMMTIMEOUTS` | ReadFile/WriteFile |
| Linux | `open("/dev/ttyTHS1")` + `termios` (原始模式) | read/write |

**非阻塞读取策略**：
- Windows: `ReadIntervalTimeout = MAXDWORD`，立即返回已有数据
- Linux: `VMIN = 0, VTIME = 1`，等待 0.1 秒后返回

#### 4.1.2 signal_edge_ui_api.h — 统一 C API（仅头文件声明）

**动态库仅对外暴露 7 个函数**，不包含配置文件管理：

| 函数 | 功能 |
|------|------|
| `ZD_GetAPIVersion()` | 返回版本字符串 `"1.0.1"` |
| `ZD_GetLastError()` | 返回最近一次错误描述 |
| `ZD_ServerCreate(device, ip)` | 从 config.txt 读取串口参数 → 打开连接 |
| `ZD_ServerDestroy()` | 关闭串口，释放资源 |
| `ZD_GetGnssData(ch, buf, len, info)` | 从串口读取 GNSS 原始字节 |
| `ZD_GetInsData(ch, buf, len, info)` | 从串口读取 INS 原始字节 |
| `ZD_GetImageData(ch, buf, len, info)` | 从串口读取图像原始字节 |

`ZD_ServerCreate` **签名（固定，不可修改）**：
```c
int ZD_ServerCreate(char *device, char *ip);
```
内部流程：参数校验 → 从 `config.txt` 读取 `COM_PORT`/`BAUD_RATE` → 解析波特率 → `serial_open()` → 返回 0（成功）或负数错误码。

---

### 4.2 GNSS 业务层 (`src/gnss/`)

#### 4.2.1 gnss_types.h — 数据类型

```cpp
enum class FixType {
    Invalid      = 0,   // 无效
    Single       = 1,   // 单点定位
    Differential = 2,   // 差分定位
    FixedRtk     = 4,   // 固定解 RTK（厘米级）
    FloatRtk     = 5    // 浮点解 RTK（分米级）
};

struct GnssFix {
    bool valid;              // 定位有效性
    FixType fixType;         // 定位解类型
    QDateTime timestampUtc;  // UTC 时间
    double latitudeDeg;      // 纬度（度）
    double longitudeDeg;     // 经度（度）
    double altitudeM;        // 海拔（米）
    double speedMps;         // 地面速率（m/s）
    double courseDeg;        // 航向（度）
    int satellites;          // 卫星数量
    double hdop;             // 水平精度因子
    QString sourceSentence;  // 原始 NMEA 语句
};

struct NeoM8pConfig {
    QString device       = "SRP-Jeston"; // 设备型号
    QString ip           = "Localhost";  // 载荷 IP
    int channelId        = 0;            // 通道编号
    int updateRateHz     = 5;            // 轮询频率
    // 串口参数由 ZD_ServerCreate 内部从 config.txt 读取，不在此维护
};
```

#### 4.2.2 nmea_parser.cpp — NMEA 0183 协议解析

NMEA 0183 是美国国家海洋电子协会（NMEA）制定的 GPS 接收机数据输出标准。

**报文格式**：`$TALKER,SENTENCE,FIELD1,FIELD2,...*CK\r\n`

| 字段 | 说明 |
|------|------|
| `$` | 起始符 |
| `TALKER` | 设备标识（GP=GPS, GN=多系统, GL=GLONASS, BD=北斗） |
| `CK` | 校验和：`$` 到 `*` 之间字符的逐字节 XOR 值（2 位十六进制） |

**支持两种报文**：

| 报文 | 字段 | 提供信息 |
|------|------|----------|
| **GGA** | UTC, 纬度, N/S, 经度, E/W, 质量, 卫星数, HDOP, 高度, ... | 位置、高度、精度 |
| **RMC** | UTC, 状态, 纬度, N/S, 经度, E/W, 速度, 航向, 日期 | 速度、航向、日期 |

**关键设计**：GGA 不含日期字段，解析器内部维护 `m_lastDate`（来自最近 RMC），GGA 解析时以此补充完整时间戳。

**经纬度解析**：NMEA 格式 `DDDMM.mmmm` → 十进制度数 `DDD + MM.mmmm/60`

#### 4.2.3 neo_m8p_receiver.cpp — NEO-M8P 接收机封装

**完整数据链路**：

```
串口硬件
  │ NMEA 字节流
  ▼
m_pollTimer (定时器, 默认 5Hz = 200ms)
  │ timeout 信号
  ▼
pollGnssData()
  ├─ ZD_GetGnssData() → 非阻塞读取原始字节
  ├─ 追加到 m_rxBuffer (环形缓冲)
  └─ 从缓冲提取完整 NMEA 行 ($ 开头, \n 结尾)
       │
       ▼
consumeNmeaLine()
  ├─ emit rawSentenceReceived()  → 原始报文（调试用）
  └─ m_parser.parseSentence()
       └─ emit fixUpdated()      → 定位结果（UI 刷新）
```

**缓冲分帧逻辑**：单次 `read()` 可能只返回不完整数据（如半条 NMEA 报文）。`m_rxBuffer` 跨多次轮询累积数据，遇到 `\n` 才切割出完整句子送入解析器。

---

### 4.3 主界面 (`mainwindow.cpp`)

#### 4.3.1 模块管理

4 个布尔标记控制模块启停：

| 标记 | 对应控件 | 数据来源 |
|------|----------|----------|
| `m_gnssRunning` | 卫星导航面板 | NeoM8pReceiver 信号驱动 |
| `m_insRunning` | 惯性导航面板 | 定时器 + 三角函数模拟 |
| `m_image1Running` | 图像第1路 | 定时器 + QPainter 绘制 |
| `m_image2Running` | 图像第2路 | 定时器 + QPainter 绘制 |

#### 4.3.2 定时器智能管理

两个定时器按需启动，所有模块停止时自动关闭以节省 CPU：

```cpp
void updateTimers() {
    // 传感器定时器：GNSS 或 INS 任一运行
    (m_gnssRunning || m_insRunning) ? start : stop;  // 500ms

    // 图像定时器：图像1 或 图像2 任一运行
    (m_image1Running || m_image2Running) ? start : stop;  // 40ms
}
```

#### 4.3.3 数据流

```
┌──────────────────────────────────────────────────┐
│  GNSS（真实数据）：                                │
│  NeoM8pReceiver 内部定时器轮询                     │
│    → fixUpdated 信号 → onGnssFixUpdated() → UI    │
│                                                   │
│  INS（模拟数据）：                                  │
│  m_sensorTimer (500ms) → updateSensorData()       │
│    → 三角函数生成  → UI                            │
│                                                   │
│  图像（模拟视频）：                                  │
│  m_imageTimer (40ms) → updateImageData()           │
│    → drawImage() → QPainter → QPixmap → UI        │
└──────────────────────────────────────────────────┘
```

#### 4.3.4 配置文件集成 (`config.txt`)

配置文件 `config.txt` 存放在可执行文件同目录。**写入**由 Qt 端通过 `QFile + QTextStream` 完成，**读取**由动态库的 `ZD_ServerCreate` 内部通过 `config_read_str` 完成。

```txt
# GNSS 串口配置
COM_PORT=/dev/ttyTHS1
BAUD_RATE=115200
```

```cpp
// Qt 端：启动时加载 config.txt → 恢复 UI 控件
loadGnssConfig()
  → readConfigFile(comPort, baudRate)
    → QFile::open + QTextStream::readLine → 解析 KEY=VALUE
    → comboGnssCom->setCurrentText(comPort)
    → comboGnssBaud->setCurrentText(QString::number(baudRate))

// Qt 端：保存按钮 → 写入 config.txt
on_btnSaveGnssConfig_clicked()
  → saveGnssConfig()
    → QFile::open(WriteOnly) + QTextStream
    → out << "COM_PORT=" << comboGnssCom->currentText()
    → out << "BAUD_RATE=" << comboGnssBaud->currentText()

// 动态库端：ZD_ServerCreate 内部读取 config.txt
ZD_ServerCreate(device, ip)
  → config_read_str(NULL, "COM_PORT", comPort)
  → config_read_str(NULL, "BAUD_RATE", baudStr)
  → strtol(baudStr) → serial_open(comPort, baudRate)

---

## 5. 关键设计决策

| 决策 | 说明 |
|------|------|
| **C API 独立动态库** | C API 编译为独立 .so/.dll，Qt 主程序通过 `LIBS` 链接。接口变更只需替换动态库，无需重新编译主程序 |
| **非阻塞串口** | 避免 UI 线程被串口读取阻塞，配合定时器轮询实现异步数据流 |
| **环形缓冲分帧** | 处理串口字节流的不确定性，跨多次读取累积拼接完整 NMEA 行 |
| **信号驱动 UI** | GNSS 数据通过 Qt 信号异步更新 UI，不污染 UI 线程 |
| **定时器按需启停** | 无模块运行时自动停止定时器，Jetson 嵌入式环境节省 CPU |
| **配置文件由 Qt 端管理** | `config.txt` 由 UI 层通过 `QFile` 直接读写，不经过动态库。动态库只接收调用方传入的串口参数 |

---

## 6. 编译与运行

### 6.1 依赖

| 组件 | 版本要求 |
|------|----------|
| Qt | 5.12+ |
| 模块 | `core gui widgets` |
| 编译器 | MSVC 2019+ (Windows) / GCC 7+ (Linux) |

### 6.2 编译

```bash
# 1. 先编译动态库
cd GNSSdll
qmake signaledge_ui_api.pro
make -j$(nproc)          # Linux
# 或
nmake                     # Windows (MSVC)

# 2. 再编译主程序（链接动态库）
cd ../QTdesktop
qmake UavEdgeMonitor.pro
make -j$(nproc)          # Linux
# 或
nmake                     # Windows (MSVC)
```

### 6.3 串口权限（Linux/Jetson）

```bash
sudo usermod -a -G dialout $USER
# 重新登录后生效
```

### 6.4 配置文件

首次运行前在可执行文件同目录创建 `config.txt`：

```txt
COM_PORT=/dev/ttyTHS1
BAUD_RATE=115200
```

或通过 UI 界面的串口下拉框 + "保存配置" 按钮自动生成。

格式说明：
- 支持 `#` 和 `;` 开头的注释行
- 支持 `KEY=VALUE` 格式（等号前后空格自动去除）
- 大小写不敏感
