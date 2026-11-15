#include "scanner.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QFile>
#include <QSet>
#include <QVariantMap>
#include <algorithm>
#include <memory>

#include "geometry.h"
#include "roles.h"

namespace {

constexpr int Selectable = 22;

bool has(quint64 states, int bit) {
    return states & (quint64(1) << bit);
}

quint64 stateBits(const QList<uint> &words) {
    quint64 bits = 0;
    if (!words.isEmpty())
        bits |= words.at(0);
    if (words.size() > 1)
        bits |= quint64(words.at(1)) << 32;
    return bits;
}

int parentPid(int pid) {
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    // The command name is in parentheses and may contain spaces; the parent
    // pid is the second field after the closing one.
    const QByteArray stat = file.readAll();
    const int close = stat.lastIndexOf(')');
    if (close < 0)
        return 0;
    const QList<QByteArray> fields = stat.mid(close + 2).split(' ');
    return fields.size() > 1 ? fields.at(1).toInt() : 0;
}

// Wrapper scripts and sandboxes put the process that talks to the
// accessibility bus a level or two below the one Hyprland knows about.
bool descends(int pid, int ancestor) {
    for (int depth = 0; pid > 1 && depth < 6; ++depth) {
        if (pid == ancestor)
            return true;
        pid = parentPid(pid);
    }
    return false;
}

// Widgets whose children are only their own label, so walking into them costs
// requests and finds nothing new.
bool isLeaf(int role) {
    switch (role) {
    case Role::Link:
    case Role::PushButton:
    case Role::ToggleButton:
    case Role::CheckBox:
    case Role::RadioButton:
    case Role::Entry:
    case Role::PasswordText:
    case Role::Slider:
    case Role::SpinButton:
    case Role::Switch:
        return true;
    default:
        return false;
    }
}

bool needsFocusable(int role) {
    // A web page's <li> and <td> are list items and table cells too; only the
    // ones that can be focused or selected are things to click.
    return role == Role::ListItem || role == Role::TableCell || role == Role::TreeItem || role == Role::Icon;
}

bool isCandidate(int role, quint64 states) {
    const bool focusable = has(states, State::Focusable) || has(states, Selectable);
    if (role == Role::Text)
        return has(states, State::Editable);
    if (Roles::isInteractive(role))
        return !needsFocusable(role) || focusable;
    return has(states, State::Focusable) && !Roles::isContainer(role) && role != Role::Label
        && role != Role::Heading && role != Role::Static;
}

}

Scanner::Scanner(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish(true); });
    m_progress.setInterval(45);
    connect(&m_progress, &QTimer::timeout, this, [this] {
        if (m_raw.size() != m_lastProgress) {
            m_lastProgress = m_raw.size();
            emit progress(finalize(false));
        }
    });
}

void Scanner::start(const QList<ScanTarget> &targets, int timeoutMs) {
    cancel();
    ++m_generation;
    m_targets = targets;
    m_frames.clear();
    m_documents.clear();
    m_raw.clear();
    m_queue.clear();
    m_targetFrame = QList<int>(targets.size(), -1);
    m_targetNodes = QList<int>(targets.size(), 0);
    m_inflight = 0;
    m_pendingApps = 0;
    m_nodes = 0;
    m_framesResolved = false;
    m_lastProgress = -1;
    m_error.clear();
    m_running = true;
    m_clock.start();

    QString error;
    if (!Atspi::bus(&error).isConnected()) {
        m_error = error;
        finish(false);
        return;
    }
    if (targets.isEmpty()) {
        finish(false);
        return;
    }
    m_timeout.start(timeoutMs);
    m_progress.start();
    listApplications();
}

void Scanner::cancel() {
    if (!m_running)
        return;
    ++m_generation;
    m_running = false;
    m_queue.clear();
    m_timeout.stop();
    m_progress.stop();
}

void Scanner::issue(std::function<QDBusPendingCall()> make, Handler handler) {
    m_queue.append({std::move(make), std::move(handler)});
    pump();
}

