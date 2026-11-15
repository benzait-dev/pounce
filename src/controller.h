#pragma once

#include <QObject>
#include <QRectF>
#include <QStringList>
#include <QTimer>
#include <memory>

#include "actuator.h"
#include "config.h"
#include "history.h"
#include "hyprland.h"
#include "ranker.h"
#include "resultsmodel.h"
#include "scanner.h"

// The overlay's state, exposed to QML as `backend`: what's been typed, what
// it matches, which result is selected, and the actions on it. The scan
// starts the moment the overlay is asked for and its results stream in.
class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString scope READ scope NOTIFY scopeChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QRectF selectedRect READ selectedRect NOTIFY selectedChanged)
    Q_PROPERTY(int count READ count NOTIFY resultsChanged)
    Q_PROPERTY(int total READ total NOTIFY resultsChanged)
    Q_PROPERTY(bool hintMode READ hintMode NOTIFY resultsChanged)
    Q_PROPERTY(QString hintTyped READ hintTyped NOTIFY resultsChanged)
    Q_PROPERTY(bool hintsReady READ hintsReady NOTIFY resultsChanged)
    Q_PROPERTY(QStringList inaccessible READ inaccessible NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged)
    Q_PROPERTY(qint64 scanMilliseconds READ scanMilliseconds NOTIFY statusChanged)
    Q_PROPERTY(double dim READ dim NOTIFY openChanged)
    Q_PROPERTY(QString focusedMonitor READ focusedMonitor NOTIFY openChanged)
    Q_PROPERTY(QObject *results READ results CONSTANT)

public:
    explicit Controller(QObject *parent = nullptr);

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    bool isOpen() const { return m_open; }
    bool scanning() const { return m_scanner.running(); }
    QString scope() const { return m_scope == Config::Scope::Screen ? "screen" : "window"; }
    int selected() const { return m_selected; }
    void setSelected(int row);
    QRectF selectedRect() const;
    int count() const { return m_model.rowCount(); }
    int total() const { return int(m_elements.size()); }
    bool hintMode() const { return m_parsed.hintMode; }
    QString hintTyped() const { return m_parsed.hint; }
    bool hintsReady() const { return !m_scanner.running(); }
    QStringList inaccessible() const { return m_inaccessible; }
    QString error() const { return m_error; }
    qint64 scanMilliseconds() const { return m_scanMilliseconds; }
    double dim() const { return m_config.dim; }
    QString focusedMonitor() const { return m_focusedMonitor; }
    QObject *results() { return &m_model; }

    void setAccent(const QString &accent) { m_model.setAccent(accent); }

    Q_INVOKABLE void activate(int row, const QString &mode = "click");
    Q_INVOKABLE void activateSelected(const QString &mode = "click");
    Q_INVOKABLE void move(int delta);
    Q_INVOKABLE void toggleScope();
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void close();

public slots:
    // `hints` opens straight into labelling everything, as `pounce hints`.
    void show(bool hints = false);
    void toggle(bool hints = false);

signals:
    void queryChanged();
    void openChanged();
    void scanningChanged();
    void scopeChanged();
    void selectedChanged();
    void resultsChanged();
    void statusChanged();

private:
    void startScan();
    void refresh();
    void scanned(const ScanReport &report);

    Config m_config;
    Config::Scope m_scope = Config::Scope::Window;
    HyprState m_state;
    Scanner m_scanner;
    Actuator m_actuator;
    History m_history;
    ResultsModel m_model;
    Elements m_elements;

    QString m_query;
    Ranker::Query m_parsed;
    int m_selected = 0;
    bool m_open = false;
    QString m_focusedMonitor;
    QStringList m_inaccessible;
    QString m_error;
    qint64 m_scanMilliseconds = 0;
};
