#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include "atspi.h"
#include "config.h"
#include "controller.h"
#include "geometry.h"
#include "history.h"
#include "hyprland.h"
#include "matcher.h"
#include "pointer.h"
#include "ranker.h"
#include "resultsmodel.h"
#include "theme.h"
#include "translator.h"

namespace {

Element element(const QString &name, int order = 0, bool active = true) {
    Element e;
    e.name = name;
    e.order = order;
    e.inActiveWindow = active;
    e.app = "test";
    e.rect = QRectF(order * 10, 0, 10, 10);
    return e;
}

}

class PounceTests : public QObject {
    Q_OBJECT

private slots:
    // Matching

    void exactBeatsPrefixBeatsWordBeatsScattered() {
        const int exact = Matcher::match("save", "Save")->score;
        const int prefix = Matcher::match("save", "Save As…")->score;
        const int word = Matcher::match("save", "Auto save")->score;
        const int inside = Matcher::match("ave", "Save")->score;
        const int scattered = Matcher::match("sve", "Save")->score;
        QVERIFY(exact > prefix);
        QVERIFY(prefix > word);
        QVERIFY(word > inside);
        QVERIFY(inside > scattered);
    }

    void initialsMatchWords() {
        const auto hit = Matcher::match("sa", "Save As");
        QVERIFY(hit);
        QCOMPARE(hit->positions, (QList<int>{0}) + QList<int>{1});
        const auto initials = Matcher::match("nt", "New Tab");
        QVERIFY(initials);
        QCOMPARE(initials->positions, (QList<int>{0, 4}));
        QVERIFY(initials->score >= 600);
    }

    void wordsInAnyOrder() {
        QVERIFY(Matcher::match("tab new", "Open new tab"));
        QVERIFY(!Matcher::match("tab zebra", "Open new tab"));
    }

    void foldsCaseAndAccents() {
        QVERIFY(Matcher::match("reglages", "Réglages"));
        QVERIFY(Matcher::match("RÉG", "reglages"));
        QCOMPARE(Matcher::fold("Ünïcödé").size(), QString("Ünïcödé").size());
    }

    void camelCaseIsAWordBoundary() {
        const auto hit = Matcher::match("tc", "toggleComments");
        QVERIFY(hit);
        QCOMPARE(hit->positions, (QList<int>{0, 6}));
    }

    void rejectsWhatIsNotThere() {
        QVERIFY(!Matcher::match("xyz", "Save"));
        QVERIFY(!Matcher::match("save", ""));
    }

    // Hint labels

    void hintsArePrefixFreeAndShortestFirst() {
        for (int count : {1, 5, 14, 15, 60, 200, 900}) {
            const QStringList labels = Matcher::hints(count, "sadfjklewcmpgh");
            QCOMPARE(labels.size(), count);
            QCOMPARE(QSet<QString>(labels.cbegin(), labels.cend()).size(), count);
            for (int i = 0; i < labels.size(); ++i) {
                if (i > 0)
                    QVERIFY(labels[i - 1].size() <= labels[i].size());
                for (int j = 0; j < labels.size(); ++j)
                    if (i != j)
                        QVERIFY2(!labels[j].startsWith(labels[i]), qPrintable(labels[i] + " / " + labels[j]));
            }
        }
        QCOMPARE(Matcher::hints(3, "asd"), (QStringList{"a", "s", "d"}));
    }

    // Ranking and the query language

    void queryParsesSearchAndHint() {
        auto q = Ranker::Query::parse("save;sd");
        QCOMPARE(q.search, QString("save"));
        QCOMPARE(q.hint, QString("sd"));
        QVERIFY(q.hintMode);
        QVERIFY(!q.rightClick);
        q = Ranker::Query::parse(";sD");
        QCOMPARE(q.hint, QString("sd"));
        QVERIFY(q.rightClick);
        q = Ranker::Query::parse("plain");
        QVERIFY(!q.hintMode);
    }

    void rankingPutsBestMatchFirst() {
        const Elements elements{element("Save As", 0), element("Save", 1), element("Autosave settings", 2),
                                element("Open", 3)};
        const auto ranked = Ranker::rank(elements, "save", "asdf");
        QCOMPARE(ranked.size(), 3);
        QCOMPARE(elements[ranked[0].index].name, QString("Save"));
        QCOMPARE(elements[ranked[1].index].name, QString("Save As"));
        QCOMPARE(ranked[0].hint, QString("a"));
    }

