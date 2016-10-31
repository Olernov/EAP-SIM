#-------------------------------------------------
#
# Project created by QtCreator 2013-12-11T11:57:21
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = GW-tester
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += "C:/DSI/INC" \
	"c:/Projects/PolicyServer/utils/src/utils/pspacket" \
	"c:/Projects/PolicyServer/utils/src/utils"

LIBS += -lws2_32


SOURCES += main.cpp \
	c:/Projects/PolicyServer/utils/src/utils/pspacket/pspacket.cpp \

HEADERS += \
	pspacket.h

QMAKE_LFLAGS = -static -static-libgcc
QMAKE_CXXFLAGS += -std=gnu++0x

CONFIG += static
DEFINES += STATIC WIN32
message("~~~ static build ~~~")


