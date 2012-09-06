#-------------------------------------------------
#
# Project created by QtCreator 2011-12-07T18:29:27
#
#-------------------------------------------------

QT       -= core gui

TARGET = audio-at-manager
TEMPLATE = lib

DEFINES += ATMANAGER_LIBRARY

QMAKE_CXXFLAGS += -Wno-unused-result

SOURCES += AudioATManager.cpp \
    ProgressUnsollicitedATCommand.cpp \
    XDRVIUnsollicitedATCommand.cpp \
    CallStatUnsollicitedATCommand.cpp \
    Tokenizer.cpp

HEADERS += AudioATManager.h \
    CallStatUnsollicitedATCommand.h \
    ProgressUnsollicitedATCommand.h \
    XDRVIUnsollicitedATCommand.h \
    AudioATModemTypes.h \
    ModemAudioEvent.h \
    Tokenizer.h

INCLUDEPATH += ../../simulation ../../utility/event-listener ../at-parser ../tty-handler ../at-manager
DEPENDPATH += ../../simulation ../../utility/event-listener ../at-parser ../tty-handler ../at-manager

CONFIG(debug, debug|release) {
    DESTDIR = ../../build/debug
} else {
    DESTDIR = ../../build/release
}

LIBS += -L$$DESTDIR -levent-listener -lat-parser -ltty-handler -lrt -lsimulation
OTHER_FILES += \
    Android.mk \
