void Scanner::pump() {
    while (m_running && m_inflight < MaxInflight && !m_queue.isEmpty()) {
        auto [make, handler] = m_queue.takeFirst();
        ++m_inflight;
        auto *watcher = new QDBusPendingCallWatcher(make(), this);
        const quint64 generation = m_generation;
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, generation, handler = std::move(handler)] {
                    watcher->deleteLater();
                    if (generation != m_generation)
                        return;
                    --m_inflight;
                    handler(*watcher);
                    pump();
                    settle();
                });
    }
}

void Scanner::settle() {
    if (m_running && m_framesResolved && m_inflight == 0 && m_queue.isEmpty())
        finish(false);
}

bool Scanner::wantsPid(int pid) const {
    for (const auto &target : m_targets)
        if (target.window.pid == pid || descends(pid, target.window.pid))
            return true;
    return false;
}

void Scanner::listApplications() {
    ++m_pendingApps;
    const ObjRef root{Atspi::RegistryService, QDBusObjectPath(Atspi::RootPath)};
    issue([root] { return Atspi::call(root, Atspi::AccessibleInterface, "GetChildren"); },
          [this](const QDBusPendingCall &call) {
              const QDBusPendingReply<QList<ObjRef>> reply = call;
              if (reply.isError())
                  m_error = reply.error().message();
              for (const ObjRef &app : reply.value()) {
                  if (app.isNull())
                      continue;
                  if (m_pidCache.contains(app.service)) {
                      applicationFound(app, m_pidCache.value(app.service));
                      continue;
                  }
                  ++m_pendingApps;
                  issue([app] { return Atspi::pidCall(app.service); }, [this, app](const QDBusPendingCall &call) {
                      const QDBusPendingReply<uint> reply = call;
                      const int pid = reply.isError() ? 0 : int(reply.value());
                      m_pidCache.insert(app.service, pid);
                      applicationFound(app, pid);
                      if (--m_pendingApps == 0)
                          framesReady();
                  });
              }
              if (--m_pendingApps == 0)
                  framesReady();
          });
}

void Scanner::applicationFound(const ObjRef &app, int pid) {
    if (pid <= 0 || !wantsPid(pid))
        return;
    ++m_pendingApps;
    issue([app] { return Atspi::call(app, Atspi::AccessibleInterface, "GetChildren"); },
          [this, pid](const QDBusPendingCall &call) {
              const QDBusPendingReply<QList<ObjRef>> reply = call;
              for (const ObjRef &ref : reply.value()) {
                  if (ref.isNull())
                      continue;
                  const int index = m_frames.size();
                  Frame frame;
                  frame.ref = ref;
                  frame.pid = pid;
                  m_frames.append(frame);
                  m_pendingApps += 3;

                  readNames(ref, [this, index](const QString &name, const QString &) {
                      m_frames[index].title = name;
                      if (--m_pendingApps == 0)
                          framesReady();
                  });
                  issue([ref] { return Atspi::call(ref, Atspi::ComponentInterface, "GetExtents", {Atspi::CoordWindow}); },
                        [this, index](const QDBusPendingCall &call) {
                            const QDBusPendingReply<AtspiRect> reply = call;
                            if (!reply.isError()) {
                                const AtspiRect r = reply.value();
                                m_frames[index].raw = QRectF(r.x, r.y, r.width, r.height);
                            }
                            if (--m_pendingApps == 0)
                                framesReady();
                        });
                  issue([ref] { return Atspi::call(ref, Atspi::AccessibleInterface, "GetState"); },
                        [this, index](const QDBusPendingCall &call) {
                            const QDBusPendingReply<QList<uint>> reply = call;
                            const quint64 states = stateBits(reply.value());
                            m_frames[index].active = has(states, State::Active);
                            m_frames[index].showing = has(states, State::Showing);
                            if (--m_pendingApps == 0)
                                framesReady();
                        });
              }
              if (--m_pendingApps == 0)
                  framesReady();
          });
}

