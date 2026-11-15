#include "pointer.h"

#include <QThread>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <wayland-client.h>

#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

namespace {

// linux/input-event-codes.h
constexpr quint32 BTN_LEFT = 0x110;
constexpr quint32 BTN_RIGHT = 0x111;
constexpr quint32 BTN_MIDDLE = 0x112;

quint32 code(Pointer::Button button) {
    switch (button) {
    case Pointer::Button::Right: return BTN_RIGHT;
    case Pointer::Button::Middle: return BTN_MIDDLE;
    default: return BTN_LEFT;
    }
}

}

struct PointerRegistry {
    static void global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
        auto *self = static_cast<Pointer *>(data);
        if (std::strcmp(interface, wl_seat_interface.name) == 0 && !self->m_seat)
            self->m_seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
        else if (std::strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0)
            self->m_manager = static_cast<zwlr_virtual_pointer_manager_v1 *>(
                wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, std::min(version, 2u)));
    }
    static void remove(void *, wl_registry *, uint32_t) {}
};

static const wl_registry_listener registryListener = {PointerRegistry::global, PointerRegistry::remove};

Pointer::Pointer() = default;

Pointer::~Pointer() {
    if (m_pointer)
        zwlr_virtual_pointer_v1_destroy(m_pointer);
    if (m_manager)
        zwlr_virtual_pointer_manager_v1_destroy(m_manager);
    if (m_seat)
        wl_seat_destroy(m_seat);
    if (m_registry)
        wl_registry_destroy(m_registry);
    if (m_display) {
        wl_display_flush(m_display);
        wl_display_disconnect(m_display);
    }
}

bool Pointer::connect(QString *error) {
    if (m_pointer)
        return true;
    if (!m_display)
        m_display = wl_display_connect(nullptr);
    if (!m_display) {
        if (error)
            *error = QStringLiteral("Couldn't connect to the Wayland display.");
        return false;
    }
    if (!m_registry) {
        m_registry = wl_display_get_registry(m_display);
        wl_registry_add_listener(m_registry, &registryListener, this);
        wl_display_roundtrip(m_display);
    }
    if (!m_manager) {
        if (error)
            *error = QStringLiteral("The compositor doesn't offer virtual pointers (zwlr_virtual_pointer_manager_v1).");
        return false;
    }
    m_pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(m_manager, m_seat);
    return flush();
}

quint32 Pointer::now() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return quint32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void Pointer::frame() {
    zwlr_virtual_pointer_v1_frame(m_pointer);
}

bool Pointer::flush() {
    return wl_display_roundtrip(m_display) >= 0;
}

int Pointer::absoluteCoordinate(double value, double origin, double extent, int resolution) {
    const double clamped = std::clamp(value - origin, 0.0, std::max(0.0, extent - 1.0 / resolution));
    return int(std::lround(clamped * resolution));
}

bool Pointer::moveTo(const QPointF &point, const QRectF &layout) {
    if (!connect() || layout.isEmpty())
        return false;
    const quint32 width = quint32(std::lround(layout.width() * Resolution));
    const quint32 height = quint32(std::lround(layout.height() * Resolution));
    zwlr_virtual_pointer_v1_motion_absolute(m_pointer, now(),
                                            absoluteCoordinate(point.x(), layout.x(), layout.width(), Resolution),
                                            absoluteCoordinate(point.y(), layout.y(), layout.height(), Resolution),
                                            width, height);
    frame();
    return flush();
}

bool Pointer::click(const QPointF &point, const QRectF &layout, Button button, int count) {
    if (!moveTo(point, layout))
        return false;
    // Let the window see the pointer arrive before the press, as it would
    // from a real mouse; some toolkits pick the press target on motion.
    QThread::msleep(25);
    const quint32 which = code(button);
    for (int i = 0; i < std::max(1, count); ++i) {
        zwlr_virtual_pointer_v1_button(m_pointer, now(), which, WL_POINTER_BUTTON_STATE_PRESSED);
        frame();
        if (!flush())
            return false;
        QThread::msleep(12);
        zwlr_virtual_pointer_v1_button(m_pointer, now(), which, WL_POINTER_BUTTON_STATE_RELEASED);
        frame();
        if (!flush())
            return false;
        if (i + 1 < count)
            QThread::msleep(40);
    }
    return true;
}

bool Pointer::scroll(const QPointF &point, const QRectF &layout, double dx, double dy) {
    if (!moveTo(point, layout))
        return false;
    zwlr_virtual_pointer_v1_axis_source(m_pointer, WL_POINTER_AXIS_SOURCE_WHEEL);
    if (dy != 0)
        zwlr_virtual_pointer_v1_axis_discrete(m_pointer, now(), WL_POINTER_AXIS_VERTICAL_SCROLL,
                                              wl_fixed_from_double(dy * 15), int(std::lround(dy)));
    if (dx != 0)
        zwlr_virtual_pointer_v1_axis_discrete(m_pointer, now(), WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                                              wl_fixed_from_double(dx * 15), int(std::lround(dx)));
    frame();
    return flush();
}

namespace {
void collectGlobal(void *data, wl_registry *, uint32_t, const char *interface, uint32_t) {
    static_cast<QStringList *>(data)->append(QString::fromLatin1(interface));
}
void ignoreRemoval(void *, wl_registry *, uint32_t) {}
const wl_registry_listener collectListener = {collectGlobal, ignoreRemoval};
}

QStringList Pointer::waylandGlobals(QString *error) {
    QStringList globals;
    wl_display *display = wl_display_connect(nullptr);
    if (!display) {
        if (error)
            *error = QStringLiteral("Couldn't connect to the Wayland display.");
        return globals;
    }
    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &collectListener, &globals);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return globals;
}
