#-------------------------------------------------
#
# Project created by QtCreator 2011-12-07T18:29:27
#
#-------------------------------------------------

QT       -= core gui

TARGET = libamc
TEMPLATE = lib

DEFINES += AMC_LIBRARY

QMAKE_CXXFLAGS += -Wno-unused-result

SOURCES += AudioModemControl_IFX_XMM6160.c \
    AmcConfDev.c \
    ATModemControl.cpp

HEADERS += ../simulation/utils/Log.h \
    AudioModemControl.h \
    ATmodemControl.h \
    ../simulation/cutils/sockets.h

INCLUDEPATH += ../../simulation  ../event-listener  ../at-manager ../../utility/event-listener
DEPENDPATH += ../../simulation  ../event-listener  ../at-manager ../../utility/event-listener

CONFIG(debug, debug|release) {
    DESTDIR = ../../build/debug
} else {
    DESTDIR = ../../build/release
}

LIBS += -L$$DESTDIR -lat-manager -levent-listener -lsimulation
OTHER_FILES += \
    Android.mk \
    ATmodemControl.old


























