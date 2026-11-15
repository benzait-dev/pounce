#include "ipc.h"

#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <unistd.h>

namespace Ipc {

QString socketPath() {
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        runtime = QDir::tempPath();
    // One instance per Wayland session, so a nested or second session gets
    // its own.
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY", "wayland-0");
    return runtime + "/pounce-" + QString::number(getuid()) + "-" + display + ".sock";
}

bool send(const QByteArray &command, QByteArray *reply, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(socketPath());
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(command + "\n");
    socket.flush();
    if (socket.waitForReadyRead(timeoutMs) && reply)
        *reply = socket.readAll().trimmed();
    return true;
}

}

IpcServer::IpcServer(QObject *parent) : QObject(parent) {
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                while (socket->canReadLine()) {
                    const QByteArray line = socket->readLine().trimmed();
                    socket->write("ok\n");
                    socket->flush();
                    emit command(line);
                }
            });
        }
    });
}

bool IpcServer::listen(QString *error) {
    const QString path = Ipc::socketPath();
    // A socket file left by a crash refuses new listeners; only remove it
    // once it's clear nothing answers on it.
    if (QFile::exists(path) && !Ipc::send("ping", nullptr, 200))
        QLocalServer::removeServer(path);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(path)) {
        if (error)
            *error = m_server.errorString();
        return false;
    }
    return true;
}
