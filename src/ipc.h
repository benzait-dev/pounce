#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

// pounce runs once, resident, so the overlay opens instantly; every later
// `pounce` just tells it what to do over a socket in $XDG_RUNTIME_DIR and
// exits.
namespace Ipc {

QString socketPath();

// Sends one command ("toggle", "toggle hints", "quit") to the running
// instance. False when none is running.
bool send(const QByteArray &command, QByteArray *reply = nullptr, int timeoutMs = 400);

}

class IpcServer : public QObject {
    Q_OBJECT

public:
    explicit IpcServer(QObject *parent = nullptr);
    bool listen(QString *error);

signals:
    void command(const QByteArray &command);

private:
    QLocalServer m_server;
};
