#include "hyprland.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QRegularExpression>
#include <unistd.h>

#include <algorithm>

namespace {

QString socketPath() {
    const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (signature.isEmpty())
        return {};
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        runtime = QStringLiteral("/run/user/") + QString::number(getuid());
    return runtime + "/hypr/" + signature + "/.socket.sock";
}

QRectF rectFrom(const QJsonObject &object) {
    const QJsonArray at = object.value("at").toArray();
    const QJsonArray size = object.value("size").toArray();
    return QRectF(at.at(0).toDouble(), at.at(1).toDouble(), size.at(0).toDouble(), size.at(1).toDouble());
}

}

const HyprWindow *HyprState::active() const {
    for (const auto &window : windows)
        if (window.address == activeAddress)
            return &window;
    return nullptr;
}

const HyprMonitor *HyprState::focusedMonitor() const {
    for (const auto &monitor : monitors)
        if (monitor.focused)
            return &monitor;
    return monitors.isEmpty() ? nullptr : &monitors.first();
}

const HyprMonitor *HyprState::monitorAt(const QPointF &point) const {
    for (const auto &monitor : monitors)
        if (monitor.rect.contains(point))
            return &monitor;
    return nullptr;
}

const HyprMonitor *HyprState::monitorFor(const HyprWindow &window) const {
    for (const auto &monitor : monitors)
        if (monitor.id == window.monitor)
            return &monitor;
    return monitorAt(window.rect.center());
}

QList<HyprWindow> HyprState::visibleWindows() const {
    QList<HyprWindow> visible;
    for (const auto &window : windows) {
        if (!window.mapped || window.hidden || window.rect.isEmpty())
            continue;
        const HyprMonitor *monitor = monitorFor(window);
        if (!monitor)
            continue;
        const bool shown = window.workspace == monitor->activeWorkspace
                        || (monitor->specialWorkspace != 0 && window.workspace == monitor->specialWorkspace);
        if (!shown || !window.rect.intersects(monitor->rect))
            continue;
        visible.append(window);
    }
    // Most recently focused first, so the active window leads the results.
    std::stable_sort(visible.begin(), visible.end(),
                     [](const HyprWindow &a, const HyprWindow &b) { return a.focusHistory < b.focusHistory; });
    return visible;
}

QRectF HyprState::layoutBounds() const {
    QRectF bounds;
    for (const auto &monitor : monitors)
        bounds = bounds.isNull() ? monitor.rect : bounds.united(monitor.rect);
    return bounds;
}

QList<HyprWindow> HyprState::parseWindows(const QJsonArray &array) {
    QList<HyprWindow> windows;
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        HyprWindow window;
        window.address = object.value("address").toString();
        window.cls = object.value("class").toString();
        window.title = object.value("title").toString();
        window.pid = object.value("pid").toInt();
        window.rect = rectFrom(object);
        window.workspace = object.value("workspace").toObject().value("id").toInt();
        window.monitor = object.value("monitor").toInt(-1);
        window.mapped = object.value("mapped").toBool(true);
        window.hidden = object.value("hidden").toBool(false);
        window.xwayland = object.value("xwayland").toBool(false);
        window.focusHistory = object.value("focusHistoryID").toInt(1000);
        windows.append(window);
    }
    return windows;
}

QList<HyprMonitor> HyprState::parseMonitors(const QJsonArray &array) {
    QList<HyprMonitor> monitors;
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        if (object.value("disabled").toBool(false))
            continue;
        HyprMonitor monitor;
        monitor.id = object.value("id").toInt();
        monitor.name = object.value("name").toString();
        monitor.scale = object.value("scale").toDouble(1.0);
        if (monitor.scale <= 0)
            monitor.scale = 1.0;
        // Width and height are in device pixels; a rotated monitor swaps them.
        double width = object.value("width").toDouble(), height = object.value("height").toDouble();
        if (object.value("transform").toInt() % 2 == 1)
            std::swap(width, height);
        monitor.rect = QRectF(object.value("x").toDouble(), object.value("y").toDouble(), width / monitor.scale,
                              height / monitor.scale);
        monitor.focused = object.value("focused").toBool();
        monitor.activeWorkspace = object.value("activeWorkspace").toObject().value("id").toInt();
        monitor.specialWorkspace = object.value("specialWorkspace").toObject().value("id").toInt();
        monitors.append(monitor);
    }
    return monitors;
}

bool Hyprland::available() {
    return !socketPath().isEmpty();
}

QByteArray Hyprland::request(const QByteArray &command, int timeoutMs) {
    const QString path = socketPath();
    if (path.isEmpty())
        return {};
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(timeoutMs))
        return {};
    socket.write(command);
    socket.flush();
    QByteArray reply;
    // Hyprland answers and closes the connection, so read until it does.
    while (socket.state() == QLocalSocket::ConnectedState && socket.waitForReadyRead(timeoutMs))
        reply += socket.readAll();
    reply += socket.readAll();
    return reply;
}

HyprState Hyprland::state() {
    HyprState state;
    state.windows = HyprState::parseWindows(QJsonDocument::fromJson(request("j/clients")).array());
    state.monitors = HyprState::parseMonitors(QJsonDocument::fromJson(request("j/monitors")).array());
    state.activeAddress = QJsonDocument::fromJson(request("j/activewindow")).object().value("address").toString();
    return state;
}

QPointF Hyprland::cursor() {
    const QJsonObject object = QJsonDocument::fromJson(request("j/cursorpos")).object();
    return QPointF(object.value("x").toDouble(), object.value("y").toDouble());
}

bool Hyprland::focusWindow(const QString &address) {
    static const QRegularExpression valid("^0x[0-9a-fA-F]+$");
    if (!valid.match(address).hasMatch())
        return false;
    // Hyprland 0.55+ configures and dispatches in Lua.
    const QByteArray reply =
        request("dispatch hl.dsp.focus({ window = \"address:" + address.toLatin1() + "\" })");
    return reply.trimmed() == "ok";
}
