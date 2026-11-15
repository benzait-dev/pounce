#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

// Fuzzy matching tuned for UI labels, which are short and made of words:
// "sv" finds "Save", "sa" finds "Save As" by its initials, "new tab" finds
// "Open new tab" by both words, and "réglages" finds "Reglages". Exact and
// prefix matches always outrank scattered ones, so the obvious target is the
// first one.
namespace Matcher {

struct Result {
    int score = 0;
    QList<int> positions; // Matched character indexes in the original text.
};

std::optional<Result> match(const QString &query, const QString &text);

// Lowercase without accents, one character in for one character out, so
// match positions line up with the original text.
QString fold(const QString &text);

// Vimium-style hint labels: as short as possible, never one a prefix of
// another, typed from the home row first.
QStringList hints(int count, const QString &alphabet);

}
