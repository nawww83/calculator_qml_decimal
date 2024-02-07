CONFIG -= qt

TEMPLATE = lib
DEFINES += CALCULUS_LIBRARY

CONFIG += c++17

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
