#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

struct LaunchPlan {
    QString program;
    QStringList arguments;
    // Extra environment variables, as NAME=value.
    QStringList environment;
    // Why this game can't start, in words a person can act on. Empty when it can.
    QString problem;
};

struct LaunchPaths {
    QString gamesRoot;
    QString coreDirectory = "/usr/lib/libretro";
    QString retroarch;
    QString armsx2;
    QString dolphin;
    // What an X11 app must scale itself by to match the screen. 1 unless the
    // compositor hands X11 apps raw pixels on a scaled display.
    double x11Scale = 1;
    // Where RetroArch keeps its note of each PS1 game's last disc. Empty if unknown.
    QString discRecords;
};

// Starts a game in the right emulator and watches it until it ends.
class Launcher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit Launcher(const LaunchPaths &paths, QObject *parent = nullptr);
    ~Launcher() override;

    bool running() const { return m_process != nullptr; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE bool launch(const QString &system, const QString &path);
    // Has RetroArch start this playlist on a disc, by the same note it writes
    // itself on the way out. It keeps the disc order, and with it the memory card.
    Q_INVOKABLE void startOnDisc(const QString &playlist, int index, const QString &discFile);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clearError();

    static LaunchPlan plan(const QString &system, const QString &path, const LaunchPaths &paths);
    static LaunchPaths defaultPaths(const QString &gamesRoot);
    static QString discRecordFolder(const QString &retroarchConfig, const QString &home);
    static double x11ScaleFor(double screenScale, const QByteArray &zeroScalingOption);
    // Picked out so tests can pin down the rule for what counts as a failed launch.
    static bool isFailedStart(qint64 elapsedMs, int exitCode, QProcess::ExitStatus status, bool stopRequested);

signals:
    void runningChanged();
    void lastErrorChanged();

private:
    void signalGroup(int signal);
    void setError(const QString &error);

    LaunchPaths m_paths;
    QString m_lastError;
    QProcess *m_process = nullptr;
    QElapsedTimer m_elapsed;
    QTimer m_killTimer;
    bool m_stopRequested = false;
};