void Scanner::framesReady() {
    if (m_framesResolved)
        return;
    m_framesResolved = true;

    // Assign frames per process: Chromium's one process owns every browser
    // window, and each window must get its own frame.
    QSet<int> done;
    for (int t = 0; t < m_targets.size(); ++t) {
        const int pid = m_targets[t].window.pid;
        if (done.contains(pid))
            continue;
        done.insert(pid);

        QList<int> targetIndexes, frameIndexes;
        QList<Geometry::WindowCandidate> windows;
        QList<Geometry::FrameCandidate> frames;
        for (int u = 0; u < m_targets.size(); ++u)
            if (m_targets[u].window.pid == pid) {
                targetIndexes.append(u);
                windows.append({m_targets[u].window.title, m_targets[u].window.rect, m_targets[u].active,
                                m_targets[u].monitorScale});
            }
        for (int f = 0; f < m_frames.size(); ++f)
            if (m_frames[f].pid == pid || descends(m_frames[f].pid, pid)) {
                frameIndexes.append(f);
                frames.append({m_frames[f].title, m_frames[f].raw, m_frames[f].active, m_frames[f].showing});
            }
        const QList<int> assigned = Geometry::assignFrames(frames, windows);
        for (int i = 0; i < assigned.size(); ++i)
            if (assigned[i] >= 0)
                m_targetFrame[targetIndexes[i]] = frameIndexes[assigned[i]];
    }

    for (int t = 0; t < m_targets.size(); ++t) {
        const int f = m_targetFrame[t];
        if (f < 0)
            continue;
        const ObjRef ref = m_frames[f].ref;
        issue([ref] { return Atspi::call(ref, Atspi::AccessibleInterface, "GetChildren"); },
              [this, t](const QDBusPendingCall &call) {
                  const QDBusPendingReply<QList<ObjRef>> reply = call;
                  visitChildren(reply.value(), t, 1, -1);
              });
    }
    settle();
}

void Scanner::visitChildren(const QList<ObjRef> &children, int target, int depth, int document) {
    if (depth > MaxDepth)
        return;
    for (const ObjRef &child : children)
        if (!child.isNull())
            visit(child, target, depth, document);
}

void Scanner::visit(const ObjRef &ref, int target, int depth, int document) {
    if (m_nodes >= MaxNodes)
        return;
    ++m_nodes;
    ++m_targetNodes[target];

    struct Node {
        int role = -1;
        quint64 states = 0;
        QList<ObjRef> children;
        int pending = 3;
    };
    auto node = std::make_shared<Node>();

    auto complete = [this, node, ref, target, depth, document] {
        if (--node->pending > 0 || node->role < 0)
            return;
        if (has(node->states, State::Defunct) || !has(node->states, State::Showing))
            return;
        const int role = node->role;
        if (isCandidate(role, node->states))
            fetchDetails(ref, role, node->states, target, document);
        if (isLeaf(role))
            return;
        if (role == Role::DocumentWeb) {
            // Read the page's own rectangle before its contents, so every
            // element below already knows which units the page reports in.
            const int index = m_documents.size();
            m_documents.append(Document{target, QRectF()});
            const QList<ObjRef> children = node->children;
            issue([ref] { return Atspi::call(ref, Atspi::ComponentInterface, "GetExtents", {Atspi::CoordWindow}); },
                  [this, index, children, target, depth](const QDBusPendingCall &call) {
                      const QDBusPendingReply<AtspiRect> reply = call;
                      if (!reply.isError()) {
                          const AtspiRect r = reply.value();
                          m_documents[index].raw = QRectF(r.x, r.y, r.width, r.height);
                      }
                      visitChildren(children, target, depth + 1, index);
                  });
            return;
        }
        visitChildren(node->children, target, depth + 1, document);
    };

    issue([ref] { return Atspi::call(ref, Atspi::AccessibleInterface, "GetRole"); },
          [node, complete](const QDBusPendingCall &call) {
              const QDBusPendingReply<uint> reply = call;
              if (!reply.isError())
                  node->role = int(reply.value());
              complete();
          });
    issue([ref] { return Atspi::call(ref, Atspi::AccessibleInterface, "GetState"); },
          [node, complete](const QDBusPendingCall &call) {
              const QDBusPendingReply<QList<uint>> reply = call;
              if (!reply.isError())
                  node->states = stateBits(reply.value());
              complete();
          });
    issue([ref] { return Atspi::call(ref, Atspi::AccessibleInterface, "GetChildren"); },
          [node, complete](const QDBusPendingCall &call) {
              const QDBusPendingReply<QList<ObjRef>> reply = call;
              if (!reply.isError())
                  node->children = reply.value();
              complete();
          });
}

