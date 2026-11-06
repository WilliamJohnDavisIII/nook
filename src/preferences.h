#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <memory>

// The drawer's few personal touches: ambient music, and whether things move.
class Preferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool musicEnabled READ musicEnabled WRITE setMusicEnabled NOTIFY musicChanged)
    Q_PROPERTY(QUrl musicUrl READ musicUrl NOTIFY musicChanged)
    Q_PROPERTY(QString musicFolder READ musicFolder CONSTANT)
    Q_PROPERTY(bool reducedMotion READ reducedMotion CONSTANT)

public:
    explicit Preferences(const QString &gamesRoot, const QString &settingsPath, QObject *parent = nullptr);

    bool musicEnabled() const;
    void setMusicEnabled(bool enabled);
    QUrl musicUrl() const;
    QString musicFolder() const { return m_gamesRoot + "/music"; }
    bool reducedMotion() const { return m_reducedMotion; }

signals:
    void musicChanged();

private:
    QString m_gamesRoot;
    std::unique_ptr<QSettings> m_settings;
    bool m_reducedMotion = false;
};
