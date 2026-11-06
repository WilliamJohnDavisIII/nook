QT += core gui network testlib
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += sdl3
TARGET = nook_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/artwork.h ../src/gamepad.h ../src/launcher.h ../src/library.h ../src/preferences.h ../src/theme.h ../src/translator.h
SOURCES += nook_tests.cpp ../src/artwork.cpp ../src/gamepad.cpp ../src/launcher.cpp ../src/library.cpp ../src/preferences.cpp ../src/theme.cpp ../src/translator.cpp
