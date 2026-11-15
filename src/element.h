#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

// One thing on screen that pounce can act on: a button, link, field, tab, menu
// item, or list entry, found through the accessibility tree. The object
// reference (service + path) is what AT-SPI needs to press it without the
// pointer; the rectangle, in Hyprland's global layout coordinates, is where
// the hint is drawn and where the pointer goes when an action isn't enough.
struct Element {
    QString service;
    QString path;

    QString name;
    QString description;
    int role = 0;
    QString roleName;

    QString app;
    int pid = 0;
    QString windowAddress;
    QString windowTitle;
    bool inActiveWindow = false;

    QRectF rect;
    QStringList actions;

    bool editable = false;
    bool focusable = false;
    bool focused = false;
    bool checked = false;
    bool selected = false;

    // Reading order inside the scan, used to keep results stable.
    int order = 0;

    // The text people search for: the element's name, else its description.
    QString label() const { return name.isEmpty() ? description : name; }

    // Stable across scans and restarts, so the history can learn which
    // elements someone picks most.
    QString key() const { return app + QChar(0x1f) + QString::number(role) + QChar(0x1f) + label(); }

    QPointF center() const { return rect.center(); }
};

using Elements = QList<Element>;
