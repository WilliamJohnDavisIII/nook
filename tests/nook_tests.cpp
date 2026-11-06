#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "artwork.h"
#include "gamepad.h"
#include "launcher.h"
#include "library.h"
#include "theme.h"
#include "translator.h"

class NookTests : public QObject {
    Q_OBJECT

private slots:
    void titlesReadLikeTitles()
    {
        QCOMPARE(Library::cleanTitle("Secret Garden, The (USA) [!]"), QString("The Secret Garden"));
        QCOMPARE(Library::cleanTitle("Moon_River_Racer"), QString("Moon River Racer"));
        QCOMPARE(Library::cleanTitle("Harbor Lights (USA) (Disc 2)"), QString::fromUtf8("Harbor Lights · Disc 2"));
        QCOMPARE(Library::cleanTitle("Plain Name"), QString("Plain Name"));
        QCOMPARE(Library::cleanTitle("Live A Live English v2.00"), QString("Live A Live"));
        QCOMPARE(Library::cleanTitle("Learning English"), QString("Learning English"));
        QCOMPARE(Library::cleanTitle("Mother 3 English Patch v1.3"), QString("Mother 3"));
    }

    void thumbnailNamesFollowTheServersRules()
    {
        const auto names = Library::thumbnailCandidates("Star: Fox & Friends (USA) (Rev 1) [!]");
        QCOMPARE(names.first(), QString("Star_ Fox _ Friends (USA) (Rev 1) [!]"));
        QVERIFY(names.contains("Star_ Fox _ Friends (USA) (Rev 1)"));
        QVERIFY(names.contains("Star_ Fox _ Friends (USA)"));
        QVERIFY(Library::thumbnailCandidates("Hand Named").contains("Hand Named (USA)"));
    }

    void oneGameShowsUpOnce()
    {
        QTemporaryDir dir;
        write(dir.filePath("Game (Track 1).bin"), "x");
        write(dir.filePath("Game (Track 2).bin"), "x");
        write(dir.filePath("Game.cue"), "FILE \"Game (Track 1).bin\" BINARY\nFILE \"Game (Track 2).bin\" BINARY\n");
        write(dir.filePath("Solo.iso"), "x");
        write(dir.filePath("notes.txt"), "x");
        const auto found = Library::scanFolder(dir.path(), {"cue", "bin", "iso"});
        QCOMPARE(found.size(), 2);
        QVERIFY(found.contains(dir.filePath("Game.cue")));
        QVERIFY(found.contains(dir.filePath("Solo.iso")));
    }

    void playlistsHideTheirDiscs()
    {
        QTemporaryDir dir;
        write(dir.filePath("Saga (Disc 1).chd"), "x");
        write(dir.filePath("Saga (Disc 2).chd"), "x");
        write(dir.filePath("Saga.m3u"), "Saga (Disc 1).chd\nSaga (Disc 2).chd\n");
        const auto found = Library::scanFolder(dir.path(), {"m3u", "chd"});
        QCOMPARE(found, QStringList{dir.filePath("Saga.m3u")});
    }

    void libraryScansFiltersAndRemembers()
    {
        QTemporaryDir games, state;
        QDir(games.path()).mkpath("roms/snes");
        QDir(games.path()).mkpath("roms/ps2");
        const QString apple = games.filePath("roms/snes/Apple.sfc");
        write(apple, "x");
        write(games.filePath("roms/snes/Zebra.sfc"), "x");
        write(games.filePath("roms/ps2/Mango.iso"), "x");

        const QString statePath = state.filePath("state.ini");
        {
            Library library(games.path(), statePath);
            QCOMPARE(library.total(), 3);
            library.setShelf("ps2");
            QCOMPARE(library.rowCount(), 1);
            QCOMPARE(library.gameAt(0)["title"].toString(), QString("Mango"));
            library.setShelf("");
            QCOMPARE(library.gameAt(0)["title"].toString(), QString("Apple"));
            library.markPlayed(games.filePath("roms/snes/Zebra.sfc"));
        }
        Library reopened(games.path(), statePath);
        QCOMPARE(reopened.gameAt(0)["title"].toString(), QString("Zebra"));
        QVERIFY(reopened.gameAt(0)["lastPlayed"].toLongLong() > 0);
    }

