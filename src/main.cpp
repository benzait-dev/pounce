// pounce — click anything on screen from the keyboard.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTextStream>
#include <cstdio>
#include <memory>

#include "atspi.h"
#include "cli.h"
#include "controller.h"
#include "ipc.h"
#include "overlay.h"
#include "theme.h"
#include "translator.h"

namespace {

constexpr auto Version = "0.1.0";

int daemon(int argc, char *argv[], const QByteArray &firstCommand) {
    // Overlays are Wayland layer surfaces; nothing else can draw above
    // every window.
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);
    app.setApplicationName("pounce");
    app.setApplicationVersion(Version);
    app.setDesktopFileName("pounce");
    app.setWindowIcon(QIcon::fromTheme("pounce"));
    app.setQuitOnLastWindowClosed(false);

    IpcServer server;
    QString error;
    if (!server.listen(&error)) {
        // Lost a race with another instance starting up: hand it the
        // command instead.
        if (Ipc::send(firstCommand))
            return 0;
        QTextStream(stderr) << "pounce: couldn't listen on " << Ipc::socketPath() << ": " << error << "\n";
        return 1;
    }

    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Controller controller;
    controller.setAccent(theme.accent());
    QObject::connect(&theme, &Theme::changed, &controller, [&] { controller.setAccent(theme.accent()); });

    QQmlEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &controller);
    OverlayManager overlays(&engine, &controller);

    auto handle = [&](const QByteArray &command) {
        if (command == "toggle" || command == "toggle search")
            controller.toggle(false);
        else if (command == "toggle hints")
            controller.toggle(true);
        else if (command == "show" || command == "show search")
            controller.show(false);
        else if (command == "show hints")
            controller.show(true);
        else if (command == "hide")
            controller.close();
        else if (command == "quit")
            app.quit();
    };
    QObject::connect(&server, &IpcServer::command, &app, handle);
    if (!firstCommand.isEmpty())
        QMetaObject::invokeMethod(&app, [&, firstCommand] { handle(firstCommand); }, Qt::QueuedConnection);

    return app.exec();
}

}

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));
    const QString command = arguments.value(0);

    if (Cli::handles(command)) {
        // These never draw, and must work over SSH or from an agent.
        QCoreApplication app(argc, argv);
        app.setApplicationName("pounce");
        app.setApplicationVersion(Version);
        Atspi::registerTypes();
        return Cli::run(arguments);
    }

    QByteArray ipcCommand;
    if (command.isEmpty() || command == "open" || command == "toggle")
        ipcCommand = "toggle search";
    else if (command == "hints")
        ipcCommand = "toggle hints";
    else if (command == "quit")
        ipcCommand = "quit";
    else if (command != "daemon") {
        QTextStream(stderr) << "pounce: unknown command \"" << command << "\"\n\n" << Cli::usage();
        return 1;
    }

    // Already running: pass it on and get out of the way.
    if (!ipcCommand.isEmpty() && Ipc::send(ipcCommand))
        return 0;
    if (ipcCommand == "quit")
        return 0;
    if (command == "daemon" && Ipc::send("ping"))
        return 0;
    return daemon(argc, argv, ipcCommand);
}
