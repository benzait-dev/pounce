#pragma once

#include <QList>
#include <QString>
#include <functional>

#include "element.h"

// Turns what was typed into an ordered, labelled list.
//
// The query has two halves split by ';'. The part before it is a search
// ("save"); the part after it is a hint label ("sd"). Either can be empty:
// ";" on its own puts a label on everything, like Vimium, and "save;s" picks
// the second "Save" by its label. A label typed in uppercase asks for a
// right-click instead.
namespace Ranker {

struct Query {
    QString search;
    QString hint;     // Lowercased.
    bool hintMode = false;
    bool rightClick = false;

    static Query parse(const QString &text);
};

struct Ranked {
    int index = 0;       // Into the element list.
    double score = 0;
    QList<int> positions;
    QString hint;
};

using Boost = std::function<double(const Element &)>;

// Elements that match, best first, each with its label. Unnamed elements only
// appear when nothing is being searched for, since they can be hinted but
// not found by name.
//
// Labels belong to elements, not to places in the results: each element's
// label comes from its reading order, so it never changes while the search
// narrows. With `matchLabels`, a search that is a label, or the start of one,
// also finds that element whatever its name: an exact label goes first, so
// typing what's on screen and pressing Enter clicks it.
QList<Ranked> rank(const Elements &elements, const QString &search, const QString &alphabet, const Boost &boost = {},
                   int limit = 400, bool matchLabels = false);

// Whether `search` could be a label typed in the search field: one word,
// every letter from the label alphabet.
bool looksLikeLabel(const QString &search, const QString &alphabet);

// The result whose label is exactly `hint`, or -1.
int findHint(const QList<Ranked> &ranked, const QString &hint);

// Whether any label starts with what's been typed so far.
bool hintPrefixExists(const QList<Ranked> &ranked, const QString &prefix);

}
