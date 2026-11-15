#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct zwlr_virtual_pointer_manager_v1;
struct zwlr_virtual_pointer_v1;

// A virtual pointer through wlr-virtual-pointer-unstable-v1, which Hyprland
// implements. It is pounce's fallback for widgets that offer no accessibility
// action, and the way right, middle, and double clicks and hovering happen.
// It is its own Wayland connection, so it works the same from the overlay
// and from `pounce click` in a terminal, and needs no ydotool daemon or
// uinput permissions.
class Pointer {
public:
    enum class Button { Left, Right, Middle };

    Pointer();
    ~Pointer();
    Pointer(const Pointer &) = delete;
    Pointer &operator=(const Pointer &) = delete;

    bool connect(QString *error = nullptr);

    // `layout` is the bounding box of every monitor, which absolute motion is
    // measured against.
    bool moveTo(const QPointF &point, const QRectF &layout);
    bool click(const QPointF &point, const QRectF &layout, Button button = Button::Left, int count = 1);
    bool scroll(const QPointF &point, const QRectF &layout, double dx, double dy);

    // Every global the compositor advertises, for `pounce doctor`.
    static QStringList waylandGlobals(QString *error = nullptr);

    static int absoluteCoordinate(double value, double origin, double extent, int resolution);

    // Sub-pixel resolution for absolute motion: the extents sent are the
    // layout size times this.
    static constexpr int Resolution = 8;

private:
    void frame();
    bool flush();
    static quint32 now();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_seat *m_seat = nullptr;
    zwlr_virtual_pointer_manager_v1 *m_manager = nullptr;
    zwlr_virtual_pointer_v1 *m_pointer = nullptr;

    friend struct PointerRegistry;
};
