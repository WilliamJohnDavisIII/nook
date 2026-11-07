#include "launcher.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QStandardPaths>
#include <csignal>
#include <unistd.h>

namespace {
constexpr qint64 EarlyExitMs = 4000;
constexpr int StopGraceMs = 8000;

bool hasPs1Bios(const QString &biosDirectory) {
    for (const char *name : {"scph5500.bin", "scph5501.bin", "scph5502.bin"})
        if (QFileInfo::exists(biosDirectory + "/" + QLatin1String(name)))
            return true;
    return false;
}

bool hasPs2Bios(const QString &directory) {
    // A PS2 BIOS is a 4 MB image or larger; the name doesn't matter to the emulator.
    const auto files = QDir(directory).entryInfoList(QDir::Files);
    for (const auto &file : files)
        if (file.size() >= 4 * 1024 * 1024)
            return true;
    return false;
}
}

LaunchPaths Launcher::defaultPaths(const QString &gamesRoot) {
    LaunchPaths paths;
    paths.gamesRoot = gamesRoot;
    paths.retroarch = QStandardPaths::findExecutable("retroarch");
    paths.dolphin = QStandardPaths::findExecutable("dolphin-emu");

    // Dolphin only runs through X11. With Hyprland's force_zero_scaling, X11 apps
    // get raw pixels, so on a scaled screen its text and menus come out tiny unless
    // it's told the scale itself.
    const QString hyprctl = QStandardPaths::findExecutable("hyprctl");
    if (!paths.dolphin.isEmpty() && !hyprctl.isEmpty() && qGuiApp && qGuiApp->primaryScreen()) {
        QProcess option;
        option.start(hyprctl, {"-j", "getoption", "xwayland:force_zero_scaling"});
        if (option.waitForFinished(500))
            paths.x11Scale = x11ScaleFor(qGuiApp->primaryScreen()->devicePixelRatio(), option.readAllStandardOutput());
    }

    QFile retroarchConfig(QDir::homePath() + "/.config/retroarch/retroarch.cfg");
    if (retroarchConfig.open(QIODevice::ReadOnly | QIODevice::Text))
        paths.discRecords = discRecordFolder(QString::fromUtf8(retroarchConfig.readAll()), QDir::homePath());

    // The newest AppImage wins by version order, so updating ARMSX2 is just
    // dropping the new file in the folder. Only the build for this machine's
    // memory page size can run here.
    const QString pageBuild = sysconf(_SC_PAGESIZE) > 4096 ? "*16K-pages.AppImage" : "*4K-pages.AppImage";
    QDir armsx2(QDir::homePath() + "/.local/opt/armsx2");
    armsx2.setSorting(QDir::Name | QDir::Reversed);
    armsx2.setFilter(QDir::Files | QDir::Executable);
    const auto builds = armsx2.entryInfoList({pageBuild});
    if (!builds.isEmpty())
        paths.armsx2 = builds.first().absoluteFilePath();
    return paths;
}

