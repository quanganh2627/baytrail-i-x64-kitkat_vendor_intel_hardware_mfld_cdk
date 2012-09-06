#-------------------------------------------------
#
# Project created by QtCreator
#
#-------------------------------------------------

QT -= core gui

TARGET = vpc
TEMPLATE = lib

DEFINES +=

SOURCES += \
     acoustic.cpp \
     ctl_vpc.cpp \
     bt.cpp \
     msic.cpp \
     volume_keys.cpp

HEADERS += \
    acoustic.h \
    bt.h \
    msic.h \
    volume_keys.h \
    vpc_hardware.h

INCLUDEPATH += simulation
DEPENDPATH += simulation

CONFIG(debug, debug|release) {
    DESTDIR = ../build/debug/core
} else {
    DESTDIR = ../build/release/core
}

LIBS += -L$$DESTDIR

OTHER_FILES += \
    Android.mk














