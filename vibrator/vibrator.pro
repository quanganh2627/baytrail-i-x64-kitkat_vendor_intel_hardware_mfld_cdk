#-------------------------------------------------
#
# Project created by QtCreator 2011-12-11T10:35:08
#
#-------------------------------------------------

QT       -= core gui

TARGET = vibrator
TEMPLATE = lib

QMAKE_CXXFLAGS += -Wno-unused-result

DEFINES += EVENTLISTENER_LIBRARY

SOURCES += Vibrator.cpp \
    vibrator_instance.cpp

HEADERS += Vibrator.h

INCLUDEPATH += ../utility/event-listener ../utility/property ../../PRIVATE/audiocomms/parameter-framework/parameter ../../../../bionic/libc/kernel/common ../../simulation

CONFIG(debug, debug|release) {
    DESTDIR = ../build/debug
} else {
    DESTDIR = ../build/release
}

LIBS += -L$$DESTDIR -lproperty -levent-listener -lsimulation

OTHER_FILES += \
    Android.mk






