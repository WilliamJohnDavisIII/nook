#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

struct LaunchPlan {
    QString program;
    QStringList arguments;
    // Why this game can't start, in words a person can act on. Empty when it can.
    QString problem;
};

struct LaunchPaths {
    QString gamesRoot;
    QString coreDirectory = "/usr/lib/libretro";
    QString retroarch;
    QString armsx2;
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
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clearError();

    static LaunchPlan plan(const QString &system, const QString &path, const LaunchPaths &paths);
    static LaunchPaths defaultPaths(const QString &gamesRoot);
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
