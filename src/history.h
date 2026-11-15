#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

// Remembers what gets picked, so the button someone presses every day rises
// to the top of a vague query. Each pick counts, and old picks fade with a
// two-week half-life. Stored in ~/.local/state/pounce/history.json.
class History {
public:
    explicit History(const QString &path = defaultPath());

    static QString defaultPath();

    void record(const QString &key, const QDateTime &when = QDateTime::currentDateTimeUtc());
    double boost(const QString &key, const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    bool save() const;

    static constexpr int MaxEntries = 2000;

private:
    struct Entry {
        double weight = 0;
        QDateTime last;
    };
    void load();

    QString m_path;
    QHash<QString, Entry> m_entries;
};
