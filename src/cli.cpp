#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <cstdio>

#include "actuator.h"
#include "atspi.h"
#include "config.h"
#include "history.h"
#include "hyprland.h"
#include "pointer.h"
#include "ranker.h"
#include "scanner.h"

namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

struct Options {
    QStringList words;
    bool json = false;
    bool screen = false;
    bool dryRun = false;
    int index = 0;
    QString address;
    Actuator::Mode mode = Actuator::Mode::Primary;
    QString error;
};

Options parse(const QStringList &arguments) {
    Options options;
    for (int i = 0; i < arguments.size(); ++i) {
        const QString &a = arguments[i];
        if (a == "--json")
            options.json = true;
        else if (a == "--screen")
            options.screen = true;
        else if (a == "--window")
            options.screen = false;
        else if (a == "--dry-run")
            options.dryRun = true;
        else if (a == "--right")
            options.mode = Actuator::Mode::Right;
        else if (a == "--double")
            options.mode = Actuator::Mode::Double;
        else if (a == "--middle")
            options.mode = Actuator::Mode::Middle;
        else if (a == "--hover")
            options.mode = Actuator::Mode::Hover;
        else if (a == "--focus")
            options.mode = Actuator::Mode::Focus;
        else if (a == "--address" && i + 1 < arguments.size())
            options.address = arguments[++i];
        else if (a == "--index" && i + 1 < arguments.size())
            options.index = arguments[++i].toInt() - 1;
        else if (a.startsWith("--"))
            options.error = QStringLiteral("Unknown option %1").arg(a);
        else
            options.words.append(a);
    }
    return options;
}

ScanReport scanOnce(bool screen, const HyprState &state, const Config &config, const QString &address) {
    QList<ScanTarget> targets;
    // A window named by address is read even when it isn't on screen, so a
    // script can work in a window on another workspace.
    const QList<HyprWindow> windows = address.isEmpty() ? state.visibleWindows() : state.windows;
    for (const HyprWindow &window : windows) {
        const bool active = window.address == state.activeAddress;
        if (!address.isEmpty() ? window.address != address : (!screen && !active))
            continue;
        ScanTarget target;
        target.window = window;
        target.active = active;
        if (const HyprMonitor *monitor = state.monitorFor(window)) {
            target.monitorScale = monitor->scale;
            target.monitorRect = address.isEmpty() ? monitor->rect : window.rect;
        }
        targets.append(target);
    }
    Scanner scanner;
    ScanReport report;
    QEventLoop loop;
    QObject::connect(&scanner, &Scanner::finished, &loop, [&](const ScanReport &r) {
        report = r;
        loop.quit();
    });
    scanner.start(targets, config.scanTimeoutMs);
    if (scanner.running())
        loop.exec();
    return report;
}

// The focused window is usually the terminal pounce was run from, which has
// nothing to click; then the rest of the screen is what was meant.
ScanReport scan(bool screen, const HyprState &state, const Config &config, const QString &address = {}) {
    ScanReport report = scanOnce(screen, state, config, address);
    if (!screen && address.isEmpty() && report.elements.isEmpty()) {
        const HyprWindow *active = state.active();
        if (!active || Hyprland::isTerminalClass(active->cls) || report.inaccessible.contains(active->cls))
            report = scanOnce(true, state, config, address);
    }
    QStringList relevant;
    for (const QString &cls : report.inaccessible)
        if (!Hyprland::isTerminalClass(cls))
            relevant.append(cls);
    report.inaccessible = relevant;
    return report;
}

