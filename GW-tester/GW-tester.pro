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

LIBS += -lws2_32


SOURCES += main.cpp \
    ../EAP-SIM-GW/PSPacket.cpp

HEADERS += \
    ../EAP-SIM-GW/CoACommon.h \
    ../EAP-SIM-GW/PSPacket.h

QMAKE_LFLAGS = -static -static-libgcc
CONFIG += static
DEFINES += STATIC
message("~~~ static build ~~~")

QMAKE_CXXFLAGS += -std=c11
