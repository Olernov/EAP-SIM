
TARGET = EAP-SIM-GW
CONFIG   += console
CONFIG   -= app_bundle


INCLUDEPATH += "C:/DSI/INC" \
    "c:/Projects/PolicyServer/utils/src/utils/pspacket" \
    "c:/Projects/PolicyServer/utils/src/utils"
DEPENDPATH += "C:/DSI/32"

LIBS += "C:/DSI/LIB32/gctlib.lib" \
	-lws2_32
#	-lgctlib

HEADERS += \
    mtu.h \
    pspacket.h


SOURCES += \
    c:/Projects/PolicyServer/utils/src/utils/pspacket/pspacket.cpp \
    mtu_main.cpp \
    mtu_fmt.cpp \
    mtu.cpp


QMAKE_LFLAGS = -static -static-libgcc
CONFIG += static
DEFINES += STATIC
message("~~~ static build ~~~")

OTHER_FILES += \
    ToDo.txt



