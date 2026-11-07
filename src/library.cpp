#include "library.h"

#include "artwork.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <algorithm>

namespace {
QString stateKey(const QString &path) {
    return "played/" + QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

QString discKey(const QString &path) {
    return "disc/" + QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

// Files a playlist or cue sheet refers to, resolved beside the file itself.
QStringList referencedFiles(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || file.size() > 256 * 1024)
        return {};
    const QDir directory = QFileInfo(path).absoluteDir();
    const bool cue = path.endsWith(".cue", Qt::CaseInsensitive);
    static const QRegularExpression cueFile(R"re(^\s*FILE\s+"?(.+?)"?\s+\w+\s*$)re",
                                            QRegularExpression::CaseInsensitiveOption);
    QStringList found;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        if (cue) {
            const auto match = cueFile.match(line);
            if (match.hasMatch())
                found << QFileInfo(directory.absoluteFilePath(match.captured(1))).absoluteFilePath();
        } else {
            found << QFileInfo(directory.absoluteFilePath(line)).absoluteFilePath();
        }
    }
    return found;
}

const QRegularExpression &discTag() {
    static const QRegularExpression tag(R"(\s*\((?:Disc|Disk)\s*([0-9]+)[^\)]*\))", QRegularExpression::CaseInsensitiveOption);
    return tag;
}
}

const QList<SystemInfo> &Library::systems() {
    static const QList<SystemInfo> list{
        {"snes", "SNES", {"sfc", "smc", "swc", "fig", "bs", "st", "zip", "7z"},
         "Nintendo - Super Nintendo Entertainment System"},
        {"ps1", "PS1", {"cue", "toc", "m3u", "ccd", "pbp", "chd"}, "Sony - PlayStation"},
        {"gc", "GameCube", {"rvz", "gcz", "gcm", "iso", "ciso", "wbfs", "wia", "tgc", "m3u"}, "Nintendo - GameCube"},
        {"ps2", "PS2", {"iso", "chd", "cso", "zso", "cue", "bin", "mdf", "nrg", "gz"}, "Sony - PlayStation 2"},
    };
    return list;
}

const SystemInfo *Library::systemById(const QString &id) {
    for (const auto &system : systems())
        if (system.id == id)
            return &system;
    return nullptr;
}