QJsonObject toJson(const Element &element, const Ranker::Ranked *ranked = nullptr) {
    QJsonObject object{
        {"label", element.label()},
        {"name", element.name},
        {"description", element.description},
        {"role", element.roleName},
        {"app", element.app},
        {"window", element.windowTitle},
        {"x", qRound(element.rect.x())},
        {"y", qRound(element.rect.y())},
        {"width", qRound(element.rect.width())},
        {"height", qRound(element.rect.height())},
        {"actions", QJsonArray::fromStringList(element.actions)},
        {"editable", element.editable},
        {"checked", element.checked},
        {"focused", element.focused},
    };
    if (ranked) {
        object.insert("hint", ranked->hint);
        object.insert("score", ranked->score);
    }
    return object;
}

bool requireHyprland(bool json) {
    if (Hyprland::available())
        return true;
    if (json)
        out() << QJsonDocument(QJsonObject{{"error", "Hyprland isn't running."}}).toJson(QJsonDocument::Compact) << "\n";
    err() << "pounce: Hyprland isn't running (HYPRLAND_INSTANCE_SIGNATURE is unset).\n";
    return false;
}

int list(const QStringList &arguments) {
    const Options options = parse(arguments);
    if (!options.error.isEmpty()) {
        err() << "pounce: " << options.error << "\n";
        return 1;
    }
    if (!requireHyprland(options.json))
        return 1;
    const Config config = Config::load();
    const HyprState state = Hyprland::state();
    const ScanReport report = scan(options.screen, state, config, options.address);
    if (!report.error.isEmpty() && report.elements.isEmpty()) {
        err() << "pounce: " << report.error << "\n";
        return 1;
    }

    History history;
    const QList<Ranker::Ranked> ranked = Ranker::rank(
        report.elements, options.words.join(' '), config.hintAlphabet,
        [&history](const Element &e) { return history.boost(e.key()); }, 100000);

    if (options.json) {
        QJsonArray elements;
        for (const auto &r : ranked)
            elements.append(toJson(report.elements[r.index], &r));
        out() << QJsonDocument(QJsonObject{{"elements", elements},
                                           {"inaccessible", QJsonArray::fromStringList(report.inaccessible)},
                                           {"restart", QJsonArray::fromStringList(report.restart)},
                                           {"milliseconds", report.milliseconds},
                                           {"nodes", report.nodes},
                                           {"timedOut", report.timedOut}})
                     .toJson(QJsonDocument::Indented);
        return 0;
    }

    for (const auto &r : ranked) {
        const Element &e = report.elements[r.index];
        out() << QString("%1  %2  %3  (%4)  @%5,%6 %7x%8\n")
                     .arg(r.hint, -4)
                     .arg(e.roleName, -10)
                     .arg(e.label().isEmpty() ? QStringLiteral("—") : e.label())
                     .arg(e.app)
                     .arg(qRound(e.rect.x()))
                     .arg(qRound(e.rect.y()))
                     .arg(qRound(e.rect.width()))
                     .arg(qRound(e.rect.height()));
    }
    err() << QString("%1 elements, %2 accessibility nodes read in %3 ms%4\n")
                 .arg(ranked.size())
                 .arg(report.nodes)
                 .arg(report.milliseconds)
                 .arg(report.timedOut ? " (timed out)" : "");
    if (!report.restart.isEmpty())
        err() << report.restart.join(", ")
              << " was started before accessibility was switched on; quit it completely and open it again.\n";
    else if (!report.inaccessible.isEmpty())
        err() << "No accessibility tree from: " << report.inaccessible.join(", ") << " (see `pounce doctor`)\n";
    return 0;
}

