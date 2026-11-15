QT += core gui qml quick dbus network

CONFIG += c++17 release link_pkgconfig
PKGCONFIG += wayland-client
TARGET = pounce
TEMPLATE = app

INCLUDEPATH += /usr/include/LayerShellQt
LIBS += -lLayerShellQtInterface

include(pounce.pri)

SOURCES += src/main.cpp
RESOURCES += src/resources.qrc
