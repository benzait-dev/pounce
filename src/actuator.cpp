#include "actuator.h"

#include <QThread>

#include "atspi.h"

QString Actuator::modeName(Mode mode) {
    switch (mode) {
    case Mode::Right: return "right";
    case Mode::Double: return "double";
    case Mode::Middle: return "middle";
    case Mode::Hover: return "hover";
    case Mode::Focus: return "focus";
    default: return "click";
    }
}

bool Actuator::parseMode(const QString &name, Mode *mode) {
    static const QList<std::pair<QString, Mode>> names{{"click", Mode::Primary}, {"right", Mode::Right},
                                                       {"double", Mode::Double}, {"middle", Mode::Middle},
                                                       {"hover", Mode::Hover},   {"focus", Mode::Focus}};
    for (const auto &[n, m] : names)
        if (n == name) {
            *mode = m;
            return true;
        }
    return false;
}

Actuator::Outcome Actuator::perform(const Element &element, Mode mode, const Config &config, const HyprState &state) {
    // The element's window takes focus first, as a click there would give
    // it, so keys typed next go where the person expects.
    if (!element.windowAddress.isEmpty() && element.windowAddress != state.activeAddress) {
        Hyprland::focusWindow(element.windowAddress);
        QThread::msleep(30);
    }

    const ObjRef ref{element.service, QDBusObjectPath(element.path)};
    const bool actionsAllowed = config.activation == Config::Activation::Auto;

    if (mode == Mode::Primary && actionsAllowed) {
        const int index = Atspi::primaryAction(element.actions, element.editable);
        if (index >= 0 && Atspi::doAction(ref, index))
            return {true, "action:" + element.actions.at(index), {}};
        if (element.editable && Atspi::grabFocus(ref))
            return {true, "focus", {}};
    }
    if (mode == Mode::Right && actionsAllowed) {
        const int index = Atspi::contextMenuAction(element.actions);
        if (index >= 0 && Atspi::doAction(ref, index))
            return {true, "action:" + element.actions.at(index), {}};
    }
    if (mode == Mode::Focus) {
        if (Atspi::grabFocus(ref))
            return {true, "focus", {}};
        mode = Mode::Primary;
    }
    return pointer(element, mode, config, state);
}

Actuator::Outcome Actuator::pointer(const Element &element, Mode mode, const Config &config, const HyprState &state) {
    QString error;
    if (!m_pointer.connect(&error))
        return {false, "pointer", error};
    const QRectF layout = state.layoutBounds();
    const QPointF target = element.center();
    const QPointF original = config.restorePointer && mode != Mode::Hover ? Hyprland::cursor() : QPointF();

    bool ok = false;
    switch (mode) {
    case Mode::Hover: ok = m_pointer.moveTo(target, layout); break;
    case Mode::Right: ok = m_pointer.click(target, layout, Pointer::Button::Right); break;
    case Mode::Middle: ok = m_pointer.click(target, layout, Pointer::Button::Middle); break;
    case Mode::Double: ok = m_pointer.click(target, layout, Pointer::Button::Left, 2); break;
    default: ok = m_pointer.click(target, layout, Pointer::Button::Left); break;
    }
    // Right-clicks open menus that close when the pointer leaves them, so
    // those keep the pointer where it went.
    if (ok && config.restorePointer && mode != Mode::Hover && mode != Mode::Right) {
        QThread::msleep(30);
        m_pointer.moveTo(original, layout);
    }
    return {ok, "pointer", ok ? QString() : QStringLiteral("The virtual pointer request failed.")};
}
