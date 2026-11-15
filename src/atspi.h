#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantList>

// The accessibility bus, spoken to directly over D-Bus. libatspi would block
// on every call; going through QtDBus lets the scanner keep hundreds of
// requests in flight at once, which is the difference between a tree walk
// that takes seconds and one that is done before the overlay finishes fading
// in.

struct ObjRef {
    QString service;
    QDBusObjectPath path;
    bool isNull() const { return service.isEmpty() || path.path().isEmpty() || path.path() == "/org/a11y/atspi/null"; }
};
Q_DECLARE_METATYPE(ObjRef)

struct AtspiRect {
    int x = 0, y = 0, width = 0, height = 0;
};
Q_DECLARE_METATYPE(AtspiRect)

struct ActionInfo {
    QString name, description, keybinding;
};
Q_DECLARE_METATYPE(ActionInfo)

QDBusArgument &operator<<(QDBusArgument &argument, const ObjRef &ref);
const QDBusArgument &operator>>(const QDBusArgument &argument, ObjRef &ref);
QDBusArgument &operator<<(QDBusArgument &argument, const AtspiRect &rect);
const QDBusArgument &operator>>(const QDBusArgument &argument, AtspiRect &rect);
QDBusArgument &operator<<(QDBusArgument &argument, const ActionInfo &action);
const QDBusArgument &operator>>(const QDBusArgument &argument, ActionInfo &action);

namespace Atspi {

constexpr auto AccessibleInterface = "org.a11y.atspi.Accessible";
constexpr auto ComponentInterface = "org.a11y.atspi.Component";
constexpr auto ActionInterface = "org.a11y.atspi.Action";
constexpr auto RegistryService = "org.a11y.atspi.Registry";
constexpr auto RootPath = "/org/a11y/atspi/accessible/root";

// AT-SPI coordinate types. Window-relative is the only one every Wayland
// toolkit fills in.
constexpr uint CoordWindow = 1;

void registerTypes();

// Whether toolkits are being told to expose accessibility at all
// (org.a11y.Status.IsEnabled on the session bus).
bool isEnabled();
bool setEnabled(bool enabled, QString *error = nullptr);

// The private accessibility bus, connected once and shared. Returns a
// disconnected connection and fills `error` when there is no bus.
QDBusConnection bus(QString *error = nullptr);

QDBusPendingCall call(const ObjRef &ref, const char *interface, const char *method,
                      const QVariantList &arguments = {}, int timeoutMs = 1500);

// The process behind a connection on the accessibility bus, cached because a
// connection never changes owner.
QDBusPendingCall pidCall(const QString &service);

// Presses an element without moving the pointer.
bool doAction(const ObjRef &ref, int index, QString *error = nullptr);
bool grabFocus(const ObjRef &ref, QString *error = nullptr);

// Picks the action that means "the main thing this does", by name, or -1.
// Toolkits spell it differently: GTK says "click", Qt "Press", Chromium
// "press", "jump", "activate", or "doDefault".
int primaryAction(const QStringList &actions, bool editable);
int contextMenuAction(const QStringList &actions);

// Chromium and Electron decide once, at launch, whether to build an
// accessibility tree. True when `pid` is one of them and it was started
// without --force-renderer-accessibility, so only quitting it and opening it
// again will help.
bool startedWithoutTree(int pid);

}
