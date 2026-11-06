#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>

// Box art for a game: a picture you put in ~/Games/art/<system>/ wins, then
// the local cache, then libretro's thumbnail server. A game with no art keeps
// its drawn cover, so a missing picture never leaves a hole in the shelf.
class Artwork : public QObject {
    Q_OBJECT

public:
    explicit Artwork(const QString &gamesRoot, const QString &cacheDirectory,
                     const QString &baseUrl = "https://thumbnails.libretro.com", QObject *parent = nullptr);

    // A local picture if there is one, without touching the network.
    QString cached(const QString &system, const QString &baseName) const;
    void request(const QString &key, const QString &system, const QString &baseName);

    // When a file's name isn't one the server uses, this picks the cover it most
    // likely means from the server's own list, or nothing rather than a wrong game.
    static QString closestBoxart(const QString &baseName, const QStringList &catalog);
    static QStringList parseCatalog(const QByteArray &indexPage);

signals:
    void ready(const QString &key, const QString &path);

private:
    struct Job {
        QString key, system, baseName;
        QStringList candidates;
        bool searched = false;
    };

    void pump();
    void fetch(Job job);
    void search(Job job);
    void fetchCatalog(const QString &system);
    void finish(const Job &job, bool remember);
    QString cachePath(const QString &system, const QString &baseName) const;
    QString missPath(const QString &system, const QString &baseName) const;
    bool recentlyMissed(const QString &system, const QString &baseName) const;

    QString m_gamesRoot, m_cache, m_baseUrl;
    QNetworkAccessManager m_network;
    QQueue<Job> m_queue;
    QHash<QString, QStringList> m_catalogs;
    QHash<QString, QList<Job>> m_waiting;
    int m_active = 0;
};
