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
#include <QRegularExpression>
#include <QSaveFile>
#include <QHash>
#include <QUrl>
#include <utility>

namespace {
constexpr int MaxConcurrent = 4;
constexpr qint64 MissRetrySeconds = 7 * 24 * 60 * 60;
constexpr qint64 MaxImageBytes = 16 * 1024 * 1024;

// Reduces a title to what's left once naming style is gone, so that
// "final_fantasy_7" and "Final Fantasy VII" compare equal, and so do
// "The Legend of Zelda" and "Legend of Zelda, The".
QString comparable(QString title) {
    static const QRegularExpression leading(R"(^(the|a|an)\s+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression trailing(R"(,\s*(the|a|an)\b)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression notWord(R"([^\p{L}\p{N}]+)", QRegularExpression::UseUnicodePropertiesOption);
    title.remove(trailing);
    title.remove(leading);
    title.replace("&", " and ");
    title.remove('\'');
    title.replace(notWord, " ");
    QStringList words = title.simplified().toLower().split(' ', Qt::SkipEmptyParts);
    // Lone I and X stay words: Mega Man X isn't Mega Man 10.
    static const QHash<QString, QString> numerals{{"ii", "2"},  {"iii", "3"}, {"iv", "4"},  {"v", "5"},   {"vi", "6"},  {"vii", "7"},
                                                  {"viii", "8"}, {"ix", "9"},  {"xi", "11"}, {"xii", "12"}, {"xiii", "13"}};
    for (auto &word : words)
        word = numerals.value(word, word);
    return words.join(' ');
}

// The title with tags gone and any subtitle kept apart: a server name uses
// " - " or an underscore where the title had a colon.
struct Titled {
    QString whole;
    QString main;
};
Titled titled(QString name) {
    static const QRegularExpression tags(R"(\s*[\(\[].*$)");
    static const QRegularExpression subtitle(R"(\s+-\s+|_\s+|:\s+)");
    name.remove(tags);
    return {comparable(name), comparable(name.section(subtitle, 0, 0))};
}

QString siteUrl(const QString &base, const QString &system) {
    return base + "/" + QString::fromLatin1(QUrl::toPercentEncoding(Library::systemById(system)->thumbnailSystem)) + "/Named_Boxarts/";
}
}

QStringList Artwork::parseCatalog(const QByteArray &indexPage) {
    static const QRegularExpression link(R"re(href="([^"/?]+)\.png")re");
    QStringList names;
    auto matches = link.globalMatch(QString::fromUtf8(indexPage));
    while (matches.hasNext())
        names << QUrl::fromPercentEncoding(matches.next().captured(1).toUtf8());
    return names;
}

QString Artwork::closestBoxart(const QString &baseName, const QStringList &catalog) {
    static const QRegularExpression unofficial(R"(\((Beta|Proto|Demo|Sample|Aftermarket|Unl|Pirate|Kiosk)[^)]*\)|\[(Hack|h\d))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression laterDisc(R"(\(Dis[ck]\s*([2-9])[^)]*\))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression translationTag(R"(\(T\)|\[T[-+])", QRegularExpression::CaseInsensitiveOption);

    const QString title = Library::withoutPatchName(baseName);
    const bool wantsTranslation = title != baseName || baseName.contains(translationTag);
    const Titled wanted = titled(title);
    if (wanted.whole.isEmpty())
        return {};

    QString best;
    int bestScore = 0;
    for (const auto &name : catalog) {
        // The whole title first. Then a subtitle on either side: "Dragon Quest V"
        // can be "Dragon Quest V - The Heavenly Bride". It takes words matching in
        // full, so it can never be "Dragon Quest VI" or "Super Mario RPG Deluxe".
        const Titled candidate = titled(name);
        int score = candidate.whole == wanted.whole ? 1000
                    : candidate.main == wanted.whole ? 500
                    : candidate.whole == wanted.main ? 300
                                                     : 0;
        if (score == 0)
            continue;
        for (const auto &[region, points] : {std::pair{"(USA", 40}, {"(World", 30}, {"(Europe", 20}, {"(Japan", 10}})
            if (name.contains(QLatin1String(region))) {
                score += points;
                break;
            }
        if (name.contains(unofficial))
            score -= 300;
        if (name.contains(laterDisc))
            score -= 50;
        if (name.contains("[T-"))
            score += wantsTranslation ? 60 : -60;
        if (score > bestScore || (score == bestScore && name.size() < best.size())) {
            bestScore = score;
            best = name;
        }
    }
    return best;
}

Artwork::Artwork(const QString &gamesRoot, const QString &cacheDirectory, const QString &baseUrl, QObject *parent)
    : QObject(parent), m_gamesRoot(gamesRoot), m_cache(cacheDirectory), m_baseUrl(baseUrl) {}

QString Artwork::cachePath(const QString &system, const QString &baseName) const {
    const QByteArray hash = QCryptographicHash::hash(baseName.toUtf8(), QCryptographicHash::Sha1).toHex();
    return m_cache + "/" + system + "/" + QString::fromLatin1(hash) + ".png";
}

QString Artwork::missPath(const QString &system, const QString &baseName) const {
    return cachePath(system, baseName) + ".none";
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
    const QString url = siteUrl(m_baseUrl, job.system) + QString::fromLatin1(QUrl::toPercentEncoding(name)) + ".png";
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

        // Every guess missed, so ask the server what it does have, once.
        if (status == 404 && !job.searched) {
            job.searched = true;
            search(job);
            --m_active;
            pump();
            return;
        }

        finish(job, status == 404);
    });
}

void Artwork::search(Job job) {
    if (m_catalogs.contains(job.system)) {
        const QString match = closestBoxart(job.baseName, m_catalogs.value(job.system));
        if (match.isEmpty()) {
            ++m_active;
            finish(job, true);
            return;
        }
        job.candidates = {match};
        m_queue.prepend(job);
        return;
    }
    const bool alreadyAsked = m_waiting.contains(job.system);
    m_waiting[job.system] << job;
    if (!alreadyAsked)
        fetchCatalog(job.system);
}

void Artwork::fetchCatalog(const QString &system) {
    QNetworkRequest request{QUrl(siteUrl(m_baseUrl, system))};
    request.setHeader(QNetworkRequest::UserAgentHeader, "nook/0.1");
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, system] {
        reply->deleteLater();
        const QList<Job> waiting = m_waiting.take(system);
        // No list means no answer, not "no art", so these get tried again next time.
        if (reply->error() != QNetworkReply::NoError)
            return;
        m_catalogs.insert(system, parseCatalog(reply->readAll()));
        for (const auto &job : waiting)
            search(job);
        pump();
    });
}

// Only a clear "not there" is remembered. A dropped connection isn't the game's
// fault, so that one gets tried again next time.
void Artwork::finish(const Job &job, bool remember) {
    if (remember) {
        const QString miss = missPath(job.system, job.baseName);
        QDir().mkpath(QFileInfo(miss).absolutePath());
        QFile marker(miss);
        if (marker.open(QIODevice::WriteOnly))
            marker.close();
    }
    --m_active;
    pump();
}
