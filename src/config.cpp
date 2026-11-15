#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

QString Config::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/pounce/config.json";
}

Config Config::load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Config();
    return parse(file.readAll());
}

Config Config::parse(const QByteArray &json) {
    Config config;
    const QJsonObject object = QJsonDocument::fromJson(json).object();

    const QString scope = object.value("scope").toString();
    if (scope == "screen")
        config.scope = Scope::Screen;

    if (object.value("activation").toString() == "pointer")
        config.activation = Activation::Pointer;

    // Hints are typed after ';', in lowercase (uppercase means right-click),
    // so the alphabet must be distinct lowercase letters.
    const QString alphabet = object.value("hintAlphabet").toString().toLower();
    QSet<QChar> seen;
    bool valid = alphabet.size() >= 2;
    for (const QChar c : alphabet) {
        if (!c.isLower() || seen.contains(c))
            valid = false;
        seen.insert(c);
    }
    if (valid)
        config.hintAlphabet = alphabet;

    config.restorePointer = object.value("restorePointer").toBool(config.restorePointer);
    const double dim = object.value("dim").toDouble(config.dim);
    if (dim >= 0 && dim <= 0.8)
        config.dim = dim;
    const int timeout = object.value("scanTimeoutMs").toInt(config.scanTimeoutMs);
    if (timeout >= 200 && timeout <= 20000)
        config.scanTimeoutMs = timeout;
    return config;
}