QString Library::withoutPatchName(QString name) {
    // "English" alone can be part of a real title, so it counts only with a
    // version or the word "patch" beside it.
    static const QRegularExpression patch(R"(\s+((english|eng)(\s+patch(\s+v?\d[\d.]*)?|\s+v?\d[\d.]*)|translated|translation|patched)\s*$)",
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression version(R"(\s+v\d+(\.\d+)*\s*$)", QRegularExpression::CaseInsensitiveOption);
    name.remove(patch);
    name.remove(version);
    return name;
}

QString Library::cleanTitle(const QString &baseName) {
    static const QRegularExpression tags(R"(\s*[\(\[][^\)\]]*[\)\]])");
    static const QRegularExpression disc(R"(\((Disc|Disk)\s*([0-9]+)[^\)]*\))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression article(R"(^(.*),\s*(The|A|An)(\s*-\s*.*|\s*:\s*.*)?$)");

    QString title = baseName;
    if (!title.contains(' '))
        title.replace('_', ' ');
    const auto discMatch = disc.match(title);
    title.remove(tags);
    title = withoutPatchName(title.simplified());

    // Catalog names file "The" at the end to sort; people read it at the front.
    const auto articleMatch = article.match(title);
    if (articleMatch.hasMatch())
        title = articleMatch.captured(2) + " " + articleMatch.captured(1) + articleMatch.captured(3);

    if (title.isEmpty())
        title = baseName;
    if (discMatch.hasMatch())
        title += QString::fromUtf8(" · Disc ") + discMatch.captured(2);
    return title;
}

QStringList Library::thumbnailCandidates(const QString &baseName) {
    // The server's own rule for characters it can't keep in a file name.
    auto sanitize = [](QString name) {
        static const QRegularExpression unsafe(R"([&*/:`<>?\\|"])");
        return name.replace(unsafe, "_").simplified();
    };
    static const QRegularExpression brackets(R"(\s*\[[^\]]*\])");
    static const QRegularExpression parens(R"(\s*\([^\)]*\))");
    static const QRegularExpression firstParen(R"(^([^\(]*\([^\)]*\)))");
    static const QRegularExpression disc(R"(\s*\((Disc|Disk)\s*[0-9]+[^\)]*\))", QRegularExpression::CaseInsensitiveOption);

    QStringList names{baseName};
    QString withoutBrackets = baseName;
    withoutBrackets.remove(brackets);
    names << withoutBrackets;
    QString withoutDisc = withoutBrackets;
    withoutDisc.remove(disc);
    names << withoutDisc;
    const auto match = firstParen.match(withoutDisc);
    if (match.hasMatch())
        names << match.captured(1);

    // Hand-named files carry no region, so try the likely catalog spellings.
    QString bare = withoutDisc;
    bare.remove(parens);
    if (bare == withoutDisc) {
        for (const char *region : {"USA", "World", "Europe", "Japan"})
            names << QStringLiteral("%1 (%2)").arg(bare, QLatin1String(region));
    }

    QStringList result;
    for (const auto &name : names)
        result << sanitize(name);
    result.removeDuplicates();
    return result;
}

QStringList Library::scanFolder(const QString &directory, const QStringList &extensions) {
    QStringList files;
    QDirIterator it(directory, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (info.fileName().startsWith('.'))
            continue;
        if (extensions.contains(info.suffix().toLower()))
            files << info.absoluteFilePath();
    }

    QSet<QString> hidden;
    for (const auto &path : files) {
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == "cue" || suffix == "m3u")
            for (const auto &referenced : referencedFiles(path))
                hidden.insert(referenced);
    }
    files.erase(std::remove_if(files.begin(), files.end(), [&](const QString &path) { return hidden.contains(path); }),
                files.end());
    return files;
}

QList<QStringList> Library::groupDiscs(const QStringList &files) {
    QList<QStringList> games;
    QMap<QString, QMap<int, QString>> sets;
    QSet<QString> unclear;
    for (const auto &path : files) {
        const QFileInfo info(path);
        const auto match = discTag().match(info.completeBaseName());
        if (!match.hasMatch()) {
            games << QStringList{path};
            continue;
        }
        QString name = info.completeBaseName();
        name.remove(discTag());
        const QString key = info.absolutePath() + "/" + name.toCaseFolded();
        // Two copies of one disc, say an .iso and an .rvz, aren't a set.
        if (sets[key].contains(match.captured(1).toInt()))
            unclear.insert(key);
        sets[key].insert(match.captured(1).toInt(), path);
    }
    for (const auto &path : files) {
        const QFileInfo info(path);
        if (!discTag().match(info.completeBaseName()).hasMatch())
            continue;
        QString name = info.completeBaseName();
        name.remove(discTag());
        const QString key = info.absolutePath() + "/" + name.toCaseFolded();
        if (unclear.contains(key) || sets.value(key).size() < 2)
            games << QStringList{path};
    }
    for (auto set = sets.cbegin(); set != sets.cend(); ++set)
        if (!unclear.contains(set.key()) && set->size() >= 2)
            games << set->values();
    return games;
}

Library::Library(const QString &gamesRoot, const QString &statePath, Artwork *artwork, QObject *parent)
    : QAbstractListModel(parent), m_gamesRoot(gamesRoot), m_stateDirectory(QFileInfo(statePath).absolutePath()),
      m_artwork(artwork),
      m_state(std::make_unique<QSettings>(statePath, QSettings::IniFormat)) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(800);
    connect(&m_debounce, &QTimer::timeout, this, &Library::rescan);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    if (m_artwork)
        connect(m_artwork, &Artwork::ready, this, &Library::applyArt);
    rescan();
}

