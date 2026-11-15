#include "history.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {

constexpr double HalfLifeDays = 14.0;

double decay(const QDateTime &from, const QDateTime &to) {
    const double days = std::max<qint64>(0, from.secsTo(to)) / 86400.0;
    return std::pow(0.5, days / HalfLifeDays);
}

}

History::History(const QString &path) : m_path(path) {
    load();
}

QString History::defaultPath() {
    QString state = qEnvironmentVariable("XDG_STATE_HOME");
    if (state.isEmpty())
        state = QDir::homePath() + "/.local/state";
    return state + "/pounce/history.json";
}

void History::load() {
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray entries = QJsonDocument::fromJson(file.readAll()).object().value("entries").toArray();
    for (const auto &value : entries) {
        const QJsonObject object = value.toObject();
        const QString key = object.value("key").toString();
        const QDateTime last = QDateTime::fromString(object.value("last").toString(), Qt::ISODate);
        if (key.isEmpty() || !last.isValid())
            continue;
        m_entries.insert(key, {object.value("weight").toDouble(), last});
    }
}

void History::record(const QString &key, const QDateTime &when) {
    if (key.isEmpty())
        return;
    Entry &entry = m_entries[key];
    // Decay what's there to now, then add this pick.
    entry.weight = (entry.last.isValid() ? entry.weight * decay(entry.last, when) : 0) + 1.0;
    entry.last = when;

    if (m_entries.size() > MaxEntries) {
        QList<std::pair<double, QString>> ranked;
        for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it)
            ranked.append({it->weight * decay(it->last, when), it.key()});
        std::sort(ranked.begin(), ranked.end());
        for (int i = 0; i < ranked.size() - MaxEntries; ++i)
            m_entries.remove(ranked[i].second);
    }
}

double History::boost(const QString &key, const QDateTime &now) const {
    const auto it = m_entries.constFind(key);
    if (it == m_entries.cend())
        return 0;
    // Logarithmic, so a favourite helps a weak match without burying a
    // strong one: about +55 after one pick, +130 after ten.
    return 55.0 * std::log2(1.0 + it->weight * decay(it->last, now));
}

bool History::save() const {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QJsonArray entries;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it)
        entries.append(QJsonObject{{"key", it.key()}, {"weight", it->weight}, {"last", it->last.toString(Qt::ISODate)}});
    // Written beside the file and renamed over it, so a crash never leaves
    // half a history.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}}).toJson(QJsonDocument::Compact));
    return file.commit();
}
