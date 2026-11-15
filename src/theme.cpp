// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    // pounce stays resident, so a slow poll is plenty; it also re-reads on
    // every theme change.
    m_roundingPoll.setInterval(5000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QString Theme::readColor(const QString &contents, const QString &key, const QString &fallback) {
    const QRegularExpression expression(QStringLiteral(R"re(^\s*%1\s*=\s*["'](#[0-9a-fA-F]{6})["'])re")
                                            .arg(QRegularExpression::escape(key)),
                                        QRegularExpression::MultilineOption);
    const auto match = expression.match(contents);
    return match.hasMatch() ? match.captured(1) : fallback;
}

QString Theme::readAccent(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return "#FFD60A";
    return readColor(QString::fromUtf8(file.readAll()), "accent", "#FFD60A");
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    const QColor c(m_accent);
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? "black" : "white";
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    QString contents;
    QFile file(m_directory + "/theme/colors.toml");
    if (file.open(QIODevice::ReadOnly))
        contents = QString::fromUtf8(file.readAll());

    static const QRegularExpression lightMode(R"re(^\s*mode\s*=\s*["']light["'])re", QRegularExpression::MultilineOption);
    const bool light = lightMode.match(contents).hasMatch();
    const QString accent = readColor(contents, "accent", "#FFD60A");
    const QString background = readColor(contents, "background", light ? "#f4f4f5" : "#0e0e10");
    const QString surface = readColor(contents, "lighter_background", readColor(contents, "dark_background", light ? "#ffffff" : "#1a1a1e"));
    const QString selection = readColor(contents, "selection", light ? "#dcdce0" : "#2a2a30");
    const QString foreground = readColor(contents, "bright_foreground", readColor(contents, "foreground", light ? "#18181b" : "#e6e6e6"));
    const QString muted = readColor(contents, "dark_foreground", readColor(contents, "muted", light ? "#71717a" : "#8a8a93"));

    if (accent != m_accent || background != m_background || surface != m_surface || selection != m_selection
        || foreground != m_foreground || muted != m_muted || light != m_light) {
        m_accent = accent;
        m_background = background;
        m_surface = surface;
        m_selection = selection;
        m_foreground = foreground;
        m_muted = muted;
        m_light = light;
        emit changed();
    }
}
