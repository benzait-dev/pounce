#include "ranker.h"

#include <algorithm>

#include "matcher.h"

namespace Ranker {

Query Query::parse(const QString &text) {
    Query query;
    const int split = text.indexOf(';');
    if (split < 0) {
        query.search = text;
        return query;
    }
    query.hintMode = true;
    query.search = text.left(split);
    const QString hint = text.mid(split + 1).trimmed();
    query.rightClick = !hint.isEmpty() && hint.at(hint.size() - 1).isUpper();
    query.hint = hint.toLower();
    return query;
}

bool looksLikeLabel(const QString &search, const QString &alphabet) {
    const QString lower = search.trimmed().toLower();
    if (lower.isEmpty())
        return false;
    for (const QChar c : lower)
        if (!alphabet.contains(c))
            return false;
    return true;
}

QList<Ranked> rank(const Elements &elements, const QString &search, const QString &alphabet, const Boost &boost,
                   int limit, bool matchLabels) {
    const QString trimmed = search.trimmed();
    const QStringList labels = Matcher::hints(int(elements.size()), alphabet);
    const QString typedLabel = matchLabels && looksLikeLabel(trimmed, alphabet) ? trimmed.toLower() : QString();
    // Far outside any text score, so label hits sort first or last.
    constexpr double ExactLabel = 1e6, LabelOnly = -1e6;

    QList<Ranked> ranked;
    ranked.reserve(elements.size());
    for (int i = 0; i < elements.size(); ++i) {
        const Element &element = elements[i];
        Ranked entry;
        entry.index = i;
        entry.hint = labels.value(i);
        const bool labelHit = !typedLabel.isEmpty() && entry.hint.startsWith(typedLabel);
        if (labelHit && entry.hint == typedLabel) {
            entry.score = ExactLabel;
        } else if (!trimmed.isEmpty()) {
            if (auto hit = Matcher::match(trimmed, element.name)) {
                entry.score = hit->score;
                entry.positions = hit->positions;
            } else if (element.name.isEmpty() && !element.description.isEmpty()) {
                if (auto described = Matcher::match(trimmed, element.description))
                    entry.score = described->score - 40, entry.positions = described->positions;
                else
                    continue;
            } else if (auto described = Matcher::match(trimmed, element.description); described && described->score >= 500) {
                // A tooltip-style description that matches well, with no
                // highlight since the label shown is the name.
                entry.score = described->score - 150;
            } else if (labelHit) {
                // Kept on screen while its label is still being typed.
                entry.score = LabelOnly;
            } else {
                continue;
            }
            if (element.inActiveWindow)
                entry.score += 25;
        }
        if (boost)
            entry.score += boost(element);
        ranked.append(entry);
    }

    if (!trimmed.isEmpty())
        std::stable_sort(ranked.begin(), ranked.end(), [&elements](const Ranked &a, const Ranked &b) {
            if (a.score != b.score)
                return a.score > b.score;
            return elements[a.index].order < elements[b.index].order;
        });
    else
        // Nothing typed: favourites first, then reading order.
        std::stable_sort(ranked.begin(), ranked.end(),
                         [](const Ranked &a, const Ranked &b) { return a.score > b.score; });

    if (ranked.size() > limit)
        ranked.resize(limit);
    return ranked;
}

int findHint(const QList<Ranked> &ranked, const QString &hint) {
    if (hint.isEmpty())
        return -1;
    for (int i = 0; i < ranked.size(); ++i)
        if (ranked[i].hint == hint)
            return i;
    return -1;
}

bool hintPrefixExists(const QList<Ranked> &ranked, const QString &prefix) {
    return std::any_of(ranked.cbegin(), ranked.cend(), [&prefix](const Ranked &r) { return r.hint.startsWith(prefix); });
}

}
