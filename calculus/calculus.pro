CONFIG -= qt

TEMPLATE = lib
DEFINES += CALCULUS_LIBRARY

TARGET = calculus

CONFIG += c++20

include(..\config.pri)

QMAKE_CXXFLAGS += -fwrapv

SOURCES += \
    calculus.cpp

HEADERS += \
    calculus_global.h \
    calculus.h \
    decimal.h \
    lfsr.h \
    random_gen.h \
    u128.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
