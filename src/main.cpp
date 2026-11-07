// Nook — a calm little drawer for your retro games.

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include "artwork.h"
#include "gamepad.h"
#include "launcher.h"
#include "library.h"
#include "preferences.h"
#include "theme.h"
#include "translator.h"

namespace {
QString gamesRoot() {
    const QString override = qEnvironmentVariable("NOOK_GAMES_DIR");
    return override.isEmpty() ? QDir::homePath() + "/Games" : override;
}

QString statePath(const QString &file) {
    const QString override = qEnvironmentVariable("NOOK_STATE_DIR");
    const QString base = override.isEmpty() ? QDir::homePath() + "/.local/state/nook" : override;
    QDir().mkpath(base);
    return base + "/" + file;
}

// Reports every game and whether it can start, without opening a window.
int runCheck(bool asJson) {
    Library library(gamesRoot(), statePath("state.ini"));
    const LaunchPaths paths = Launcher::defaultPaths(gamesRoot());

    QJsonArray games;
    int problems = 0;
    for (const auto &game : library.all()) {
        const LaunchPlan plan = Launcher::plan(game.system, library.launchPathFor(game.path), paths);
        problems += plan.problem.isEmpty() ? 0 : 1;
        games.append(QJsonObject{{"title", game.title},
                                 {"system", game.system},
                                 {"path", game.path},
                                 {"command", QJsonValue::fromVariant(QStringList{plan.program} + plan.arguments)},
                                 {"problem", plan.problem}});
    }

    QTextStream out(stdout);
    if (asJson) {
        out << QJsonDocument(QJsonObject{{"games", games}, {"problems", problems}}).toJson();
    } else {
        out << games.size() << " games, " << problems << " with problems\n";
        for (const auto &value : std::as_const(games)) {
            const QJsonObject game = value.toObject();
            out << "  [" << game["system"].toString() << "] " << game["title"].toString();
            if (!game["problem"].toString().isEmpty())
                out << " -- " << game["problem"].toString();
            out << "\n";
        }
    }
    return problems == 0 ? 0 : 1;
}
}

int main(int argc, char *argv[]) {
    const QStringList raw = [&] {
        QStringList list;
        for (int i = 1; i < argc; ++i)
            list << QString::fromLocal8Bit(argv[i]);
        return list;
    }();
    const bool check = raw.contains("check");
    if (check)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("nook");
    app.setApplicationVersion("0.1.0");

    // Matches the window to nook.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("nook");
    app.setWindowIcon(QIcon::fromTheme("nook"));

    QCommandLineParser parser;
    parser.setApplicationDescription("A calm little drawer for your retro games.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption windowed("windowed", "Open in a window rather than fullscreen.");
    QCommandLineOption json("json", "With check: print the report as JSON.");
    parser.addOption(windowed);
    parser.addOption(json);
    parser.addPositionalArgument("check", "List every game and whether it can start, then exit.", "[check]");
    parser.process(app);

    if (check)
        return runCheck(parser.isSet(json));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    const QString root = gamesRoot();
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/art";
    Artwork artwork(root, cache);
    Library library(root, statePath("state.ini"), &artwork);
    Launcher launcher(Launcher::defaultPaths(root));
    Preferences preferences(root, statePath("settings.ini"));
    Gamepad gamepad;

    QObject::connect(&launcher, &Launcher::runningChanged, &gamepad, [&] { gamepad.setGameRunning(launcher.running()); });

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    auto *context = engine.rootContext();
    context->setContextProperty("translator", &translator);
    context->setContextProperty("theme", &theme);
    context->setContextProperty("library", &library);
    context->setContextProperty("launcher", &launcher);
    context->setContextProperty("gamepad", &gamepad);
    context->setContextProperty("preferences", &preferences);
    context->setContextProperty("startWindowed", parser.isSet(windowed));
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
