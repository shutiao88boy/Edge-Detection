QT += core gui widgets

CONFIG += c++17

TARGET = UavEdgeMonitor
TEMPLATE = app

# ═══════════════════════════════════════════════
# 动态库链接说明
# ═══════════════════════════════════════════════
# 本程序通过动态库（DLL/SO）调用 C API，不再静态编译 .c 源码。
# 构建步骤：
#   1. 先编译 GNSSdll/ 目录生成 libsignaledge_ui_api.so（或 .dll）
#   2. 再编译本目录，链接上述动态库
#
# 导入导出宏逻辑：
#   - 构建 DLL/SO 时定义 SIGNAL_EDGE_UI_API_EXPORTS → dllexport
#   - 使用 DLL/SO 时（本程序）不定义任何宏        → dllimport（Win）
#                                                     visibility("default")（Linux）

# ═══════════════════════════════════════════════
# GNSS 模块（C++，编译进主程序）
# ═══════════════════════════════════════════════
SOURCES += \
    $$PWD/src/gnss/nmea_parser.cpp \
    $$PWD/src/gnss/neo_m8p_receiver.cpp

HEADERS += \
    $$PWD/src/gnss/gnss_types.h \
    $$PWD/src/gnss/nmea_parser.h \
    $$PWD/src/gnss/neo_m8p_receiver.h

# ═══════════════════════════════════════════════
# C API 头文件（声明来自动态库的函数）
#   neo_m8p_receiver.cpp 内 extern "C" 引用
# ═══════════════════════════════════════════════
HEADERS += \
    $$PWD/src/api/signal_edge_ui_api.h

# ═══════════════════════════════════════════════
# 主程序（Qt C++）
# ═══════════════════════════════════════════════
SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp

HEADERS += \
    $$PWD/mainwindow.h

FORMS += \
    $$PWD/mainwindow.ui

# ═══════════════════════════════════════════════
# 全局配置
# ═══════════════════════════════════════════════
INCLUDEPATH += $$PWD/src/api $$PWD/src/gnss

# ═══════════════════════════════════════════════
# 链接动态库（GNSSdll）
#   - 先 cd GNSSdll && qmake && make 编译出动态库
#   - 或手动指定库路径
# ═══════════════════════════════════════════════
LIBS += -L$$PWD/../GNSSdll -lsignaledge_ui_api

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../build-signaledge_ui_api-Desktop_Qt_5_12_1_MinGW_64_bit-Debug/release/ -lsignaledge_ui_api
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build-signaledge_ui_api-Desktop_Qt_5_12_1_MinGW_64_bit-Debug/debug/ -lsignaledge_ui_api
else:unix: LIBS += -L$$PWD/../build-signaledge_ui_api-Desktop_Qt_5_12_1_MinGW_64_bit-Debug/ -lsignaledge_ui_api

INCLUDEPATH += $$PWD/../build-signaledge_ui_api-Desktop_Qt_5_12_1_MinGW_64_bit-Debug/debug
DEPENDPATH += $$PWD/../build-signaledge_ui_api-Desktop_Qt_5_12_1_MinGW_64_bit-Debug/debug
