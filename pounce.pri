# Sources shared by the app and the tests.

POUNCE_ROOT = $$PWD

INCLUDEPATH += $$POUNCE_ROOT/src

HEADERS += \
    $$POUNCE_ROOT/src/actuator.h \
    $$POUNCE_ROOT/src/atspi.h \
    $$POUNCE_ROOT/src/cli.h \
    $$POUNCE_ROOT/src/config.h \
    $$POUNCE_ROOT/src/controller.h \
    $$POUNCE_ROOT/src/element.h \
    $$POUNCE_ROOT/src/geometry.h \
    $$POUNCE_ROOT/src/history.h \
    $$POUNCE_ROOT/src/hyprland.h \
    $$POUNCE_ROOT/src/ipc.h \
    $$POUNCE_ROOT/src/matcher.h \
    $$POUNCE_ROOT/src/overlay.h \
    $$POUNCE_ROOT/src/pointer.h \
    $$POUNCE_ROOT/src/ranker.h \
    $$POUNCE_ROOT/src/resultsmodel.h \
    $$POUNCE_ROOT/src/roles.h \
    $$POUNCE_ROOT/src/scanner.h \
    $$POUNCE_ROOT/src/theme.h \
    $$POUNCE_ROOT/src/translator.h

SOURCES += \
    $$POUNCE_ROOT/src/actuator.cpp \
    $$POUNCE_ROOT/src/atspi.cpp \
    $$POUNCE_ROOT/src/cli.cpp \
    $$POUNCE_ROOT/src/config.cpp \
    $$POUNCE_ROOT/src/controller.cpp \
    $$POUNCE_ROOT/src/geometry.cpp \
    $$POUNCE_ROOT/src/history.cpp \
    $$POUNCE_ROOT/src/hyprland.cpp \
    $$POUNCE_ROOT/src/ipc.cpp \
    $$POUNCE_ROOT/src/matcher.cpp \
    $$POUNCE_ROOT/src/overlay.cpp \
    $$POUNCE_ROOT/src/pointer.cpp \
    $$POUNCE_ROOT/src/ranker.cpp \
    $$POUNCE_ROOT/src/resultsmodel.cpp \
    $$POUNCE_ROOT/src/roles.cpp \
    $$POUNCE_ROOT/src/scanner.cpp \
    $$POUNCE_ROOT/src/theme.cpp \
    $$POUNCE_ROOT/src/translator.cpp

# The virtual pointer protocol's client bindings, generated at build time.
WAYLAND_PROTOCOLS = $$POUNCE_ROOT/protocols/wlr-virtual-pointer-unstable-v1.xml

wayland_header.input = WAYLAND_PROTOCOLS
wayland_header.output = ${QMAKE_FILE_BASE}-client-protocol.h
wayland_header.commands = wayland-scanner client-header ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
wayland_header.variable_out = HEADERS
wayland_header.CONFIG += target_predeps no_link

wayland_code.input = WAYLAND_PROTOCOLS
wayland_code.output = ${QMAKE_FILE_BASE}-protocol.c
wayland_code.commands = wayland-scanner private-code ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
wayland_code.variable_out = SOURCES
wayland_code.depends = ${QMAKE_FILE_BASE}-client-protocol.h

QMAKE_EXTRA_COMPILERS += wayland_header wayland_code
INCLUDEPATH += $$OUT_PWD