    void emptyQueryKeepsReadingOrderAndUnnamed() {
        Elements elements{element("B", 0), element("", 1), element("A", 2)};
        elements[1].roleName = "Button";
        const auto ranked = Ranker::rank(elements, "", "asdf");
        QCOMPARE(ranked.size(), 3);
        QCOMPARE(ranked[0].index, 0);
        QCOMPARE(ranked[1].index, 1);
        QCOMPARE(Ranker::findHint(ranked, "s"), 1);
    }

    void unnamedElementsAreNotSearchable() {
        const Elements elements{element("", 0)};
        QVERIFY(Ranker::rank(elements, "button", "asdf").isEmpty());
    }

    void historyLiftsAFavourite() {
        const Elements elements{element("Save draft", 0), element("Save", 1)};
        const auto plain = Ranker::rank(elements, "sav", "asdf");
        QCOMPARE(plain[0].index, 1);
        const auto boosted = Ranker::rank(elements, "sav", "asdf",
                                          [](const Element &e) { return e.name == "Save draft" ? 200.0 : 0.0; });
        QCOMPARE(boosted[0].index, 0);
    }

    void activeWindowWinsTies() {
        const Elements elements{element("Close", 0, false), element("Close", 1, true)};
        const auto ranked = Ranker::rank(elements, "close", "asdf");
        QCOMPARE(ranked[0].index, 1);
    }

    void highlightMarksMatchedLetters() {
        QCOMPARE(ResultsModel::highlight("Save", {0, 1}, "#fff"),
                 QString("<font color=\"#fff\"><b>Sa</b></font>ve"));
        QCOMPARE(ResultsModel::highlight("a<b", {}, "#fff"), QString("a&lt;b"));
    }

    // Geometry

    void framesMatchByTitleThenSize() {
        const QList<Geometry::FrameCandidate> frames{
            {"Inbox - Chromium", QRectF(0, 0, 900, 1000), false, true},
            {"Docs - Chromium", QRectF(0, 0, 900, 1000), true, true},
            {"", QRectF(99, 25, 600, 88), false, true}, // A popup.
        };
        const QList<Geometry::WindowCandidate> windows{
            {"Docs - Chromium", QRectF(10, 40, 900, 1000), true, 2.0},
            {"Inbox - Chromium", QRectF(920, 40, 900, 1000), false, 2.0},
        };
        QCOMPARE(Geometry::assignFrames(frames, windows), (QList<int>{1, 0}));
    }

    void loneWindowTakesItsOnlyFrame() {
        const QList<Geometry::FrameCandidate> frames{{"Untitled", QRectF(0, 0, 400, 300), false, true}};
        const QList<Geometry::WindowCandidate> windows{{"Calculator", QRectF(0, 0, 640, 480), true, 1.0}};
        QCOMPARE(Geometry::assignFrames(frames, windows), (QList<int>{0}));
    }

    void ambiguousFramesAreNotGuessed() {
        const QList<Geometry::FrameCandidate> frames{{"x", QRectF(0, 0, 400, 300), false, true},
                                                     {"y", QRectF(0, 0, 410, 310), false, true}};
        const QList<Geometry::WindowCandidate> windows{{"a", QRectF(0, 0, 640, 480), false, 1.0},
                                                       {"b", QRectF(0, 0, 800, 600), false, 1.0}};
        QCOMPARE(Geometry::assignFrames(frames, windows), (QList<int>{-1, -1}));
    }

    void placesElementsInTheWindow() {
        const QRectF window(100, 50, 400, 300);
        QCOMPARE(Geometry::toLayout(QRectF(10, 20, 30, 40), QRectF(0, 0, 400, 300), window),
                 QRectF(110, 70, 30, 40));
        // An XWayland client drawing at twice the size.
        QCOMPARE(Geometry::toLayout(QRectF(20, 40, 60, 80), QRectF(0, 0, 800, 600), window),
                 QRectF(110, 70, 30, 40));
    }