int click(const QStringList &arguments) {
    const Options options = parse(arguments);
    if (!options.error.isEmpty() || options.words.isEmpty()) {
        err() << "pounce: " << (options.error.isEmpty() ? QStringLiteral("click needs something to search for")
                                                        : options.error)
              << "\n";
        return 1;
    }
    if (!requireHyprland(options.json))
        return 1;
    const Config config = Config::load();
    const HyprState state = Hyprland::state();
    const ScanReport report = scan(options.screen, state, config, options.address);

    History history;
    const QList<Ranker::Ranked> ranked = Ranker::rank(
        report.elements, options.words.join(' '), config.hintAlphabet,
        [&history](const Element &e) { return history.boost(e.key()); });
    if (options.index < 0 || options.index >= ranked.size()) {
        const QString message = ranked.isEmpty() ? QStringLiteral("Nothing matches \"%1\".").arg(options.words.join(' '))
                                                 : QStringLiteral("Only %1 matches.").arg(ranked.size());
        if (options.json)
            out() << QJsonDocument(QJsonObject{{"ok", false}, {"error", message}}).toJson(QJsonDocument::Compact) << "\n";
        err() << "pounce: " << message << "\n";
        return 1;
    }

    const Element element = report.elements[ranked[options.index].index];
    Actuator::Outcome outcome{true, "dry-run", {}};
    if (!options.dryRun) {
        Actuator actuator;
        outcome = actuator.perform(element, options.mode, config, state);
        if (outcome.ok) {
            history.record(element.key());
            history.save();
        }
    }
    if (options.json) {
        out() << QJsonDocument(QJsonObject{{"ok", outcome.ok},
                                           {"mode", Actuator::modeName(options.mode)},
                                           {"method", outcome.method},
                                           {"error", outcome.error},
                                           {"element", toJson(element, &ranked[options.index])}})
                     .toJson(QJsonDocument::Compact)
              << "\n";
    } else {
        out() << (outcome.ok ? "" : "Failed: ") << Actuator::modeName(options.mode) << " " << element.roleName
              << " \"" << element.label() << "\" in " << element.app << " via " << outcome.method << "\n";
    }
    if (!outcome.ok)
        err() << "pounce: " << outcome.error << "\n";
    return outcome.ok ? 0 : 1;
}

QString flagsFile(const QString &name) {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/" + name;
}

bool fileHasLine(const QString &path, const QString &line) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    for (const QByteArray &existing : file.readAll().split('\n'))
        if (QString::fromUtf8(existing).trimmed() == line)
            return true;
    return false;
}

// Appends a line to a flags file, written beside it and renamed so the
// browser never reads half a file.
bool appendLine(const QString &path, const QString &line, QString *error) {
    QByteArray contents;
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly))
        contents = existing.readAll();
    if (!contents.isEmpty() && !contents.endsWith('\n'))
        contents += '\n';
    contents += line.toUtf8() + '\n';
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

const QString ChromiumFlag = QStringLiteral("--force-renderer-accessibility");

struct Check {
    QString name;
    bool ok;
    QString detail;
    QString fix;
};

