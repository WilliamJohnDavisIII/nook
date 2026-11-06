#include "artwork.h"

#include "library.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

namespace {
constexpr int MaxConcurrent = 4;
constexpr qint64 MissRetrySeconds = 7 * 24 * 60 * 60;
constexpr qint64 MaxImageBytes = 16 * 1024 * 1024;
}

Artwork::Artwork(const QString &gamesRoot, const QString &cacheDirectory, const QString &baseUrl, QObject *parent)
    : QObject(parent), m_gamesRoot(gamesRoot), m_cache(cacheDirectory), m_baseUrl(baseUrl) {}

QString Artwork::cachePath(const QString &system, const QString &baseName) const {
    const QByteArray hash = QCryptographicHash::hash(baseName.toUtf8(), QCryptographicHash::Sha1).toHex();
    return m_cache + "/" + system + "/" + QString::fromLatin1(hash) + ".png";
}

QString Artwork::missPath(const QString &system, const QString &baseName) const {
    return cachePath(system, baseName) + ".miss";
}

QString Artwork::cached(const QString &system, const QString &baseName) const {
    for (const char *suffix : {".png", ".jpg", ".jpeg", ".webp"}) {
        const QString own = m_gamesRoot + "/art/" + system + "/" + baseName + QLatin1String(suffix);
        if (QFileInfo::exists(own))
            return own;
    }
    const QString path = cachePath(system, baseName);
    return QFileInfo::exists(path) ? path : QString();
}

bool Artwork::recentlyMissed(const QString &system, const QString &baseName) const {
    const QFileInfo miss(missPath(system, baseName));
    return miss.exists() && miss.lastModified().secsTo(QDateTime::currentDateTime()) < MissRetrySeconds;
}

void Artwork::request(const QString &key, const QString &system, const QString &baseName) {
    if (recentlyMissed(system, baseName) || !Library::systemById(system))
        return;
    m_queue.enqueue({key, system, baseName, Library::thumbnailCandidates(baseName)});
    pump();
}

void Artwork::pump() {
    while (m_active < MaxConcurrent && !m_queue.isEmpty()) {
        ++m_active;
        fetch(m_queue.dequeue());
    }
}

void Artwork::fetch(Job job) {
    const QString name = job.candidates.takeFirst();
    const QString url = m_baseUrl + "/" + QString::fromLatin1(QUrl::toPercentEncoding(Library::systemById(job.system)->thumbnailSystem))
                        + "/Named_Boxarts/" + QString::fromLatin1(QUrl::toPercentEncoding(name)) + ".png";
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "nook/0.1");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, job]() mutable {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->error() == QNetworkReply::NoError && reply->bytesAvailable() <= MaxImageBytes
                                    ? reply->readAll() : QByteArray();

        // The server's answer decides this file; a picture must really decode
        // before it's trusted, since it's about to be drawn on screen.
        if (!body.isEmpty() && !QImage::fromData(body).isNull()) {
            const QString target = cachePath(job.system, job.baseName);
            QDir().mkpath(QFileInfo(target).absolutePath());
            QSaveFile file(target);
            if (file.open(QIODevice::WriteOnly) && file.write(body) == body.size() && file.commit())
                emit ready(job.key, target);
            --m_active;
            pump();
            return;
        }

        if (status == 404 && !job.candidates.isEmpty()) {
            fetch(job);
            return;
        }

        // Only a clear "not there" is remembered. A dropped connection isn't
        // the game's fault, so that one gets tried again next time.
        if (status == 404) {
            const QString miss = missPath(job.system, job.baseName);
            QDir().mkpath(QFileInfo(miss).absolutePath());
            QFile marker(miss);
            if (marker.open(QIODevice::WriteOnly))
                marker.close();
        }
        --m_active;
        pump();
    });
}