void Scanner::readNames(const ObjRef &ref, std::function<void(const QString &, const QString &)> done) {
    issue([ref] { return Atspi::call(ref, "org.freedesktop.DBus.Properties", "GetAll", {QString(Atspi::AccessibleInterface)}); },
          [this, ref, done](const QDBusPendingCall &call) {
              const QDBusPendingReply<QVariantMap> reply = call;
              if (!reply.isError()) {
                  const QVariantMap properties = reply.value();
                  done(properties.value("Name").toString(), properties.value("Description").toString());
                  return;
              }
              struct Names {
                  QString name, description;
                  int remaining = 2;
              };
              auto names = std::make_shared<Names>();
              auto get = [ref](const char *property) {
                  return [ref, property] {
                      return Atspi::call(ref, "org.freedesktop.DBus.Properties", "Get",
                                         {QString(Atspi::AccessibleInterface), QString(property)});
                  };
              };
              auto read = [names, done](QString Names::*field) {
                  return [names, done, field](const QDBusPendingCall &call) {
                      const QDBusPendingReply<QDBusVariant> reply = call;
                      if (!reply.isError())
                          (*names).*field = reply.value().variant().toString();
                      if (--names->remaining == 0)
                          done(names->name, names->description);
                  };
              };
              issue(get("Name"), read(&Names::name));
              issue(get("Description"), read(&Names::description));
          });
}

void Scanner::fetchDetails(const ObjRef &ref, int role, quint64 states, int target, int document) {
    auto raw = std::make_shared<RawElement>();
    raw->target = target;
    raw->document = document;
    Element &e = raw->element;
    e.service = ref.service;
    e.path = ref.path.path();
    e.role = role;
    e.roleName = Roles::displayName(role);
    e.editable = has(states, State::Editable) || role == Role::Entry || role == Role::PasswordText;
    e.focusable = has(states, State::Focusable);
    e.focused = has(states, State::Focused);
    e.checked = has(states, State::Checked) || has(states, State::Pressed);
    e.selected = has(states, State::Selected);
    const HyprWindow &window = m_targets[target].window;
    e.app = window.cls;
    e.pid = window.pid;
    e.windowAddress = window.address;
    e.windowTitle = window.title;
    e.inActiveWindow = m_targets[target].active;

    auto pending = std::make_shared<int>(3);
    auto complete = [this, raw, pending] {
        if (--*pending > 0)
            return;
        if (raw->raw.width() > 0 && raw->raw.height() > 0)
            m_raw.append(*raw);
    };

    readNames(ref, [raw, complete](const QString &name, const QString &description) {
        raw->element.name = name.simplified();
        raw->element.description = description.simplified();
        complete();
    });
    issue([ref] { return Atspi::call(ref, Atspi::ComponentInterface, "GetExtents", {Atspi::CoordWindow}); },
          [raw, complete](const QDBusPendingCall &call) {
              const QDBusPendingReply<AtspiRect> reply = call;
              if (!reply.isError()) {
                  const AtspiRect r = reply.value();
                  raw->raw = QRectF(r.x, r.y, r.width, r.height);
              }
              complete();
          });
    issue([ref] { return Atspi::call(ref, Atspi::ActionInterface, "GetActions"); },
          [raw, complete](const QDBusPendingCall &call) {
              const QDBusPendingReply<QList<ActionInfo>> reply = call;
              if (!reply.isError())
                  for (const ActionInfo &action : reply.value())
                      raw->element.actions.append(action.name);
              complete();
          });
}