    void shelvesWrapAround()
    {
        QTemporaryDir games, state;
        Library library(games.path(), state.filePath("s.ini"));
        library.stepShelf(-1);
        QCOMPARE(library.shelf(), QString("ps2"));
        library.stepShelf(1);
        QCOMPARE(library.shelf(), QString());
    }

    void launchCommandsPickTheRightEmulator()
    {
        QTemporaryDir root;
        QDir(root.path()).mkpath("cores");
        QDir(root.path()).mkpath("bios/ps2");
        QDir(root.path()).mkpath("bios/dolphin-emu/Sys");
        for (const char *core : {"snes9x", "mednafen_psx_hw", "dolphin"})
            write(root.filePath(QStringLiteral("cores/%1_libretro.so").arg(QLatin1String(core))), "x");
        write(root.filePath("bios/scph5501.bin"), "x");
        write(root.filePath("bios/ps2/bios.bin"), QByteArray(4 * 1024 * 1024, 'x'));
        const QString game = root.filePath("-odd game.iso");
        write(game, "x");

        LaunchPaths paths;
        paths.gamesRoot = root.path();
        paths.coreDirectory = root.filePath("cores");
        paths.retroarch = "/usr/bin/retroarch";
        paths.armsx2 = "/opt/armsx2.AppImage";

        auto snes = Launcher::plan("snes", game, paths);
        QCOMPARE(snes.problem, QString());
        QCOMPARE(snes.program, QString("/usr/bin/retroarch"));
        QCOMPARE(snes.arguments, (QStringList{"-f", "-L", root.filePath("cores/snes9x_libretro.so"), game}));

        auto ps2 = Launcher::plan("ps2", game, paths);
        QCOMPARE(ps2.program, QString("/opt/armsx2.AppImage"));
        // The file name comes after "--", so a dash at its start can't read as an option.
        QCOMPARE(ps2.arguments, (QStringList{"-nogui", "-fullscreen", "--", game}));

        QVERIFY(Launcher::plan("ps1", game, paths).problem.isEmpty());
        QVERIFY(Launcher::plan("gc", game, paths).problem.isEmpty());
    }

    void standaloneDolphinWinsForGameCube()
    {
        QTemporaryDir root;
        const QString game = root.filePath("game.rvz");
        write(game, "x");
        LaunchPaths paths;
        paths.gamesRoot = root.path();
        paths.dolphin = "/usr/bin/dolphin-emu";
        const auto plan = Launcher::plan("gc", game, paths);
        QCOMPARE(plan.problem, QString());
        QCOMPARE(plan.program, QString("/usr/bin/dolphin-emu"));
        QVERIFY(plan.arguments.contains("--exec=" + game));
        QVERIFY(plan.arguments.contains("--batch"));
    }

    void missingPiecesAreExplained()
    {
        QTemporaryDir root;
        const QString game = root.filePath("game.iso");
        write(game, "x");
        LaunchPaths paths;
        paths.gamesRoot = root.path();
        paths.coreDirectory = root.path();
        paths.retroarch = "/usr/bin/retroarch";

        QVERIFY(Launcher::plan("ps2", game, paths).problem.contains("ARMSX2"));
        paths.armsx2 = "/opt/armsx2.AppImage";
        QVERIFY(Launcher::plan("ps2", game, paths).problem.contains("BIOS"));
        QVERIFY(Launcher::plan("snes", game, paths).problem.contains("emulator"));
        QVERIFY(Launcher::plan("snes", root.filePath("gone.sfc"), paths).problem.contains("moved"));
    }

    void onlyAQuickExitCountsAsAFailedStart()
    {
        QVERIFY(Launcher::isFailedStart(800, 1, QProcess::NormalExit, false));
        QVERIFY(!Launcher::isFailedStart(800, 0, QProcess::NormalExit, false));
        // ARMSX2 can return 1 after a perfectly good session.
        QVERIFY(!Launcher::isFailedStart(600000, 1, QProcess::NormalExit, false));
        QVERIFY(Launcher::isFailedStart(600000, 0, QProcess::CrashExit, false));
        // We asked it to stop, so however it ended isn't a failure.
        QVERIFY(!Launcher::isFailedStart(800, 1, QProcess::CrashExit, true));
    }