int doctor(const QStringList &arguments) {
    const Options options = parse(arguments);
    QList<Check> checks;

    const bool hypr = Hyprland::available() && !Hyprland::request("j/version").isEmpty();
    checks.append({"hyprland", hypr, hypr ? "Hyprland's command socket answers." : "Hyprland isn't reachable.",
                   hypr ? QString() : "Run pounce inside a Hyprland session."});

    QString waylandError;
    const QStringList globals = Pointer::waylandGlobals(&waylandError);
    const bool layer = globals.contains("zwlr_layer_shell_v1");
    const bool pointer = globals.contains("zwlr_virtual_pointer_manager_v1");
    checks.append({"layer-shell", layer, layer ? "The compositor can show overlays." : (waylandError.isEmpty() ? "zwlr_layer_shell_v1 is missing." : waylandError), {}});
    checks.append({"virtual-pointer", pointer,
                   pointer ? "Right, middle, and double clicks and pointer fallback work."
                           : "zwlr_virtual_pointer_manager_v1 is missing, so only accessibility actions work.", {}});

    QString busError;
    const bool bus = Atspi::bus(&busError).isConnected();
    checks.append({"accessibility-bus", bus, bus ? "The accessibility bus is running." : busError,
                   bus ? QString() : "Install at-spi2-core."});
    const bool enabled = Atspi::isEnabled();
    checks.append({"accessibility-enabled", enabled,
                   enabled ? "Apps are told to share their interface." : "Accessibility is switched off.",
                   enabled ? QString() : "Run pounce setup."});

    const bool chromiumFlag = fileHasLine(flagsFile("chromium-flags.conf"), ChromiumFlag);
    checks.append({"chromium", chromiumFlag,
                   chromiumFlag ? "Chromium builds its accessibility tree at launch."
                                : "Chromium only builds its accessibility tree when started with " + ChromiumFlag + ".",
                   chromiumFlag ? QString() : "Run pounce setup, then restart Chromium."});

    QStringList inaccessible, restartNeeded;
    int elements = 0;
    if (hypr && bus) {
        const ScanReport report = scan(true, Hyprland::state(), Config::load());
        inaccessible = report.inaccessible;
        restartNeeded = report.restart;
        elements = int(report.elements.size());
        checks.append({"windows", inaccessible.isEmpty(),
                       inaccessible.isEmpty()
                           ? QStringLiteral("Every visible window shares its interface (%1 elements in %2 ms).")
                                 .arg(elements).arg(report.milliseconds)
                           : QStringLiteral("No accessibility tree from: %1.").arg(inaccessible.join(", ")),
                       inaccessible.isEmpty() ? QString()
                       : !restartNeeded.isEmpty()
                           ? QStringLiteral("%1 was started before accessibility was switched on. Quit it "
                                            "completely (every window) and open it again.")
                                 .arg(restartNeeded.join(", "))
                           : QStringLiteral("Run pounce setup, then restart those apps. Terminals and games "
                                            "never have one.")});
    }

    bool allOk = true;
    for (const Check &check : checks)
        allOk = allOk && (check.ok || check.name == "windows" || check.name == "chromium");

    if (options.json) {
        QJsonArray array;
        for (const Check &check : checks)
            array.append(QJsonObject{{"check", check.name}, {"ok", check.ok}, {"detail", check.detail}, {"fix", check.fix}});
        out() << QJsonDocument(QJsonObject{{"ok", allOk}, {"checks", array}}).toJson(QJsonDocument::Indented);
    } else {
        for (const Check &check : checks) {
            out() << (check.ok ? "  ✓ " : "  ✗ ") << check.detail << "\n";
            if (!check.fix.isEmpty())
                out() << "      → " << check.fix << "\n";
        }
    }
    return allOk ? 0 : 1;
}

int setup(const QStringList &arguments) {
    const Options options = parse(arguments);
    QStringList done, failed, restart;

    QString error;
    if (Atspi::isEnabled())
        done << "Accessibility was already on.";
    else if (Atspi::setEnabled(true, &error))
        done << "Switched accessibility on for this session.";
    else
        failed << "Couldn't switch accessibility on: " + error;

    // The session flag above is forgotten at logout; this setting is what
    // the accessibility bus reads at login.
    if (!QStandardPaths::findExecutable("gsettings").isEmpty()) {
        const int code = QProcess::execute("gsettings", {"set", "org.gnome.desktop.interface", "toolkit-accessibility", "true"});
        if (code == 0)
            done << "Accessibility stays on after logging out.";
        else
            failed << "gsettings couldn't save toolkit-accessibility.";
    }

    // Arch's Chromium and system Electron read the first two. Apps that
    // bundle their own Electron read a file named after themselves, when
    // their launcher reads one at all.
    struct FlagsFile {
        QString name;
        QString launcher;
        QString app;
    };
    const QList<FlagsFile> flagFiles{
        {"chromium-flags.conf", {}, "Chromium"},
        {"electron-flags.conf", {}, "Electron apps"},
        {"claude-desktop-flags.conf", "claude-desktop", "Claude"},
        {"codex-flags.conf", "chatgpt", "ChatGPT"},
    };
    for (const FlagsFile &flags : flagFiles) {
        if (!flags.launcher.isEmpty() && QStandardPaths::findExecutable(flags.launcher).isEmpty())
            continue;
        const QString &name = flags.name;
        const QString path = flagsFile(name);
        if (fileHasLine(path, ChromiumFlag)) {
            done << QStringLiteral("%1 already has %2.").arg(name, ChromiumFlag);
        } else if (appendLine(path, ChromiumFlag, &error)) {
            done << QStringLiteral("Added %1 to %2.").arg(ChromiumFlag, path);
            restart << flags.app;
        } else {
            failed << QStringLiteral("Couldn't write %1: %2").arg(path, error);
        }
    }

    if (options.json) {
        out() << QJsonDocument(QJsonObject{{"ok", failed.isEmpty()},
                                           {"done", QJsonArray::fromStringList(done)},
                                           {"failed", QJsonArray::fromStringList(failed)},
                                           {"restart", QJsonArray::fromStringList(restart)}})
                     .toJson(QJsonDocument::Indented);
    } else {
        for (const QString &line : done)
            out() << "  ✓ " << line << "\n";
        for (const QString &line : failed)
            out() << "  ✗ " << line << "\n";
        if (!restart.isEmpty())
            out() << "\nRestart " << restart.join(" and ")
                  << " so they build their accessibility tree. Apps already running before accessibility was "
                     "switched on need a restart too.\n";
    }
    return failed.isEmpty() ? 0 : 1;
}