    void chromiumPagesInDevicePixels() {
        const QRectF frame(0, 0, 941, 1030);
        QCOMPARE(Geometry::contentScale(QRectF(0, 174, 1900, 860), frame, 2.0), 2.0);
        QCOMPARE(Geometry::contentScale(QRectF(0, 87, 941, 900), frame, 2.0), 1.0);
        QCOMPARE(Geometry::contentScale(QRectF(0, 174, 1900, 860), frame, 1.0), 1.0);
        QCOMPARE(Geometry::toLayout(QRectF(815, 1006, 96, 24), frame, QRectF(967, 38, 941, 1030), 2.0),
                 QRectF(967 + 407.5, 38 + 503, 48, 12));
    }

    void overlapOfIdenticalRectsIsOne() {
        QCOMPARE(Geometry::overlap(QRectF(0, 0, 10, 10), QRectF(0, 0, 10, 10)), 1.0);
        QCOMPARE(Geometry::overlap(QRectF(0, 0, 10, 10), QRectF(20, 0, 10, 10)), 0.0);
    }

    // Hyprland

    void parsesMonitorsInLogicalPixels() {
        const auto monitors = HyprState::parseMonitors(QJsonDocument::fromJson(R"([
            {"id":0,"name":"eDP-1","x":0,"y":0,"width":3840,"height":2160,"scale":2,"transform":0,"focused":true,
             "activeWorkspace":{"id":3},"specialWorkspace":{"id":0}},
            {"id":1,"name":"DP-1","x":1920,"y":0,"width":1440,"height":2560,"scale":1,"transform":1,"focused":false,
             "activeWorkspace":{"id":5},"specialWorkspace":{"id":-98}}
        ])").array());
        QCOMPARE(monitors.size(), 2);
        QCOMPARE(monitors[0].rect, QRectF(0, 0, 1920, 1080));
        QCOMPARE(monitors[1].rect, QRectF(1920, 0, 2560, 1440));
        QCOMPARE(monitors[1].specialWorkspace, -98);
    }

    void visibleWindowsAreOnShownWorkspacesAndOnScreen() {
        HyprState state;
        state.monitors = HyprState::parseMonitors(QJsonDocument::fromJson(R"([
            {"id":0,"name":"eDP-1","x":0,"y":0,"width":1920,"height":1080,"scale":1,"focused":true,
             "activeWorkspace":{"id":1},"specialWorkspace":{"id":-98}}])").array());
        state.windows = HyprState::parseWindows(QJsonDocument::fromJson(R"([
            {"address":"0x1","class":"a","pid":1,"at":[0,0],"size":[900,1000],"workspace":{"id":1},"monitor":0,"focusHistoryID":2},
            {"address":"0x2","class":"b","pid":2,"at":[-900,0],"size":[900,1000],"workspace":{"id":1},"monitor":0,"focusHistoryID":3},
            {"address":"0x3","class":"c","pid":3,"at":[0,0],"size":[900,1000],"workspace":{"id":2},"monitor":0,"focusHistoryID":1},
            {"address":"0x4","class":"d","pid":4,"at":[100,100],"size":[500,500],"workspace":{"id":-98},"monitor":0,"focusHistoryID":0}
        ])").array());
        const auto visible = state.visibleWindows();
        QCOMPARE(visible.size(), 2);
        QCOMPARE(visible[0].address, QString("0x4")); // Most recently focused first.
        QCOMPARE(visible[1].address, QString("0x1"));
        QCOMPARE(state.layoutBounds(), QRectF(0, 0, 1920, 1080));
    }

    void terminalsAreRecognised() {
        QVERIFY(Hyprland::isTerminalClass("foot"));
        QVERIFY(Hyprland::isTerminalClass("Alacritty"));
        QVERIFY(Hyprland::isTerminalClass("org.gnome.Terminal"));
        QVERIFY(!Hyprland::isTerminalClass("chromium"));
    }

