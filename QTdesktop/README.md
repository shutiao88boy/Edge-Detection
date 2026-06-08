# 无人机端边缘计算组件 Qt 调试界面

该工程是标准 Qt Widgets/qmake 工程，界面文件为 `mainwindow.ui`，可直接用 Qt Creator 打开编辑。

主要文件：

- `UavEdgeMonitor.pro`
- `main.cpp`
- `mainwindow.h`
- `mainwindow.cpp`
- `mainwindow.ui`

界面按示意图实现了 2×2 Qt Widgets 布局：

- 卫星导航：开关状态切换，模拟卫星数量、定位状态、经纬度、高度刷新。
- 惯性导航：开关状态切换，模拟陀螺仪、加速度、姿态/角度数据刷新。
- 图像第 1 路：开关状态切换，开启后显示模拟视频帧。
- 图像第 2 路：开关状态切换，开启后显示模拟视频帧。

## 编译运行方式一：Qt Creator

1. 打开 Qt Creator。
2. 选择 `打开项目`。
3. 打开 `UavEdgeMonitor.pro`。
4. 配置 Kit 后点击运行。

## 编译运行方式二：命令行

需要安装 Qt 5 或 Qt 6，并确保 `qmake` 可用。

```powershell
qmake UavEdgeMonitor.pro
nmake
.\release\UavEdgeMonitor.exe
```

如果使用 MinGW：

```powershell
qmake UavEdgeMonitor.pro
mingw32-make
.\release\UavEdgeMonitor.exe
```

## 后续接入真实功能

当前工程中的卫星导航、惯性导航、图像数据均为模拟数据。接入真实设备时，可替换：

- `updateSensorData()`：接入 GNSS 串口解析和 FSS-IMU618 SPI 解析结果。
- `updateVideoFrames()` / `ImageDisplay::nextFrame()`：接入 MIPI-CSI 相机或视频采集帧。
- 按钮槽函数：替换为真实设备初始化、启动采集、停止采集和资源释放流程。
