#pragma once

#include <QString>

#include "config.h"
#include "element.h"
#include "hyprland.h"
#include "pointer.h"

// Does what was asked to an element. A plain activation goes through the
// element's own accessibility action when it has one: it works on windows
// that are partly covered, leaves the pointer where it was, and can't miss.
// Everything that only makes sense as a mouse gesture, and anything without
// an action, goes through the virtual pointer.
class Actuator {
public:
    enum class Mode { Primary, Right, Double, Middle, Hover, Focus };

    struct Outcome {
        bool ok = false;
        QString method; // "action:<name>", "focus", or "pointer"
        QString error;
    };

    Outcome perform(const Element &element, Mode mode, const Config &config, const HyprState &state);

    static QString modeName(Mode mode);
    static bool parseMode(const QString &name, Mode *mode);

private:
    Outcome pointer(const Element &element, Mode mode, const Config &config, const HyprState &state);

    Pointer m_pointer;
};
