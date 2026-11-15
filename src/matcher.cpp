#include "matcher.h"

#include <QRegularExpression>
#include <algorithm>

namespace Matcher {

namespace {

bool isBoundary(const QString &original, int i) {
    if (i <= 0)
        return true;
    const QChar previous = original.at(i - 1), current = original.at(i);
    if (!previous.isLetterOrNumber())
        return true;
    // camelCase and digits after letters start a new word too.
    if (previous.isLower() && current.isUpper())
        return true;
    return previous.isLetter() != current.isLetter();
}

QList<int> range(int from, int count) {
    QList<int> positions;
    positions.reserve(count);
    for (int i = 0; i < count; ++i)
        positions.append(from + i);
    return positions;
}

// Every letter of the token starts a consecutive word: "sa" in "Save As".
std::optional<QList<int>> initials(const QString &token, const QString &folded, const QString &original) {
    QList<int> starts;
    for (int i = 0; i < folded.size(); ++i)
        if (folded.at(i).isLetterOrNumber() && isBoundary(original, i))
            starts.append(i);
    for (int first = 0; first + token.size() <= starts.size(); ++first) {
        bool ok = true;
        for (int k = 0; k < token.size() && ok; ++k)
            ok = folded.at(starts[first + k]) == token.at(k);
        if (ok)
            return starts.mid(first, token.size());
    }
    return std::nullopt;
}

// The best scattered match: every start position is tried, and characters
// prefer word starts, then the next occurrence.
std::optional<Result> subsequence(const QString &token, const QString &folded, const QString &original) {
    std::optional<Result> best;
    for (int start = folded.indexOf(token.at(0)); start >= 0; start = folded.indexOf(token.at(0), start + 1)) {
        Result result;
        result.positions.append(start);
        int score = isBoundary(original, start) ? 30 : 0;
        int at = start;
        bool ok = true;
        for (int k = 1; k < token.size() && ok; ++k) {
            const QChar c = token.at(k);
            int next = -1;
            // A word start within reach beats the nearest plain occurrence.
            for (int j = at + 1; j < folded.size(); ++j)
                if (folded.at(j) == c && isBoundary(original, j)) {
                    next = j;
                    break;
                }
            const int plain = folded.indexOf(c, at + 1);
            if (plain < 0) {
                ok = false;
                break;
            }
            if (next < 0 || plain == at + 1)
                next = plain;
            if (next == at + 1)
                score += 16;
            else
                score -= std::min(12, next - at - 1);
            if (isBoundary(original, next))
                score += 20;
            result.positions.append(next);
            at = next;
        }
        if (!ok)
            break;
        result.score = 200 + score - start;
        if (!best || result.score > best->score)
            best = result;
    }
    return best;
}

std::optional<Result> matchToken(const QString &token, const QString &folded, const QString &original) {
    if (token.isEmpty())
        return Result{};
    const int length = token.size();
    const int slack = int(folded.size() - length);

    if (folded == token)
        return Result{1000, range(0, length)};
    if (folded.startsWith(token))
        return Result{900 - std::min(100, slack), range(0, length)};

    int firstSubstring = -1;
    for (int at = folded.indexOf(token); at >= 0; at = folded.indexOf(token, at + 1)) {
        if (firstSubstring < 0)
            firstSubstring = at;
        if (isBoundary(original, at))
            return Result{760 - std::min(100, at) - std::min(60, slack / 2), range(at, length)};
    }
    if (length >= 2)
        if (auto positions = initials(token, folded, original))
            return Result{700 - std::min(60, slack / 2), *positions};
    if (firstSubstring >= 0)
        return Result{500 - std::min(100, firstSubstring) - std::min(60, slack / 2), range(firstSubstring, length)};
    return subsequence(token, folded, original);
}

}

QString fold(const QString &text) {
    QString folded;
    folded.reserve(text.size());
    for (const QChar c : text) {
        QChar base = c;
        if (c.decompositionTag() != QChar::NoDecomposition) {
            const QString decomposed = c.decomposition();
            if (!decomposed.isEmpty() && decomposed.at(0).isLetter())
                base = decomposed.at(0);
        }
        folded.append(base.toLower());
    }
    return folded;
}

std::optional<Result> match(const QString &query, const QString &text) {
    const QString q = fold(query.simplified());
    if (q.isEmpty())
        return Result{};
    if (text.isEmpty())
        return std::nullopt;
    const QString folded = fold(text);

    // The whole phrase first; words in any order only when it doesn't match.
    if (auto whole = matchToken(q, folded, text); whole && (whole->score >= 500 || !q.contains(' ')))
        return whole;

    static const QRegularExpression spaces("\\s+");
    const QStringList tokens = q.split(spaces, Qt::SkipEmptyParts);
    Result total;
    for (const QString &token : tokens) {
        auto result = matchToken(token, folded, text);
        if (!result)
            return std::nullopt;
        total.score += result->score;
        total.positions += result->positions;
    }
    total.score = total.score / int(tokens.size()) - 20 * int(tokens.size() - 1);
    std::sort(total.positions.begin(), total.positions.end());
    total.positions.erase(std::unique(total.positions.begin(), total.positions.end()), total.positions.end());
    return total;
}

QStringList hints(int count, const QString &alphabet) {
    QStringList result;
    if (count <= 0 || alphabet.size() < 2)
        return result;
    // Grow the shortest labels into longer ones until there are enough; the
    // labels still at the front of the queue are the ones handed out, so no
    // label is ever a prefix of another.
    QStringList queue{QString()};
    int offset = 0;
    while (queue.size() - offset < count || queue.size() == 1) {
        const QString base = queue.at(offset++);
        for (const QChar c : alphabet)
            queue.append(QString(c) + base);
    }
    result = queue.mid(offset, count);
    for (QString &label : result)
        std::reverse(label.begin(), label.end());
    // Shortest first, so the best results get the quickest labels.
    std::stable_sort(result.begin(), result.end(),
                     [](const QString &a, const QString &b) { return a.size() < b.size(); });
    return result;
}

}