QString Launcher::discRecordFolder(const QString &retroarchConfig, const QString &home) {
    const auto setting = [&](const char *name) {
        const QRegularExpression line(QStringLiteral(R"re(^\s*%1\s*=\s*"([^"]*)")re").arg(QLatin1String(name)),
                                      QRegularExpression::MultilineOption);
        return line.match(retroarchConfig).captured(1);
    };
    QString folder = setting("savefile_directory");
    if (folder.isEmpty() || folder == "default" || folder.startsWith(':'))
        return {};
    if (folder.startsWith('~'))
        folder = home + folder.mid(1);
    // The folder sorted saves go in is named after the core's own name for itself.
    return setting("sort_savefiles_enable") == "true" ? folder + "/Beetle PSX HW" : folder;
}

void Launcher::startOnDisc(const QString &playlist, int index, const QString &discFile) {
    if (m_paths.discRecords.isEmpty())
        return;
    QDir().mkpath(m_paths.discRecords);
    QSaveFile record(m_paths.discRecords + "/" + QFileInfo(playlist).completeBaseName() + ".ldci");
    const QJsonObject note{{"version", "1.0"}, {"image_index", index}, {"image_path", discFile}};
    if (record.open(QIODevice::WriteOnly))
        record.write(QJsonDocument(note).toJson()), record.commit();
}

double Launcher::x11ScaleFor(double screenScale, const QByteArray &zeroScalingOption) {
    const bool zeroScaling = QJsonDocument::fromJson(zeroScalingOption).object().value("int").toInt() == 1;
    return zeroScaling && screenScale > 1 ? screenScale : 1;
}

LaunchPlan Launcher::plan(const QString &system, const QString &path, const LaunchPaths &paths) {
    LaunchPlan plan;
    if (!QFileInfo::exists(path)) {
        plan.problem = tr("This game's file has moved or been deleted:\n%1").arg(path);
        return plan;
    }

    if (system == "ps2") {
        if (paths.armsx2.isEmpty()) {
            plan.problem = tr("ARMSX2 isn't installed. Put its AppImage in ~/.local/opt/armsx2 to play PS2 games.");
            return plan;
        }
        if (!hasPs2Bios(paths.gamesRoot + "/bios/ps2")) {
            plan.problem = tr("PS2 games need a BIOS. Put one in %1/bios/ps2.").arg(paths.gamesRoot);
            return plan;
        }
        plan.program = paths.armsx2;
        plan.arguments = {"-nogui", "-fullscreen", "--", path};
        return plan;
    }

    // Standalone Dolphin wins when it's installed: on some systems (Apple Silicon
    // Linux among them) the RetroArch core can't start its renderer at all.
    if (system == "gc" && !paths.dolphin.isEmpty()) {
        plan.program = paths.dolphin;
        // The statistics question would otherwise pop up over a controller-only
        // launch with no way to answer it from the pad, so it's answered here: no.
        plan.arguments = {"--batch", "--config=Dolphin.Display.Fullscreen=True",
                          "--config=Dolphin.Analytics.PermissionAsked=True", "--config=Dolphin.Analytics.Enabled=False",
                          // When the game asks for the next disc, put it in.
                          "--config=Dolphin.Core.AutoDiscChange=True",
                          "--exec=" + path};
        if (paths.x11Scale > 1)
            plan.environment << "QT_SCALE_FACTOR=" + QString::number(paths.x11Scale);
        return plan;
    }

    QString core;
    if (system == "snes")
        core = "snes9x";
    else if (system == "ps1")
        core = "mednafen_psx_hw";
    else if (system == "gc")
        core = "dolphin";
    else {
        plan.problem = tr("Nook doesn't know how to play \"%1\" games.").arg(system);
        return plan;
    }

    if (paths.retroarch.isEmpty()) {
        plan.problem = tr("RetroArch isn't installed.");
        return plan;
    }
    const QString corePath = paths.coreDirectory + "/" + core + "_libretro.so";
    if (!QFileInfo::exists(corePath)) {
        plan.problem = tr("The emulator for this game is missing:\n%1").arg(corePath);
        return plan;
    }
    if (system == "ps1" && !hasPs1Bios(paths.gamesRoot + "/bios")) {
        plan.problem = tr("PS1 games need a BIOS. Put scph5501.bin in %1/bios.").arg(paths.gamesRoot);
        return plan;
    }
    if (system == "gc" && !QFileInfo::exists(paths.gamesRoot + "/bios/dolphin-emu/Sys")) {
        plan.problem = tr("GameCube games need Dolphin's Sys folder in %1/bios/dolphin-emu.").arg(paths.gamesRoot);
        return plan;
    }
    plan.program = paths.retroarch;
    plan.arguments = {"-f", "-L", corePath, path};
    return plan;
}

bool Launcher::isFailedStart(qint64 elapsedMs, int exitCode, QProcess::ExitStatus status, bool stopRequested) {
    if (stopRequested)
        return false;
    if (status == QProcess::CrashExit)
        return true;
    // ARMSX2 exits with 1 even for -help, so a code means little on its own.
    // Failing straight away with one does: nobody finishes a game in 4 seconds.
    return elapsedMs < EarlyExitMs && exitCode != 0;
}

Launcher::Launcher(const LaunchPaths &paths, QObject *parent) : QObject(parent), m_paths(paths) {
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(StopGraceMs);
    connect(&m_killTimer, &QTimer::timeout, this, [this] { signalGroup(SIGKILL); });
}

Launcher::~Launcher() {
    if (m_process) {
        // Nook is going away, but a game in progress shouldn't go with it.
        m_process->disconnect(this);
        m_process->setParent(nullptr);
    }
}

void Launcher::setError(const QString &error) {
    if (error == m_lastError)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void Launcher::clearError() {
    setError({});
}

void Launcher::signalGroup(int signal) {
    if (m_process && m_process->processId() > 0)
        ::kill(-pid_t(m_process->processId()), signal);
}

bool Launcher::launch(const QString &system, const QString &path) {
    if (m_process)
        return false;

    const LaunchPlan plan = Launcher::plan(system, path, m_paths);
    if (!plan.problem.isEmpty()) {
        setError(plan.problem);
        return false;
    }

    auto *process = new QProcess(this);
    process->setProgram(plan.program);
    process->setArguments(plan.arguments);
    process->setWorkingDirectory(QDir::homePath());
    process->setProcessChannelMode(QProcess::MergedChannels);
    if (!plan.environment.isEmpty())
        process->setEnvironment(QProcess::systemEnvironment() + plan.environment);
    // Its own process group, because an AppImage is a launcher wrapped around
    // the real program: stopping a game has to reach both.
    process->setChildProcessModifier([] { ::setsid(); });

    // Keep only the tail of the emulator's chatter, for the error message.
    connect(process, &QProcess::readyRead, process, [process] {
        const QByteArray tail = process->property("tail").toByteArray() + process->readAll();
        process->setProperty("tail", tail.right(2000));
    });

    connect(process, &QProcess::errorOccurred, this, [this, process, plan](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        setError(tr("%1 couldn't start.").arg(QFileInfo(plan.program).fileName()));
        process->deleteLater();
        m_process = nullptr;
        emit runningChanged();
    });

    connect(process, &QProcess::finished, this, [this, process, plan](int code, QProcess::ExitStatus status) {
        m_killTimer.stop();
        if (isFailedStart(m_elapsed.elapsed(), code, status, m_stopRequested)) {
            const QString tail = QString::fromUtf8(process->property("tail").toByteArray()).trimmed().right(400);
            setError(tail.isEmpty() ? tr("%1 closed right away.").arg(QFileInfo(plan.program).fileName())
                                    : tr("%1 closed right away:\n%2").arg(QFileInfo(plan.program).fileName(), tail));
        }
        process->deleteLater();
        m_process = nullptr;
        emit runningChanged();
    });

    m_stopRequested = false;
    m_process = process;
    m_elapsed.start();
    clearError();
    process->start();
    emit runningChanged();
    return m_process != nullptr;
}

void Launcher::stop() {
    if (!m_process || m_stopRequested)
        return;
    m_stopRequested = true;
    // A polite request first, so the emulator can write its saves.
    signalGroup(SIGTERM);
    m_killTimer.start();
}
