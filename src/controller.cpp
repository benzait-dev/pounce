#include "controller.h"

#include <QProcess>

#include "roles.h"

Controller::Controller(QObject *parent) : QObject(parent) {
    connect(&m_scanner, &Scanner::progress, this, [this](const Elements &elements) {
        m_elements = elements;
        refresh();
    });
    connect(&m_scanner, &Scanner::finished, this, &Controller::scanned);
}

void Controller::show(bool hints) {
    m_config = Config::load();
    if (!m_open)
        m_scope = m_config.scope;
    m_state = Hyprland::state();
    if (const HyprMonitor *monitor = m_state.focusedMonitor())
        m_focusedMonitor = monitor->name;

    m_query = hints ? QStringLiteral(";") : QString();
    m_parsed = Ranker::Query::parse(m_query);
    m_selected = 0;
    m_elements.clear();
    m_model.set({}, {});
    m_error.clear();
    m_inaccessible.clear();

    const bool wasOpen = m_open;
    m_open = true;
    emit queryChanged();
    emit resultsChanged();
    emit selectedChanged();
    emit statusChanged();
    if (!wasOpen)
        emit openChanged();
    startScan();
}

void Controller::toggle(bool hints) {
    if (m_open)
        close();
    else
        show(hints);
}

void Controller::close() {
    m_scanner.cancel();
    if (!m_open)
        return;
    m_open = false;
    emit openChanged();
    emit scanningChanged();
}

void Controller::startScan() {
    QList<ScanTarget> targets;
    const QList<HyprWindow> visible = m_state.visibleWindows();
    for (const HyprWindow &window : visible) {
        const bool active = window.address == m_state.activeAddress;
        if (m_scope == Config::Scope::Window && !active)
            continue;
        ScanTarget target;
        target.window = window;
        target.active = active;
        if (const HyprMonitor *monitor = m_state.monitorFor(window)) {
            target.monitorScale = monitor->scale;
            target.monitorRect = monitor->rect;
        }
        targets.append(target);
    }
    // No focused window (an empty workspace): look at everything visible.
    if (targets.isEmpty() && m_scope == Config::Scope::Window)
        for (const HyprWindow &window : visible) {
            ScanTarget target;
            target.window = window;
            if (const HyprMonitor *monitor = m_state.monitorFor(window)) {
                target.monitorScale = monitor->scale;
                target.monitorRect = monitor->rect;
            }
            targets.append(target);
        }
    m_scanner.start(targets, m_config.scanTimeoutMs);
    emit scanningChanged();
}

void Controller::scanned(const ScanReport &report) {
    m_elements = report.elements;
    m_inaccessible = report.inaccessible;
    m_scanMilliseconds = report.milliseconds;
    m_error.clear();
    if (!report.error.isEmpty() && report.elements.isEmpty())
        m_error = report.error;
    else if (!Atspi::isEnabled())
        m_error = tr("Accessibility is off, so apps aren't sharing their buttons. Run pounce setup.");
    emit scanningChanged();
    emit statusChanged();
    refresh();

    // A label typed while results were still arriving acts now.
    if (m_open && m_parsed.hintMode && !m_parsed.hint.isEmpty()) {
        const int row = Ranker::findHint(m_model.all(), m_parsed.hint);
        if (row >= 0)
            activate(row, m_parsed.rightClick ? "right" : "click");
    }
}

void Controller::setQuery(const QString &query) {
    if (query == m_query)
        return;
    m_query = query;
    m_parsed = Ranker::Query::parse(query);
    emit queryChanged();
    m_selected = 0;
    refresh();
    emit selectedChanged();

    // A complete label acts at once, as in Vimium, but only once labels have
    // stopped moving: while the scan streams in, a label can change hands.
    if (m_parsed.hintMode && !m_parsed.hint.isEmpty() && hintsReady()) {
        const int row = Ranker::findHint(m_model.all(), m_parsed.hint);
        if (row >= 0)
            activate(row, m_parsed.rightClick ? "right" : "click");
    }
}

void Controller::refresh() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto boost = [this, &now](const Element &element) { return m_history.boost(element.key(), now); };
    QList<Ranker::Ranked> ranked = Ranker::rank(m_elements, m_parsed.search, m_config.hintAlphabet, boost);

    // While a label is being typed, only the elements it can still reach
    // stay listed.
    if (m_parsed.hintMode && !m_parsed.hint.isEmpty()) {
        QList<Ranker::Ranked> reachable;
        for (const auto &r : ranked)
            if (r.hint.startsWith(m_parsed.hint))
                reachable.append(r);
        ranked = reachable;
    }
    m_model.set(m_elements, ranked);
    if (m_selected >= m_model.rowCount())
        m_selected = std::max(0, m_model.rowCount() - 1);
    emit resultsChanged();
    emit selectedChanged();
}

void Controller::setSelected(int row) {
    row = std::clamp(row, 0, std::max(0, m_model.rowCount() - 1));
    if (row == m_selected)
        return;
    m_selected = row;
    emit selectedChanged();
}

QRectF Controller::selectedRect() const {
    const Element *element = m_model.element(m_selected);
    return element ? element->rect : QRectF();
}

void Controller::move(int delta) {
    const int rows = m_model.rowCount();
    if (rows == 0)
        return;
    setSelected(((m_selected + delta) % rows + rows) % rows);
}

void Controller::toggleScope() {
    m_scope = m_scope == Config::Scope::Window ? Config::Scope::Screen : Config::Scope::Window;
    emit scopeChanged();
    rescan();
}

void Controller::rescan() {
    m_state = Hyprland::state();
    startScan();
}

void Controller::activateSelected(const QString &mode) {
    activate(m_selected, mode);
}

void Controller::activate(int row, const QString &modeName) {
    const Element *found = m_model.element(row);
    if (!found)
        return;
    Actuator::Mode mode = Actuator::Mode::Primary;
    Actuator::parseMode(modeName, &mode);
    const Element element = *found;

    // The overlay has to be gone before anything happens: a pointer click
    // would land on it, and the target window can't take keyboard focus
    // while it holds the keyboard. The layer surface unmaps on the next
    // frame, so act shortly after.
    close();
    QTimer::singleShot(60, this, [this, element, mode] {
        const auto outcome = m_actuator.perform(element, mode, m_config, m_state);
        if (outcome.ok) {
            m_history.record(element.key());
            m_history.save();
        } else {
            QProcess::startDetached("notify-send",
                                    {"-a", "pounce", "-i", "pounce", tr("Couldn't press %1").arg(element.label()),
                                     outcome.error});
        }
    });
}
