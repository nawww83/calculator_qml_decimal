CONFIG -= qt

TEMPLATE = lib
DEFINES += CALCULUS_LIBRARY

TARGET = calculus

CONFIG += c++17

include(..\config.pri)

QMAKE_CXXFLAGS += -fwrapv

SOURCES += \
    calculus.cpp

HEADERS += \
    calculus_global.h \
    calculus.h \
    decimal.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