    void panelMovesAwayFromCrowdedAreas() {
        const QRectF monitor(0, 0, 1920, 1080);
        QCOMPARE(Controller::choosePanelSlot({}, monitor), 0);
        Elements crowdedTop;
        for (int i = 0; i < 6; ++i)
            crowdedTop.append(element("x", i));
        for (Element &e : crowdedTop)
            e.rect = QRectF(700 + e.order * 40, 200, 30, 30);
        QCOMPARE(Controller::choosePanelSlot(crowdedTop, monitor), 1);
        for (Element &e : crowdedTop)
            e.rect.moveTop(500); // Under both the top and middle places.
        QCOMPARE(Controller::choosePanelSlot(crowdedTop, monitor), 2);
        // One element of another window in the way isn't worth moving for.
        Elements one{element("x", 0, false)};
        one[0].rect = QRectF(900, 200, 30, 30);
        QCOMPARE(Controller::choosePanelSlot(one, monitor), 0);
    }

    // Accessibility actions

    void picksThePrimaryActionByName() {
        QCOMPARE(Atspi::primaryAction({"showContextMenu", "doDefault"}, false), 1);
        QCOMPARE(Atspi::primaryAction({"Press", "Set Focus", "Press"}, false), 0);
        QCOMPARE(Atspi::primaryAction({"listitem.scroll-to"}, false), -1);
        QCOMPARE(Atspi::primaryAction({"activate", "showContextMenu"}, true), -1);
        QCOMPARE(Atspi::primaryAction({"jump"}, false), 0);
        QCOMPARE(Atspi::contextMenuAction({"press", "showContextMenu"}), 1);
    }

    // The pointer's absolute coordinates.

    void absoluteCoordinatesClampToTheLayout() {
        QCOMPARE(Pointer::absoluteCoordinate(100, 0, 1920, 8), 800);
        QCOMPARE(Pointer::absoluteCoordinate(-5, 0, 1920, 8), 0);
        QCOMPARE(Pointer::absoluteCoordinate(5000, 0, 1920, 8), 1920 * 8 - 1);
        QCOMPARE(Pointer::absoluteCoordinate(-1800, -1920, 3840, 8), 120 * 8);
    }

    // Config and history

    void configFallsBackOnBadValues() {
        const Config config = Config::parse(R"({"scope":"screen","hintAlphabet":"aab","dim":5,"activation":"pointer"})");
        QCOMPARE(config.scope, Config::Scope::Screen);
        QCOMPARE(config.activation, Config::Activation::Pointer);
        QCOMPARE(config.hintAlphabet, Config().hintAlphabet);
        QCOMPARE(config.dim, Config().dim);
        QCOMPARE(Config::parse("not json").scope, Config::Scope::Window);
        QCOMPARE(Config::parse(R"({"hintAlphabet":"HJKL"})").hintAlphabet, QString("hjkl"));
    }

    void historyRemembersAndFades() {
        QTemporaryDir dir;
        const QString path = dir.filePath("history.json");
        const QDateTime now = QDateTime::currentDateTimeUtc();
        {
            History history(path);
            history.record("save", now);
            history.record("save", now);
            QVERIFY(history.save());
        }
        History history(path);
        const double fresh = history.boost("save", now);
        QVERIFY(fresh > 0);
        QVERIFY(history.boost("save", now.addDays(28)) < fresh / 2);
        QCOMPARE(history.boost("missing", now), 0.0);
    }

    // Theme and language

    void themeReadsOmarchyColors() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "accent = \"#112233\"\nbackground = \"#000001\"\nlighter_background = \"#000002\"\n"
              "foreground = \"#eeeeee\"\ndark_foreground = \"#777777\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.background(), QString("#000001"));
        QCOMPARE(theme.surface(), QString("#000002"));
        QCOMPARE(theme.foreground(), QString("#eeeeee"));
        QCOMPARE(theme.muted(), QString("#777777"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "da\n");
        Translator translator(dir.path() + "/language", ":/i18n");
        QCOMPARE(translator.language(), QString("da"));
        QCOMPARE(translator.translate("SearchPanel", "Window"), QString("Vindue"));
        QVERIFY(translator.translate("SearchPanel", "Not a string anywhere").isNull());
    }

    void everyCatalogIsValidJsonAndCoversTheSameStrings() {
        QFile english(":/i18n/en.json");
        QVERIFY(english.open(QIODevice::ReadOnly));
        const QDir dir(":/i18n");
        for (const QString &name : dir.entryList({"*.json"})) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name + ": " + error.errorString()));
            QVERIFY(doc.isObject());
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(PounceTests)
#include "pounce_tests.moc"
