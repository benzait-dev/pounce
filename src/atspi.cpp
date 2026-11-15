#include "atspi.h"

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>

QDBusArgument &operator<<(QDBusArgument &argument, const ObjRef &ref) {
    argument.beginStructure();
    argument << ref.service << ref.path;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ObjRef &ref) {
    argument.beginStructure();
    argument >> ref.service >> ref.path;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const AtspiRect &rect) {
    argument.beginStructure();
    argument << rect.x << rect.y << rect.width << rect.height;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, AtspiRect &rect) {
    argument.beginStructure();
    argument >> rect.x >> rect.y >> rect.width >> rect.height;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ActionInfo &action) {
    argument.beginStructure();
    argument << action.name << action.description << action.keybinding;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ActionInfo &action) {
    argument.beginStructure();
    argument >> action.name >> action.description >> action.keybinding;
    argument.endStructure();
    return argument;
}

namespace Atspi {

void registerTypes() {
    static bool done = false;
    if (done)
        return;
    done = true;
    qDBusRegisterMetaType<ObjRef>();
    qDBusRegisterMetaType<QList<ObjRef>>();
    qDBusRegisterMetaType<AtspiRect>();
    qDBusRegisterMetaType<ActionInfo>();
    qDBusRegisterMetaType<QList<ActionInfo>>();
}

bool isEnabled() {
    QDBusInterface status("org.a11y.Bus", "/org/a11y/bus", "org.a11y.Status", QDBusConnection::sessionBus());
    return status.isValid() && status.property("IsEnabled").toBool();
}

bool setEnabled(bool enabled, QString *error) {
    QDBusMessage message = QDBusMessage::createMethodCall("org.a11y.Bus", "/org/a11y/bus",
                                                          "org.freedesktop.DBus.Properties", "Set");
    message << QString("org.a11y.Status") << QString("IsEnabled") << QVariant::fromValue(QDBusVariant(enabled));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error)
            *error = reply.errorMessage();
        return false;
    }
    return true;
}

QDBusConnection bus(QString *error) {
    static const QString name = QStringLiteral("pounce-a11y");
    QDBusConnection existing(name);
    if (existing.isConnected())
        return existing;

    registerTypes();
    QDBusMessage message = QDBusMessage::createMethodCall("org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress");
    const QDBusReply<QString> address = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    if (!address.isValid() || address.value().isEmpty()) {
        if (error)
            *error = QStringLiteral("The accessibility bus isn't running (org.a11y.Bus: %1).")
                         .arg(address.error().message());
        return QDBusConnection(name);
    }
    QDBusConnection connection = QDBusConnection::connectToBus(address.value(), name);
    if (!connection.isConnected() && error)
        *error = QStringLiteral("Couldn't connect to the accessibility bus: %1").arg(connection.lastError().message());
    return connection;
}

QDBusPendingCall call(const ObjRef &ref, const char *interface, const char *method, const QVariantList &arguments,
                      int timeoutMs) {
    QDBusMessage message =
        QDBusMessage::createMethodCall(ref.service, ref.path.path(), QString::fromLatin1(interface), QString::fromLatin1(method));
    message.setArguments(arguments);
    return bus().asyncCall(message, timeoutMs);
}

QDBusPendingCall pidCall(const QString &service) {
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                          "org.freedesktop.DBus", "GetConnectionUnixProcessID");
    message << service;
    return bus().asyncCall(message, 1000);
}

bool doAction(const ObjRef &ref, int index, QString *error) {
    QDBusMessage message = QDBusMessage::createMethodCall(ref.service, ref.path.path(), ActionInterface, "DoAction");
    message << index;
    const QDBusReply<bool> reply = bus().call(message, QDBus::Block, 2000);
    if (!reply.isValid()) {
        if (error)
            *error = reply.error().message();
        return false;
    }
    return reply.value();
}

bool grabFocus(const ObjRef &ref, QString *error) {
    QDBusMessage message = QDBusMessage::createMethodCall(ref.service, ref.path.path(), ComponentInterface, "GrabFocus");
    const QDBusReply<bool> reply = bus().call(message, QDBus::Block, 2000);
    if (!reply.isValid()) {
        if (error)
            *error = reply.error().message();
        return false;
    }
    return reply.value();
}

int primaryAction(const QStringList &actions, bool editable) {
    // In order of preference. "activate" on a text field submits it in some
    // toolkits, so fields only take the click-like names.
    static const QStringList preferred{"click", "press", "jump", "activate", "open", "toggle", "dodefault", "expand or contract"};
    static const QStringList fieldPreferred{"click", "press"};
    const QStringList &order = editable ? fieldPreferred : preferred;
    for (const QString &want : order)
        for (int i = 0; i < actions.size(); ++i)
            if (actions[i].compare(want, Qt::CaseInsensitive) == 0)
                return i;
    return -1;
}

int contextMenuAction(const QStringList &actions) {
    static const QStringList preferred{"showcontextmenu", "showmenu", "menu", "popup"};
    for (const QString &want : preferred)
        for (int i = 0; i < actions.size(); ++i)
            if (actions[i].compare(want, Qt::CaseInsensitive) == 0)
                return i;
    return -1;
}

}
