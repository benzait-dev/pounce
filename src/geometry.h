#pragma once

#include <QList>
#include <QRectF>
#include <QString>

// Turning what toolkits report into places on the screen.
//
// On Wayland no client knows where its window is, so AT-SPI's "screen"
// coordinates are really window coordinates, and some toolkits (GTK 4) only
// fill in the window-relative kind at all. pounce asks every element for its
// window-relative rectangle, measures it from the toplevel frame's own
// rectangle, and places the result inside the window Hyprland reports.
namespace Geometry {

struct FrameCandidate {
    QString title;
    QRectF raw;          // The frame's own window-relative extents.
    bool active = false; // AT-SPI's "active" state: this frame has focus.
    bool showing = true;
};

struct WindowCandidate {
    QString title;
    QRectF rect;          // Hyprland's logical geometry.
    bool active = false;
    double monitorScale = 1.0;
};

// How well a frame matches a window, higher is better, negative for "can't
// be this one". Titles are the strongest signal (Chromium's frames are named
// after the window), then size, then which one has focus.
double frameScore(const FrameCandidate &frame, const WindowCandidate &window);

// Assigns frames to windows of the same process, best pairs first, each frame
// and window used once. Returns, for every window, the index of its frame or
// -1.
QList<int> assignFrames(const QList<FrameCandidate> &frames, const QList<WindowCandidate> &windows);

// How much bigger the toolkit's pixels are than Hyprland's: 1 for Wayland
// clients, the monitor scale for an XWayland client drawing at native
// resolution.
double frameScale(const QRectF &frameRaw, const QRectF &window);

// Chromium reports web page content in device pixels while its own toolbar
// uses logical ones. A document wider than the frame it sits in can only be
// in device pixels; this returns the factor to divide by (1 when it isn't).
double contentScale(const QRectF &documentRaw, const QRectF &frameRaw, double monitorScale);

// Places an element's window-relative rectangle in the global layout.
QRectF toLayout(const QRectF &raw, const QRectF &frameRaw, const QRectF &window, double contentScale = 1.0);

// Intersection over union, for spotting two accessibles that are one widget.
double overlap(const QRectF &a, const QRectF &b);

}
