CONFIG -= qt

TEMPLATE = lib
DEFINES += SIGNALEDGE_UI_API_LIBRARY
DEFINES += SIGNAL_EDGE_UI_API_EXPORTS

SOURCES += \
    serial_port.c \
    config_file.c \
    signaledge_ui_api.c

HEADERS += \
    serial_port.h \
    config_file.h \
    signaledge_ui_api_global.h \
    signaledge_ui_api.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
