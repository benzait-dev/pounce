#include "overlay.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScreen>

#include "controller.h"
#include "hyprland.h"

OverlayManager::OverlayManager(QQmlEngine *engine, Controller *controller, QObject *parent)
    : QObject(parent), m_engine(engine), m_controller(controller) {
    connect(controller, &Controller::openChanged, this, [this] {
        if (m_controller->isOpen())
            showAll();
        else
            hideAll();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &OverlayManager::screenRemoved);
}

QQuickWindow *OverlayManager::windowFor(QScreen *screen) {
    if (auto window = m_windows.value(screen))
        return window;

    QQmlComponent component(m_engine, QUrl("qrc:/Overlay.qml"));
    QObject *object = component.createWithInitialProperties({{"monitorName", screen->name()}});
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        m_error = component.errorString();
        qWarning("pounce: %s", qPrintable(m_error));
        delete object;
        return nullptr;
    }
    window->setScreen(screen);
    window->setColor(Qt::transparent);

    // Made a layer surface before it is ever shown, which is when Qt Wayland
    // picks the shell integration for it.
    auto *layer = LayerShellQt::Window::get(window);
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                    | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    layer->setExclusiveZone(-1);
    layer->setScope("pounce");
    layer->setScreen(screen);
    layer->setActivateOnShow(true);

    m_windows.insert(screen, window);
    return window;
}

void OverlayManager::showAll() {
    const QString focused = m_controller->focusedMonitor();
    const auto screens = QGuiApplication::screens();
    QQuickWindow *primary = nullptr;
    for (QScreen *screen : screens) {
        QQuickWindow *window = windowFor(screen);
        if (!window)
            continue;
        const bool isPrimary = screen->name() == focused || (focused.isEmpty() && screen == QGuiApplication::primaryScreen());
        window->setProperty("primary", isPrimary);
        // Only one surface may hold the keyboard; the others pass input
        // through to nothing, since they cover the whole monitor anyway and
        // a click on them closes the overlay.
        LayerShellQt::Window::get(window)->setKeyboardInteractivity(
            isPrimary ? LayerShellQt::Window::KeyboardInteractivityExclusive
                      : LayerShellQt::Window::KeyboardInteractivityNone);
        if (isPrimary)
            primary = window;
        else
            window->show();
    }
    // The keyboard surface last, so it ends up with focus.
    if (primary) {
        primary->show();
        primary->requestActivate();
    }
}

void OverlayManager::hideAll() {
    for (auto window : std::as_const(m_windows))
        if (window)
            window->hide();
}

void OverlayManager::screenRemoved(QScreen *screen) {
    if (auto window = m_windows.take(screen))
        window->deleteLater();
}
