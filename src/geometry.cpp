#include "geometry.h"

#include <algorithm>
#include <cmath>

namespace Geometry {

namespace {

bool near(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

bool sizeMatches(const QRectF &raw, const QRectF &window, double scale) {
    return near(raw.width(), window.width() * scale, 6 * scale) && near(raw.height(), window.height() * scale, 6 * scale);
}

QString normalizedTitle(QString title) {
    return title.trimmed().toCaseFolded();
}

}

double frameScore(const FrameCandidate &frame, const WindowCandidate &window) {
    // Popups, tooltips, and the zero-sized frames toolkits keep around for
    // closed menus are never a toplevel window.
    if (frame.raw.width() < 24 || frame.raw.height() < 24)
        return -1;

    double score = 0;
    const QString a = normalizedTitle(frame.title), b = normalizedTitle(window.title);
    if (!a.isEmpty() && a == b)
        score += 100;
    else if (!a.isEmpty() && !b.isEmpty() && (b.contains(a) || a.contains(b)))
        score += 50;

    if (sizeMatches(frame.raw, window.rect, 1.0))
        score += 40;
    else if (window.monitorScale > 1.01 && sizeMatches(frame.raw, window.rect, window.monitorScale))
        score += 30;

    if (frame.active && window.active)
        score += 30;
    else if (frame.active != window.active)
        score -= 5;
    if (frame.showing)
        score += 5;
    return score;
}

QList<int> assignFrames(const QList<FrameCandidate> &frames, const QList<WindowCandidate> &windows) {
    struct Pair {
        double score;
        int frame, window;
    };
    QList<Pair> pairs;
    for (int f = 0; f < frames.size(); ++f)
        for (int w = 0; w < windows.size(); ++w) {
            const double score = frameScore(frames[f], windows[w]);
            if (score >= 0)
                pairs.append({score, f, w});
        }
    std::stable_sort(pairs.begin(), pairs.end(), [](const Pair &x, const Pair &y) { return x.score > y.score; });

    QList<int> result(windows.size(), -1);
    QList<bool> used(frames.size(), false);
    for (const auto &pair : pairs) {
        if (result[pair.window] != -1 || used[pair.frame])
            continue;
        // With several windows of one app, a frame that matches neither the
        // title nor the size is a guess that would put hints on the wrong
        // window; a lone window gets its only plausible frame regardless.
        if (pair.score < 30 && windows.size() > 1)
            continue;
        result[pair.window] = pair.frame;
        used[pair.frame] = true;
    }
    return result;
}

double frameScale(const QRectF &frameRaw, const QRectF &window) {
    if (frameRaw.width() <= 0 || window.width() <= 0)
        return 1.0;
    const double scale = frameRaw.width() / window.width();
    return near(scale, 1.0, 0.03) ? 1.0 : scale;
}

double contentScale(const QRectF &documentRaw, const QRectF &frameRaw, double monitorScale) {
    if (monitorScale <= 1.01 || frameRaw.width() <= 0)
        return 1.0;
    // A page can never be wider than its window in the window's own units.
    if (documentRaw.width() > frameRaw.width() * 1.04 + 4 || documentRaw.height() > frameRaw.height() * 1.04 + 4)
        return monitorScale;
    return 1.0;
}

QRectF toLayout(const QRectF &raw, const QRectF &frameRaw, const QRectF &window, double contentScale) {
    const QRectF content(raw.x() / contentScale, raw.y() / contentScale, raw.width() / contentScale,
                         raw.height() / contentScale);
    const double scale = frameScale(frameRaw, window);
    return QRectF(window.x() + (content.x() - frameRaw.x()) / scale, window.y() + (content.y() - frameRaw.y()) / scale,
                  content.width() / scale, content.height() / scale);
}

double overlap(const QRectF &a, const QRectF &b) {
    const QRectF intersection = a.intersected(b);
    if (intersection.isEmpty())
        return 0;
    const double shared = intersection.width() * intersection.height();
    const double total = a.width() * a.height() + b.width() * b.height() - shared;
    return total > 0 ? shared / total : 0;
}

}
