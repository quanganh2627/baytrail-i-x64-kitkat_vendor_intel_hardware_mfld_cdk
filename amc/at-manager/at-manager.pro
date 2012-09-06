#-------------------------------------------------
#
# Project created by QtCreator 2011-12-07T18:29:27
#
#-------------------------------------------------

QT       -= core gui

TARGET = at-manager
TEMPLATE = lib

DEFINES +=  ATMANAGER_LIBRARY \
            REQUEST_CLEANUP=0x10

QMAKE_CXXFLAGS += -Wno-unused-result

SOURCES += ATCommand.cpp \
    ATManager.cpp \
    PeriodicATCommand.cpp \
    UnsollicitedATCommand.cpp

HEADERS += ATManager.h \
    PeriodicATCommand.h \
    UnsollicitedATCommand.h \
    ATCommand.h \
    EventNotifier.h

INCLUDEPATH += ../../simulation ../../utility/event-listener ../at-parser ../tty-handler ../../../mfld_cdk/utility/property
DEPENDPATH += ../../simulation ../../utility/event-listener ../at-parser ../tty-handler ../../../mfld_cdk/utility/property

CONFIG(debug, debug|release) {
    DESTDIR = ../../build/debug
} else {
    DESTDIR = ../../build/release
}

LIBS += -L$$DESTDIR -levent-listener -lat-parser -ltty-handler -lrt -lproperty -lsimulation
OTHER_FILES += \
    Android.mk \
























