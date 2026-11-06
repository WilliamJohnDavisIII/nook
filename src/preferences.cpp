#include "preferences.h"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>

namespace {
bool desktopWantsReducedMotion() {
    if (!qEnvironmentVariableIsEmpty("NOOK_REDUCED_MOTION"))
        return qEnvironmentVariable("NOOK_REDUCED_MOTION") != "0";
    const QString gsettings = QStandardPaths::findExecutable("gsettings");
    if (gsettings.isEmpty())
        return false;
    QProcess process;
    process.start(gsettings, {"get", "org.gnome.desktop.interface", "enable-animations"});
    if (!process.waitForFinished(500)) {
        process.kill();
        process.waitForFinished(200);
        return false;
    }
    return process.exitCode() == 0 && process.readAllStandardOutput().trimmed() == "false";
}
}

Preferences::Preferences(const QString &gamesRoot, const QString &settingsPath, QObject *parent)
    : QObject(parent), m_gamesRoot(gamesRoot), m_settings(std::make_unique<QSettings>(settingsPath, QSettings::IniFormat)),
      m_reducedMotion(desktopWantsReducedMotion()) {}

bool Preferences::musicEnabled() const {
    return m_settings->value("music/enabled", false).toBool();
}

void Preferences::setMusicEnabled(bool enabled) {
    if (enabled == musicEnabled())
        return;
    m_settings->setValue("music/enabled", enabled);
    m_settings->sync();
    emit musicChanged();
}

QUrl Preferences::musicUrl() const {
    const auto files = QDir(musicFolder()).entryInfoList({"*.mp3", "*.ogg", "*.opus", "*.flac", "*.m4a", "*.wav"},
                                                         QDir::Files, QDir::Name);
    return files.isEmpty() ? QUrl() : QUrl::fromLocalFile(files.first().absoluteFilePath());
}