int skill(const QStringList &arguments) {
    if (arguments.value(0) != "install") {
        err() << "pounce: usage: pounce skill install\n";
        return 1;
    }
    QFile source(":/skill/SKILL.md");
    if (!source.open(QIODevice::ReadOnly)) {
        err() << "pounce: the bundled skill is missing\n";
        return 1;
    }
    const QString path = QDir::homePath() + "/.claude/skills/pounce/SKILL.md";
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile target(path);
    if (!target.open(QIODevice::WriteOnly) || target.write(source.readAll()) < 0 || !target.commit()) {
        err() << "pounce: couldn't write " << path << ": " << target.errorString() << "\n";
        return 1;
    }
    out() << "Installed " << path << "\n";
    return 0;
}

}

namespace Cli {

bool handles(const QString &command) {
    static const QStringList commands{"list", "click", "doctor", "setup", "skill", "help", "--help", "-h", "--version", "-v"};
    return commands.contains(command);
}

QString usage() {
    return QStringLiteral(
        "pounce — click anything on screen from the keyboard\n"
        "\n"
        "Usage:\n"
        "  pounce                 Open the search overlay (or close it)\n"
        "  pounce hints           Open with a label on every element\n"
        "  pounce daemon          Start in the background without opening\n"
        "  pounce quit            Stop the background instance\n"
        "\n"
        "  pounce list [text]     List clickable elements, best match first\n"
        "  pounce click <text>    Click the best match\n"
        "      --right --double --middle --hover --focus   what to do instead of a click\n"
        "      --index N          take the Nth match instead of the first\n"
        "      --dry-run          say what would be clicked, without clicking\n"
        "  pounce doctor          Check that everything pounce needs is in place\n"
        "  pounce setup           Switch on accessibility for apps and Chromium\n"
        "  pounce skill install   Install the agent skill in ~/.claude/skills\n"
        "\n"
        "  --screen               Look at every visible window, not just the focused one\n"
        "  --address 0x…          Look inside one window, by its Hyprland address\n"
        "  --json                 Machine-readable output\n");
}

int run(const QStringList &arguments) {
    const QString command = arguments.value(0);
    const QStringList rest = arguments.mid(1);
    if (command == "list")
        return list(rest);
    if (command == "click")
        return click(rest);
    if (command == "doctor")
        return doctor(rest);
    if (command == "setup")
        return setup(rest);
    if (command == "skill")
        return skill(rest);
    if (command == "--version" || command == "-v") {
        out() << "pounce " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    out() << usage();
    return 0;
}

}