void Library::armWatcher() {
    const auto paths = m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    // Watch up to the games folder too, so a folder made later is noticed.
    QStringList watch{m_gamesRoot, romsPath()};
    for (const auto &system : systems()) {
        watch << romsPath() + "/" + system.id;
        QDirIterator it(romsPath() + "/" + system.id, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
            watch << it.next();
    }
    for (const auto &path : watch)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
}

void Library::rescan() {
    armWatcher();

    QList<Game> games;
    for (const auto &system : systems()) {
        for (const auto &set : groupDiscs(scanFolder(romsPath() + "/" + system.id, system.extensions))) {
            // A set goes by its first disc, which keeps that disc's art and history.
            const QString &path = set.first();
            Game game;
            game.path = path;
            game.baseName = QFileInfo(path).completeBaseName();
            game.title = cleanTitle(game.baseName);
            game.system = system.id;
            if (set.size() > 1) {
                QString name = game.baseName;
                name.remove(discTag());
                game.title = cleanTitle(name);
                game.discs = set;
                game.discCount = set.size();
            } else if (path.endsWith(".m3u", Qt::CaseInsensitive)) {
                const QStringList listed = referencedFiles(path);
                if (listed.size() > 1) {
                    game.discs = listed;
                    game.discCount = listed.size();
                }
            }
            // Whichever disc was played last, from before they were grouped too.
            for (const auto &disc : set) {
                const QStringList played = m_state->value(stateKey(disc)).toString().split(',');
                if (played.size() == 2 && played[0].toLongLong() > game.lastPlayed) {
                    game.lastPlayed = played[0].toLongLong();
                    game.playCount = played[1].toInt();
                }
            }
            if (m_artwork)
                game.art = m_artwork->cached(system.id, game.baseName);
            games << game;
        }
    }

    // Most recently played first, then alphabetical, so the games you're in
    // the middle of are always within reach.
    std::sort(games.begin(), games.end(), [](const Game &a, const Game &b) {
        if (a.lastPlayed != b.lastPlayed)
            return a.lastPlayed > b.lastPlayed;
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });

    beginResetModel();
    m_all = games;
    rebuildVisible();
    endResetModel();
    emit listChanged();

    if (m_artwork)
        for (const auto &game : std::as_const(m_all))
            if (game.art.isEmpty())
                m_artwork->request(game.path, game.system, game.baseName);
}

void Library::rebuildVisible() {
    m_visible.clear();
    for (int i = 0; i < m_all.size(); ++i)
        if (m_shelf.isEmpty() || m_all[i].system == m_shelf)
            m_visible << i;
}

void Library::setShelf(const QString &shelf) {
    if (shelf == m_shelf)
        return;
    beginResetModel();
    m_shelf = shelf;
    rebuildVisible();
    endResetModel();
    emit shelfChanged();
    emit listChanged();
}

void Library::stepShelf(int delta) {
    QStringList order{QString()};
    for (const auto &system : systems())
        order << system.id;
    const int next = (order.indexOf(m_shelf) + delta + order.size()) % order.size();
    setShelf(order[next]);
}

QVariantList Library::shelves() const {
    QVariantList result;
    result << QVariantMap{{"id", QString()}, {"label", tr("All")}, {"count", m_all.size()}};
    for (const auto &system : systems()) {
        const auto count = std::count_if(m_all.begin(), m_all.end(), [&](const Game &g) { return g.system == system.id; });
        result << QVariantMap{{"id", system.id}, {"label", system.label}, {"count", int(count)}};
    }
    return result;
}

int Library::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_visible.size();
}

int Library::hueFor(const QString &title) const {
    // Stable per title, so a game's placeholder cover never changes colour.
    return qHash(title, 7) % 360;
}

