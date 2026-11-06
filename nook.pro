QT += core gui qml quick quickcontrols2 network multimedia

CONFIG += c++17 release link_pkgconfig
PKGCONFIG += sdl3
TARGET = nook
TEMPLATE = app

HEADERS += \
    src/artwork.h \
    src/gamepad.h \
    src/launcher.h \
    src/library.h \
    src/preferences.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/artwork.cpp \
    src/gamepad.cpp \
    src/launcher.cpp \
    src/library.cpp \
    src/preferences.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
