QT += quick

CONFIG += c++20
CONFIG += console

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

# В app_core.pro

# Указываем путь к сборке библиотеки относительно текущего проекта
# В subdirs проектах OUT_PWD обычно указывает на папку сборки конкретного подпроекта
buildCalculusDir = $$OUT_PWD/../calculus

win32:CONFIG(release, debug|release): LIBS += -L$$buildCalculusDir/release/ -lcalculus
else:win32:CONFIG(debug, debug|release): LIBS += -L$$buildCalculusDir/debug/ -lcalculusd

win32 {
    CONFIG(debug, debug|release) {
        MODE = debug
        DLL_NAME = calculusd.dll
    } else {
        MODE = release
        DLL_NAME = calculus.dll
    }

    # Путь откуда берем и куда кладем (обычно в папку с exe)
    SOURCE_DLL = $$buildCalculusDir/$$MODE/$$DLL_NAME
    DEST_DIR = $$OUT_PWD/$$MODE/

    SOURCE_DLL ~= s,/,\\,g
    DEST_DIR ~= s,/,\\,g

    # Копируем конкретный файл
    copyfiles.commands = $$quote(cmd /c copy /Y $${SOURCE_DLL} $${DEST_DIR})
}

QMAKE_EXTRA_TARGETS += copyfiles
POST_TARGETDEPS += copyfiles
