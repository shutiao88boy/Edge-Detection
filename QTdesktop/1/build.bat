gcc -shared -o signal_edge_ui_api.dll signal_edge_ui_api.c -Wl,--out-implib=libsignal_edge_ui_api.a
gcc -o demo.exe demo.c -L. -lsignal_edge_ui_api
