#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <memory>

class Artwork;

struct SystemInfo {
    QString id;
    QString label;
    QStringList extensions;
    // The folder name libretro's thumbnail server files this system's art under.
    QString thumbnailSystem;
};

struct Game {
    QString path;
    QString baseName;
    QString title;
    QString system;
    QString art;
    qint64 lastPlayed = 0;
    int playCount = 0;
    // Every disc in order, for a game on more than one. Empty otherwise.
    QStringList discs;
    // How many discs it came on, however they're stored.
    int discCount = 1;
};

// Every game in ~/Games/roms, read-only. Picks up new files on its own, so
// dropping a game in the folder is all it takes to add it to the drawer.
class Library : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCountProperty NOTIFY listChanged)
    Q_PROPERTY(int total READ total NOTIFY listChanged)
    Q_PROPERTY(QString shelf READ shelf WRITE setShelf NOTIFY shelfChanged)
    Q_PROPERTY(QVariantList shelves READ shelves NOTIFY listChanged)
    Q_PROPERTY(QString romsPath READ romsPath CONSTANT)

public:
    enum Role { TitleRole = Qt::UserRole + 1, SystemRole, SystemLabelRole, PathRole, ArtRole, LastPlayedRole, HueRole, DiscsRole };

    explicit Library(const QString &gamesRoot, const QString &statePath, Artwork *artwork = nullptr,
                     QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountProperty() const { return m_visible.size(); }
    int total() const { return m_all.size(); }
    QString shelf() const { return m_shelf; }
    void setShelf(const QString &shelf);
    QVariantList shelves() const;
    QString romsPath() const { return m_gamesRoot + "/roms"; }
    const QList<Game> &all() const { return m_all; }

    Q_INVOKABLE void rescan();
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE QVariantMap gameAt(int row) const;
    // With a disc, also remembers that as the one this game was left on.
    Q_INVOKABLE void markPlayed(const QString &path, int disc = -1);
    // What to hand the emulator to boot a disc (-1: the one it was left on).
    // RetroArch takes a playlist in a fixed order and is told the disc separately,
    // Dolphin boots whatever a playlist lists first, and ARMSX2 only takes one
    // disc. Playlists Nook writes live in its state folder, not with the games.
    Q_INVOKABLE QString launchPathFor(const QString &path, int disc = -1) const;
    Q_INVOKABLE void stepShelf(int delta);

    static const QList<SystemInfo> &systems();
    static const SystemInfo *systemById(const QString &id);
    static QString cleanTitle(const QString &baseName);
    // Drops what a fan patch adds to a file's name: "Game English v2.00" is "Game".
    static QString withoutPatchName(QString name);
    static QStringList thumbnailCandidates(const QString &baseName);
    // The playable files in a folder: a .cue hides the tracks it lists, and an
    // .m3u hides the discs it lists, so one game shows up once.
    static QStringList scanFolder(const QString &directory, const QStringList &extensions);
    // Sorts files into games: "Saga (Disc 1)" and "Saga (Disc 2)" in one folder
    // become one list, in disc order. Anything else is a list of one.
    static QList<QStringList> groupDiscs(const QStringList &files);

signals:
    void listChanged();
    void shelfChanged();

private:
    void rebuildVisible();
    void armWatcher();
    void applyArt(const QString &path, const QString &art);
    int hueFor(const QString &title) const;
    int lastDisc(const Game &game) const;

    QString m_gamesRoot;
    QString m_stateDirectory;
    QString m_shelf;
    QList<Game> m_all;
    QList<int> m_visible;
    Artwork *m_artwork;
    std::unique_ptr<QSettings> m_state;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
