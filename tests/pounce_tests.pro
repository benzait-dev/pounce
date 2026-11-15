QT += core gui qml quick dbus network testlib
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += wayland-client
TARGET = pounce_tests
TEMPLATE = app

INCLUDEPATH += /usr/include/LayerShellQt
LIBS += -lLayerShellQtInterface

include(../pounce.pri)

SOURCES += pounce_tests.cpp
RESOURCES += ../src/resources.qrc
