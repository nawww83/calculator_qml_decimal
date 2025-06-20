QT += quick

CONFIG += c++20
CONFIG += console
CONFIG += use_simd

QMAKE_CXXFLAGS += -msse4.1

TARGET = calculator

SOURCES += \
        AppCore.cpp \
        controller.cpp \
        main.cpp \

include(..\config.pri)

RESOURCES += qml.qrc

OTHER_FILES += qml/*.qml \
        qml/*.js

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH += $$PWD

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH = $$PWD

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    AppCore.h \
    controller.h \
    observers.h \
    stopper.h \
    types.h \
    worker.h

DISTFILES +=

INCLUDEPATH += ../calculus
DEPENDPATH += ../calculus

win32:RC_ICONS += favicon.ico

buildCalculusDir = $$OUT_PWD/../calculus
win32:CONFIG(release, debug|release): LIBS += -L$$buildCalculusDir/release/ -lcalculus
else:win32:CONFIG(debug, debug|release): LIBS += -L$$buildCalculusDir/debug/ -lcalculus

win32 {
    PWD_WIN = $$buildCalculusDir
    DESTDIR_WIN = $$OUT_PWD/release/
    PWD_WIN ~= s,/,\\,g
    DESTDIR_WIN ~= s,/,\\,g
    copyfiles.commands = $$quote(cmd /c xcopy /S /I /Y $${PWD_WIN}\\release\\calculus*.dll $${DESTDIR_WIN})
}

QMAKE_EXTRA_TARGETS += copyfiles
POST_TARGETDEPS += copyfiles
