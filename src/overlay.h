#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>

class QQmlEngine;
class QScreen;
class Controller;

// One transparent, full-screen layer-shell surface per monitor, above
// everything including fullscreen windows. The one on the focused monitor
// holds the keyboard and the search panel; the rest only draw hints, so
// "Screen" scope can label windows on every display at once.
class OverlayManager : public QObject {
    Q_OBJECT

public:
    OverlayManager(QQmlEngine *engine, Controller *controller, QObject *parent = nullptr);

    QString error() const { return m_error; }

private:
    void showAll();
    void hideAll();
    QQuickWindow *windowFor(QScreen *screen);
    void screenRemoved(QScreen *screen);

    QQmlEngine *m_engine;
    Controller *m_controller;
    QHash<QScreen *, QPointer<QQuickWindow>> m_windows;
    QString m_error;
};
