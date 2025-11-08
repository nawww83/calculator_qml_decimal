CONFIG -= qt

CONFIG += use_simd

TEMPLATE = lib
DEFINES += CALCULUS_LIBRARY

QMAKE_CXXFLAGS += -msse4.1

TARGET = calculus

CONFIG += c++20

include(..\config.pri)

QMAKE_CXXFLAGS += -fwrapv

SOURCES += \
    calculus.cpp \
    u128_utils.cpp

HEADERS += \
    calculus_global.h \
    calculus.h \
    decimal.h \
    lfsr.h \
    random_gen.h \
    sign.h \
    singular.h \
    i128.hpp \
    u128.hpp \
    ulow.hpp \
    defines.h \
    u128_utils.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
