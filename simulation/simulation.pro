#-------------------------------------------------
#
# Project created by QtCreator 2011-12-07T18:29:27
#
#-------------------------------------------------

QT       -= core gui

TARGET = simulation
TEMPLATE = lib

DEFINES += ATMANAGER_LIBRARY

QMAKE_CXXFLAGS += -Wno-unused-result

SOURCES +=  cutils/sockets.c \
            cutils/properties.cpp

HEADERS +=  utils/Log.h \
            cutils/log.h \
            cutils/sockets.h \
            cutils/properties.h \
            stmd.h

INCLUDEPATH +=

CONFIG(debug, debug|release) {
    DESTDIR = ../build/debug
} else {
    DESTDIR = ../build/release
}

LIBS += -L$$DESTDIR
























