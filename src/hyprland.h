#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

// What pounce needs to know about Hyprland: where every window and monitor
// is, which window has focus, and a way to focus another one. It talks to
// Hyprland's command socket directly rather than starting hyprctl, which
// keeps opening the overlay well under a frame's worth of time.
struct HyprWindow {
    QString address;
    QString cls;
    QString title;
    int pid = 0;
    QRectF rect;
    int workspace = 0;
    int monitor = -1;
    bool mapped = true;
    bool hidden = false;
    bool xwayland = false;
    int focusHistory = 0;
};

struct HyprMonitor {
    int id = 0;
    QString name;
    QRectF rect;        // Logical layout coordinates.
    double scale = 1.0;
    bool focused = false;
    int activeWorkspace = 0;
    int specialWorkspace = 0;
};

struct HyprState {
    QList<HyprWindow> windows;
    QList<HyprMonitor> monitors;
    QString activeAddress;

    const HyprWindow *active() const;
    const HyprMonitor *focusedMonitor() const;
    const HyprMonitor *monitorAt(const QPointF &point) const;
    const HyprMonitor *monitorFor(const HyprWindow &window) const;

    // Windows someone can see right now: on a shown workspace (including an
    // open special workspace) and at least partly on its monitor. Scrolling
    // layouts park windows off to the side; those don't count.
    QList<HyprWindow> visibleWindows() const;

    // The bounding box of every monitor, which is the space the virtual
    // pointer's absolute coordinates are measured against.
    QRectF layoutBounds() const;

    static QList<HyprWindow> parseWindows(const QJsonArray &array);
    static QList<HyprMonitor> parseMonitors(const QJsonArray &array);
};

class Hyprland {
public:
    static bool available();

    // Sends one request on the command socket and returns the reply, or an
    // empty array when Hyprland isn't reachable.
    static QByteArray request(const QByteArray &command, int timeoutMs = 500);

    static HyprState state();
    static QPointF cursor();

    static bool focusWindow(const QString &address);
};