    void aRealProcessStartsAndEnds()
    {
        QTemporaryDir root;
        const QString emulator = root.filePath("emulator");
        write(emulator, "#!/bin/sh\nsleep 30\n");
        QVERIFY(QFile::setPermissions(emulator, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QDir(root.path()).mkpath("bios/ps2");
        write(root.filePath("bios/ps2/bios.bin"), QByteArray(4 * 1024 * 1024, 'x'));
        const QString game = root.filePath("game.iso");
        write(game, "x");

        LaunchPaths paths;
        paths.gamesRoot = root.path();
        paths.armsx2 = emulator;
        Launcher launcher(paths);
        QVERIFY(launcher.launch("ps2", game));
        QTRY_VERIFY(launcher.running());
        QTest::qWait(150);
        // Stopping reaches the whole process group, including the child "sleep".
        launcher.stop();
        QTRY_VERIFY(!launcher.running());
        QCOMPARE(launcher.lastError(), QString());
    }

    void aFailedLaunchExplainsItself()
    {
        QTemporaryDir root;
        const QString emulator = root.filePath("emulator");
        write(emulator, "#!/bin/sh\necho bad memory card >&2\nexit 3\n");
        QVERIFY(QFile::setPermissions(emulator, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QDir(root.path()).mkpath("bios/ps2");
        write(root.filePath("bios/ps2/bios.bin"), QByteArray(4 * 1024 * 1024, 'x'));
        const QString game = root.filePath("game.iso");
        write(game, "x");

        LaunchPaths paths;
        paths.gamesRoot = root.path();
        paths.armsx2 = emulator;
        Launcher launcher(paths);
        QVERIFY(launcher.launch("ps2", game));
        QTRY_VERIFY(!launcher.running());
        QVERIFY(launcher.lastError().contains("bad memory card"));
    }

    void directionsRepeatAfterADelay()
    {
        InputInterpreter input;
        InputFrame held;
        held.right = true;
        QCOMPARE(input.update(held, 0, false), QStringList{"right"});
        QVERIFY(input.update(held, 200, false).isEmpty());
        QCOMPARE(input.update(held, 400, false), QStringList{"right"});
        QVERIFY(input.update(held, 450, false).isEmpty());
        QCOMPARE(input.update(held, 560, false), QStringList{"right"});
        QVERIFY(input.update(InputFrame{}, 600, false).isEmpty());
    }

    void buttonsFireOncePerPress()
    {
        InputInterpreter input;
        InputFrame south;
        south.south = true;
        QCOMPARE(input.update(south, 0, false), QStringList{"play"});
        QVERIFY(input.update(south, 100, false).isEmpty());
        QVERIFY(input.update(InputFrame{}, 200, false).isEmpty());
        QCOMPARE(input.update(south, 300, false), QStringList{"play"});
    }

    void leavingAGameTakesAHeldCombo()
    {
        InputInterpreter input;
        InputFrame combo;
        combo.select = true;
        combo.start = true;
        QVERIFY(input.update(combo, 0, true).isEmpty());
        // A tap is left to the game, which may use it for itself.
        QVERIFY(input.update(InputFrame{}, 300, true).isEmpty());
        QVERIFY(input.update(combo, 400, true).isEmpty());
        QVERIFY(input.update(combo, 1200, true).isEmpty());
        QCOMPARE(input.update(combo, 1400, true), QStringList{"exitGame"});
        QVERIFY(input.update(combo, 1500, true).isEmpty());
    }

    void gameButtonsNeverReachTheDrawer()
    {
        InputInterpreter input;
        InputFrame play;
        play.south = true;
        play.right = true;
        QVERIFY(input.update(play, 0, true).isEmpty());
        // Still held as the game ends: this must not start another game.
        QVERIFY(input.update(play, 100, false).isEmpty());
        QVERIFY(input.update(play, 200, false).isEmpty());
    }

    void eightBitDoPadsConfirmOnTheRight()
    {
        QVERIFY(Gamepad::usesNintendoLayout("8BitDo Pro 2"));
        QVERIFY(Gamepad::usesNintendoLayout("8bitdo SN30 Pro"));
        QVERIFY(!Gamepad::usesNintendoLayout("Xbox One Wireless Controller"));
        QVERIFY(!Gamepad::usesNintendoLayout(""));
    }

    void coverSearchFindsWhatTheFileMeans()
    {
        const QStringList ps1{"Final Fantasy VII (Europe) (Disc 1)", "Final Fantasy VII (Japan) (Disc 1)",
                              "Final Fantasy VII (USA) (Disc 2)", "Final Fantasy VII (USA) (Disc 1)",
                              "Final Fantasy VIII (USA) (Disc 1)", "Final Fantasy VII (USA) (Demo)"};
        QCOMPARE(Artwork::closestBoxart("Final Fantasy VII (T)", ps1), QString("Final Fantasy VII (USA) (Disc 1)"));

        const QStringList snes{"Dragon Quest VI - Maboroshi no Daichi (Japan)", "Dragon Quest V - Tenkuu no Hanayome (Japan)",
                               "Live A Live (Japan)", "Live A Live (Japan) [T-En by Aeon Genesis v2.00 Deluxe]"};
        QCOMPARE(Artwork::closestBoxart("Dragon Quest V", snes), QString("Dragon Quest V - Tenkuu no Hanayome (Japan)"));
        QCOMPARE(Artwork::closestBoxart("Live A Live English v2.00", snes), QString("Live A Live (Japan) [T-En by Aeon Genesis v2.00 Deluxe]"));
        QCOMPARE(Artwork::closestBoxart("Live A Live", snes), QString("Live A Live (Japan)"));
    }

    void coverSearchIgnoresNamingStyle()
    {
        const QStringList catalog{"Final Fantasy VII (USA) (Disc 1)", "Legend of Zelda, The - A Link to the Past (USA)",
                                  "Ratchet & Clank (USA)", "Luigi's Mansion (USA)", "Mega Man X (USA)", "Mega Man 10 (USA)",
                                  "Metal Gear Solid 2 - Sons of Liberty (USA)", "Suikoden (USA)"};
        QCOMPARE(Artwork::closestBoxart("final_fantasy_7", catalog), QString("Final Fantasy VII (USA) (Disc 1)"));
        QCOMPARE(Artwork::closestBoxart("Final.Fantasy.VII", catalog), QString("Final Fantasy VII (USA) (Disc 1)"));
        QCOMPARE(Artwork::closestBoxart("The Legend of Zelda - A Link to the Past", catalog), QString("Legend of Zelda, The - A Link to the Past (USA)"));
        QCOMPARE(Artwork::closestBoxart("Ratchet and Clank", catalog), QString("Ratchet & Clank (USA)"));
        QCOMPARE(Artwork::closestBoxart("Luigis Mansion", catalog), QString("Luigi's Mansion (USA)"));
        QCOMPARE(Artwork::closestBoxart("Mega Man X", catalog), QString("Mega Man X (USA)"));
        QCOMPARE(Artwork::closestBoxart("Metal Gear Solid II", catalog), QString("Metal Gear Solid 2 - Sons of Liberty (USA)"));
        QCOMPARE(Artwork::closestBoxart("Suikoden - Special Edition", catalog), QString("Suikoden (USA)"));
    }

    void coverSearchWouldRatherFindNothingThanTheWrongGame()
    {
        const QStringList catalog{"Dragon Quest VI (Japan)", "Final Fantasy VIII (USA)", "Super Mario RPG Deluxe (USA)"};
        QCOMPARE(Artwork::closestBoxart("Dragon Quest V", catalog), QString());
        QCOMPARE(Artwork::closestBoxart("Final Fantasy VII (USA)", catalog), QString());
        QCOMPARE(Artwork::closestBoxart("Super Mario RPG", catalog), QString());
        QCOMPARE(Artwork::closestBoxart("", catalog), QString());
    }

    void theServersListIsRead()
    {
        const QByteArray page = "<a href=\"?C=N;O=D\">Name</a><a href=\"/parent/\">Up</a>"
                                "<a href=\"Final%20Fantasy%20VII%20(USA)%20(Disc%201).png\">x</a>"
                                "<a href=\"'99%20Koushien%20(Japan).png\">y</a>";
        QCOMPARE(Artwork::parseCatalog(page), (QStringList{"Final Fantasy VII (USA) (Disc 1)", "'99 Koushien (Japan)"}));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(theme.dark());
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#ffffff\"\ndark_background = \"#f5f5f5\"\n"
              "foreground = \"#000000\"\ndark_foreground = \"#c0c0c0\"\nmuted = \"#808080\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.background(), QString("#ffffff"));
        QCOMPARE(theme.surface(), QString("#f5f5f5"));
        QCOMPARE(theme.foreground(), QString("#000000"));
        QCOMPARE(theme.muted(), QString("#808080"));
        QVERIFY(!theme.dark());
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Play").isNull());
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(NookTests)
#include "nook_tests.moc"