Elements Scanner::finalize(bool final) const {
    Q_UNUSED(final)
    Elements placed;
    placed.reserve(m_raw.size());
    for (const RawElement &raw : m_raw) {
        const int f = m_targetFrame.value(raw.target, -1);
        if (f < 0)
            continue;
        const ScanTarget &target = m_targets[raw.target];
        const QRectF frameRaw = m_frames[f].raw;
        double content = 1.0;
        if (raw.document >= 0 && raw.document < m_documents.size())
            content = Geometry::contentScale(m_documents[raw.document].raw, frameRaw, target.monitorScale);
        QRectF rect = Geometry::toLayout(raw.raw, frameRaw, target.window.rect, content);

        // Only what can be seen: inside the window and on its monitor.
        // Scrolled-away content and collapsed menus report positions far
        // outside both.
        const QRectF visibleArea = target.window.rect.intersected(target.monitorRect);
        if (!visibleArea.contains(rect.center()))
            continue;
        rect = rect.intersected(visibleArea);
        if (rect.width() < 2 || rect.height() < 2)
            continue;

        Element element = raw.element;
        element.rect = rect;
        placed.append(element);
    }

    // One widget is often two accessibles (a GTK menu button and the toggle
    // inside it, a Chromium tab and its label). Keep the one that can act.
    QHash<QString, QList<int>> byLabel;
    for (int i = 0; i < placed.size(); ++i)
        byLabel[placed[i].label()].append(i);
    QList<bool> drop(placed.size(), false);
    for (auto it = byLabel.cbegin(); it != byLabel.cend(); ++it) {
        const QList<int> &indexes = it.value();
        for (int a = 0; a < indexes.size(); ++a)
            for (int b = a + 1; b < indexes.size(); ++b) {
                const int i = indexes[a], j = indexes[b];
                if (drop[i] || drop[j] || Geometry::overlap(placed[i].rect, placed[j].rect) < 0.6)
                    continue;
                const bool iActs = Atspi::primaryAction(placed[i].actions, placed[i].editable) >= 0;
                const bool jActs = Atspi::primaryAction(placed[j].actions, placed[j].editable) >= 0;
                if (iActs != jActs)
                    drop[iActs ? j : i] = true;
                else
                    drop[placed[i].rect.width() * placed[i].rect.height() <= placed[j].rect.width() * placed[j].rect.height() ? j : i] = true;
            }
    }

    QList<std::pair<int, Element>> ordered;
    for (int i = 0; i < placed.size(); ++i)
        if (!drop[i])
            ordered.append({i, placed[i]});

    // The order windows were given in (active first), then reading order:
    // rows top to bottom, left to right inside a row.
    QHash<QString, int> windowOrder;
    for (int t = 0; t < m_targets.size(); ++t)
        windowOrder.insert(m_targets[t].window.address, t);
    std::stable_sort(ordered.begin(), ordered.end(), [&windowOrder](const auto &x, const auto &y) {
        const int wx = windowOrder.value(x.second.windowAddress), wy = windowOrder.value(y.second.windowAddress);
        if (wx != wy)
            return wx < wy;
        const int rowX = int(x.second.rect.center().y() / 8), rowY = int(y.second.rect.center().y() / 8);
        if (rowX != rowY)
            return rowX < rowY;
        return x.second.rect.x() < y.second.rect.x();
    });

    Elements result;
    result.reserve(ordered.size());
    for (auto &[index, element] : ordered) {
        element.order = result.size();
        result.append(element);
    }
    return result;
}

void Scanner::finish(bool timedOut) {
    if (!m_running)
        return;
    ScanReport report;
    report.elements = finalize(true);
    report.nodes = m_nodes;
    report.milliseconds = m_clock.elapsed();
    report.timedOut = timedOut;
    report.error = m_error;

    QSet<QString> withElements;
    for (const Element &element : report.elements)
        withElements.insert(element.windowAddress);
    for (int t = 0; t < m_targets.size(); ++t) {
        const HyprWindow &window = m_targets[t].window;
        // No frame, or a frame with nothing inside it: Chromium and Electron
        // register with the bus but leave the tree empty until asked to build
        // it at launch.
        if ((m_targetFrame[t] < 0 || m_targetNodes[t] == 0) && !withElements.contains(window.address)
            && !report.inaccessible.contains(window.cls))
            report.inaccessible.append(window.cls);
    }

    cancel();
    emit finished(report);
}
