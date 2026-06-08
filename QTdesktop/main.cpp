/*
 * main.cpp —— 无人机端边缘计算组件调试界面 程序入口
 *
 * 本程序是一个基于 Qt 5 的桌面调试工具，用于监视和测试
 * 无人机搭载的边缘计算组件（u-blox NEO-M8P GNSS、INS、双路图像）。
 *
 * 布局：2×2 四象限面板
 *   ┌──────────────┬──────────────┐
 *   │  卫星导航(GNSS)│ 惯性导航(INS) │
 *   ├──────────────┼──────────────┤
 *   │  图像第1路     │ 图像第2路     │
 *   └──────────────┴──────────────┘
 *
 * 编译：定义 SIGNAL_EDGE_UI_STATIC，API 源码静态链接进主程序
 */

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
