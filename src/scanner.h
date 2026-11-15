#pragma once

#include <QDBusPendingCall>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

#include "atspi.h"
#include "element.h"
#include "hyprland.h"

// One window to look inside.
struct ScanTarget {
    HyprWindow window;
    double monitorScale = 1.0;
    QRectF monitorRect;
    bool active = false;
};

struct ScanReport {
    Elements elements;
    // Windows that are on screen but expose no accessibility tree, by class,
    // so the overlay can say which apps need `pounce setup` or a restart.
    QStringList inaccessible;
    int nodes = 0;
    qint64 milliseconds = 0;
    bool timedOut = false;
    QString error;
};

// Walks the accessibility trees of the target windows, breadth first, with
// every request asynchronous and many in flight. Invisible subtrees are cut
// off as soon as their state says so. Results stream out through
// `progress` so the overlay fills in while big web pages are still being
// read, and `finished` arrives once, with everything.
class Scanner : public QObject {
    Q_OBJECT

public:
    explicit Scanner(QObject *parent = nullptr);

    void start(const QList<ScanTarget> &targets, int timeoutMs = 2500);
    void cancel();
    bool running() const { return m_running; }

    // Caps that keep a pathological tree (a spreadsheet with a million cells)
    // from running away.
    static constexpr int MaxNodes = 40000;
    static constexpr int MaxDepth = 96;
    static constexpr int MaxInflight = 320;

signals:
    void progress(const Elements &elements);
    void finished(const ScanReport &report);

private:
    struct Frame {
        ObjRef ref;
        int pid = 0;
        QString title;
        QRectF raw;
        bool active = false;
        bool showing = true;
        int pending = 3;
    };
    struct Document {
        int target;
        QRectF raw;
    };
    struct RawElement {
        Element element;
        QRectF raw;
        int target;
        int document;
    };

    using Handler = std::function<void(const QDBusPendingCall &)>;
    void issue(std::function<QDBusPendingCall()> make, Handler handler);
    void pump();
    void settle();

    void listApplications();
    void applicationFound(const ObjRef &app, int pid);
    void framesReady();
    void visit(const ObjRef &ref, int target, int depth, int document);
    void visitChildren(const QList<ObjRef> &children, int target, int depth, int document);
    // Name and description in one call where the toolkit has GetAll, two
    // where it doesn't (Qt).
    void readNames(const ObjRef &ref, std::function<void(const QString &, const QString &)> done);
    void fetchDetails(const ObjRef &ref, int role, quint64 states, int target, int document);

    bool wantsPid(int pid) const;
    Elements finalize(bool final) const;
    void finish(bool timedOut);

    QList<ScanTarget> m_targets;
    QList<Frame> m_frames;
    QList<int> m_targetFrame;
    QList<Document> m_documents;
    QList<RawElement> m_raw;
    QList<int> m_targetNodes;

    QList<std::pair<std::function<QDBusPendingCall()>, Handler>> m_queue;
    int m_inflight = 0;
    int m_pendingApps = 0;
    int m_nodes = 0;
    qsizetype m_lastProgress = -1;
    quint64 m_generation = 0;
    bool m_running = false;
    bool m_framesResolved = false;
    QString m_error;

    QHash<QString, int> m_pidCache;
    QTimer m_timeout;
    QTimer m_progress;
    QElapsedTimer m_clock;
};
