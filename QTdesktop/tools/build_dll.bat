@echo off
REM ============================================================
REM 构建 signal_edge_ui_api.dll (独立动态库)
REM 用于部署到 Jetson Orin Nano 时的 DLL 构建
REM
REM Windows (MinGW):
REM   build_dll.bat
REM
REM Linux (Jetson, 需要交叉编译或直接在 Jetson 上编译):
REM   gcc -shared -fPIC -o libsignal_edge_ui_api.so ^
REM       src/api/serial_port.c ^
REM       src/api/signal_edge_ui_api.c ^
REM       -DSIGNAL_EDGE_UI_API_EXPORTS
REM ============================================================

echo === Building signal_edge_ui_api.dll ===

gcc -shared -o signal_edge_ui_api.dll ^
    src/api/serial_port.c ^
    src/api/signal_edge_ui_api.c ^
    -DSIGNAL_EDGE_UI_API_EXPORTS ^
    -Wl,--out-implib=libsignal_edge_ui_api.a

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] DLL built successfully: signal_edge_ui_api.dll
    echo [OK] Import library:          libsignal_edge_ui_api.a
) else (
    echo.
    echo [FAIL] DLL build failed!
    echo Please ensure MinGW gcc is installed and in PATH.
)