QVariant Library::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const Game &game = m_all[m_visible[index.row()]];
    switch (role) {
    case TitleRole: return game.title;
    case SystemRole: return game.system;
    case SystemLabelRole: {
        const auto *system = systemById(game.system);
        return system ? system->label : game.system;
    }
    case PathRole: return game.path;
    case ArtRole: return game.art;
    case LastPlayedRole: return game.lastPlayed;
    case HueRole: return hueFor(game.title);
    case DiscsRole: return game.discCount;
    default: return {};
    }
}

QHash<int, QByteArray> Library::roleNames() const {
    return {{TitleRole, "title"},   {SystemRole, "system"},       {SystemLabelRole, "systemLabel"}, {PathRole, "path"},
            {ArtRole, "art"},       {LastPlayedRole, "lastPlayed"}, {HueRole, "hue"},
            {DiscsRole, "discs"}};
}

int Library::indexOfPath(const QString &path) const {
    for (int row = 0; row < m_visible.size(); ++row)
        if (m_all[m_visible[row]].path == path)
            return row;
    return -1;
}

QVariantMap Library::gameAt(int row) const {
    if (row < 0 || row >= m_visible.size())
        return {};
    const QModelIndex at = index(row);
    return {{"title", data(at, TitleRole)},
            {"system", data(at, SystemRole)},
            {"systemLabel", data(at, SystemLabelRole)},
            {"path", data(at, PathRole)},
            {"art", data(at, ArtRole)},
            {"lastPlayed", data(at, LastPlayedRole)},
            {"hue", data(at, HueRole)},
            {"discs", data(at, DiscsRole)},
            {"discFiles", m_all[m_visible[row]].discs},
            {"lastDisc", lastDisc(m_all[m_visible[row]])}};
}

int Library::lastDisc(const Game &game) const {
    const int disc = m_state->value(discKey(game.path), 0).toInt();
    return disc >= 0 && disc < game.discs.size() ? disc : 0;
}

QString Library::launchPathFor(const QString &path, int disc) const {
    for (const auto &game : m_all) {
        if (game.path != path)
            continue;
        if (game.discs.isEmpty())
            return path;
        const int chosen = disc >= 0 && disc < game.discs.size() ? disc : lastDisc(game);
        if (game.system == "ps2")
            return game.discs[chosen];

        QStringList order = game.discs;
        if (game.system != "ps1")
            std::rotate(order.begin(), order.begin() + chosen, order.end());
        if (path.endsWith(".m3u", Qt::CaseInsensitive) && order == game.discs)
            return path;

        // Named after the game's own file, because RetroArch names saves after what
        // it loads, and those saves should still be there.
        const QString folder = m_stateDirectory + "/discs/" + QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
        QDir().mkpath(folder);
        QSaveFile playlist(folder + "/" + game.baseName + ".m3u");
        if (!playlist.open(QIODevice::WriteOnly) || playlist.write(order.join('\n').toUtf8() + '\n') < 0 || !playlist.commit())
            return path;
        return playlist.fileName();
    }
    return path;
}

void Library::markPlayed(const QString &path, int disc) {
    for (auto &game : m_all) {
        if (game.path != path)
            continue;
        game.lastPlayed = QDateTime::currentMSecsSinceEpoch();
        ++game.playCount;
        m_state->setValue(stateKey(path), QStringLiteral("%1,%2").arg(game.lastPlayed).arg(game.playCount));
        if (disc >= 0)
            m_state->setValue(discKey(path), disc);
        m_state->sync();
        const int row = indexOfPath(path);
        if (row >= 0)
            emit dataChanged(index(row), index(row), {LastPlayedRole});
        return;
    }
}

void Library::applyArt(const QString &path, const QString &art) {
    for (auto &game : m_all) {
        if (game.path != path)
            continue;
        game.art = art;
        const int row = indexOfPath(path);
        if (row >= 0)
            emit dataChanged(index(row), index(row), {ArtRole});
        return;
    }
}
