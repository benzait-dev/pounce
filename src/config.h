#pragma once

#include <QString>

// ~/.config/pounce/config.json. Every key is optional; anything missing or
// malformed keeps its default, and the file is read again each time the
// overlay opens, so edits apply without a restart.
struct Config {
    enum class Scope { Window, Screen };
    enum class Activation { Auto, Pointer };

    Scope scope = Scope::Window;
    Activation activation = Activation::Auto;
    QString hintAlphabet = QStringLiteral("sadfjklewcmpgh");
    bool restorePointer = false;
    double dim = 0.18;
    int scanTimeoutMs = 2500;

    static QString defaultPath();
    static Config load(const QString &path = defaultPath());
    static Config parse(const QByteArray &json);
};
