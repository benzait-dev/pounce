#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Follows the Omarchy theme live: the accent for hints and highlights, and the
// theme's own backgrounds and text colors for the search panel, so pounce
// looks like it shipped with whatever theme is on. Corners follow
// Hyprland's window rounding.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(bool light READ light NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString background() const { return m_background; }
    QString surface() const { return m_surface; }
    QString selection() const { return m_selection; }
    QString foreground() const { return m_foreground; }
    QString muted() const { return m_muted; }
    bool light() const { return m_light; }
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);
    static QString readColor(const QString &contents, const QString &key, const QString &fallback);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QString m_background = "#0e0e10";
    QString m_surface = "#1a1a1e";
    QString m_selection = "#2a2a30";
    QString m_foreground = "#e6e6e6";
    QString m_muted = "#8a8a93";
    bool m_light = false;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
