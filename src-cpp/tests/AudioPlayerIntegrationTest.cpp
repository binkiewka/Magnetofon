#include <QtTest>

#include "AudioPlayer.hpp"
#include "PlaylistModel.hpp"
#include "VisualizerLauncher.hpp"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include <cmath>

class AudioPlayerIntegrationTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QString createTone(const QString &name, double leftFrequency, double rightFrequency,
                       double seconds = 4.0)
    {
        const QString path = m_tempDir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};

        constexpr quint32 sampleRate = 48000;
        constexpr quint16 channels = 2;
        constexpr quint16 bitsPerSample = 16;
        const quint32 frames = static_cast<quint32>(seconds * sampleRate);
        const quint32 dataSize = frames * channels * (bitsPerSample / 8);

        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + dataSize);
        stream.writeRawData("WAVE", 4);
        stream.writeRawData("fmt ", 4);
        stream << quint32(16) << quint16(1) << channels << sampleRate;
        stream << quint32(sampleRate * channels * bitsPerSample / 8);
        stream << quint16(channels * bitsPerSample / 8) << bitsPerSample;
        stream.writeRawData("data", 4);
        stream << dataSize;

        for (quint32 i = 0; i < frames; ++i) {
            const double time = static_cast<double>(i) / sampleRate;
            const auto left = static_cast<qint16>(std::sin(time * leftFrequency * 2.0 * M_PI) * 16000);
            const auto right = static_cast<qint16>(std::sin(time * rightFrequency * 2.0 * M_PI) * 14500);
            stream << left << right;
        }
        return path;
    }

private slots:
    void initTestCase()
    {
        std::setlocale(LC_NUMERIC, "C");
    }

    void idleStateIsActuallyIdle()

    {
        AudioPlayer player;
        QTest::qWait(150);
        QVERIFY(!player.isPlaying());
        QVERIFY(!player.hasLoadedMedia());
        QCOMPARE(player.position(), 0.0);
        QCOMPARE(player.leftMeter(), 0.0);
        QCOMPARE(player.rightMeter(), 0.0);
        for (const QVariant &band : player.spectrum()) QCOMPARE(band.toDouble(), 0.0);

        player.play();
        QTest::qWait(100);
        QVERIFY(!player.isPlaying());
    }

    void droppedFilesFeedPlaylistAndSelection()
    {
        const QString first = createTone("first.wav", 220.0, 330.0, 0.5);
        const QString second = createTone("second.wav", 440.0, 550.0, 0.5);
        const QString ignored = m_tempDir.filePath("ignored.txt");
        QFile ignoredFile(ignored);
        QVERIFY(ignoredFile.open(QIODevice::WriteOnly));
        ignoredFile.write("not audio");
        ignoredFile.close();

        PlaylistModel playlist;
        QSignalSpy selected(&playlist, &PlaylistModel::trackSelected);
        QSignalSpy emptied(&playlist, &PlaylistModel::emptied);

        playlist.addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(ignored),
                           QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
        QCOMPARE(playlist.count(), 2);
        QCOMPARE(playlist.currentIndex(), 0);
        QCOMPARE(selected.count(), 1);

        const QVariantMap metadata = playlist.getTrack(0);
        QCOMPARE(metadata.value("title").toString(), QStringLiteral("first"));
        QCOMPARE(metadata.value("artist").toString(), QStringLiteral("Unknown Artist"));
        QCOMPARE(metadata.value("formatLabel").toString(), QStringLiteral("PCM STEREO"));
        QCOMPARE(metadata.value("sampleRate").toInt(), 48000);
        QCOMPARE(metadata.value("channels").toInt(), 2);
        QCOMPARE(metadata.value("bitDepth").toInt(), 16);
        QVERIFY(metadata.value("bitrate").toLongLong() > 1000000);
        QVERIFY(metadata.value("duration").toDouble() > 0.45);
        QVERIFY(metadata.value("artworkUrl").toString().isEmpty());

        playlist.removeTrack(0);
        QCOMPARE(playlist.count(), 1);
        QCOMPARE(playlist.currentIndex(), 0);
        QCOMPARE(selected.count(), 2);

        playlist.clear();
        QCOMPARE(playlist.count(), 0);
        QCOMPARE(emptied.count(), 1);
    }

    void folderArtworkIsDiscovered()
    {
        const QString albumDirectory = m_tempDir.filePath("folder-art-album");
        QVERIFY(QDir().mkpath(albumDirectory));
        const QString tone = createTone("folder-art-album/track.wav", 220.0, 440.0, 0.5);
        const QString coverPath = albumDirectory + QStringLiteral("/cover.png");
        QFile cover(coverPath);
        QVERIFY(cover.open(QIODevice::WriteOnly));
        cover.write(QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="));
        cover.close();

        PlaylistModel playlist;
        playlist.addFile(tone);
        QCOMPARE(playlist.count(), 1);
        QCOMPARE(playlist.getTrack(0).value("artworkUrl").toString(),
                 QUrl::fromLocalFile(coverPath).toString());
    }

    void playbackTransportAndAnalysisAreWired()
    {
        const QString tone = createTone("integration.wav", 240.0, 960.0, 8.0);
        QVERIFY(!tone.isEmpty());

        AudioPlayer player;
        player.setVolume(0.8);
        QSignalSpy ended(&player, &AudioPlayer::trackEnded);

        player.load(tone);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.leftMeter() > 0.02, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.rightMeter() > 0.02, 3000);

        bool spectrumActive = false;
        for (const QVariant &band : player.spectrum()) spectrumActive |= band.toDouble() > 0.02;
        QVERIFY(spectrumActive);

        player.pause();
        QTRY_VERIFY_WITH_TIMEOUT(!player.isPlaying(), 1000);
        QTest::qWait(700);
        QVERIFY(player.leftMeter() < 0.03);
        QVERIFY(player.rightMeter() < 0.03);

        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 1500);
        player.seek(1.0);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() >= 0.8, 1500);

        player.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!player.isPlaying(), 1000);
        QVERIFY(!player.hasLoadedMedia());
        QCOMPARE(ended.count(), 0);
    }

    void endOfTrackAdvancesAndClearStopsPlayback()
    {
        const QString first = createTone("short-first.wav", 320.0, 480.0, 0.7);
        const QString second = createTone("long-second.wav", 180.0, 720.0, 3.0);

        AudioPlayer player;
        player.setVolume(0.25);
        PlaylistModel playlist;

        connect(&playlist, &PlaylistModel::trackSelected, &player,
                [&player](const QString &path) {
                    player.load(path);
                    player.play();
                });
        connect(&player, &AudioPlayer::trackEnded, &playlist, &PlaylistModel::nextTrack);
        connect(&playlist, &PlaylistModel::emptied, &player, &AudioPlayer::stop);

        playlist.addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
        QCOMPARE(playlist.currentIndex(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(playlist.currentIndex(), 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(player.currentFile() == QFileInfo(second).absoluteFilePath(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);

        playlist.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!player.isPlaying(), 1000);
        QVERIFY(!player.hasLoadedMedia());
    }

    void visualizerUsesCuratedLibraryAndCompleteLaunchSettings()
    {
        VisualizerLauncher launcher(nullptr, false);

        QVERIFY2(launcher.curatedPresetCount() >= 100,
                 qPrintable(QStringLiteral("Expected the curated visual library, found %1 presets in %2")
                                .arg(launcher.curatedPresetCount())
                                .arg(launcher.activePresetDirectory())));
        QVERIFY(launcher.activePresetDirectory().endsWith(QStringLiteral("/visuals/curated/presets")));

        launcher.setTargetFps(0);
        launcher.setPresetDuration(8);
        launcher.setTransitionDuration(20.0);
        launcher.setBeatSensitivity(9.0);
        launcher.setHardCutDuration(99);

        QCOMPARE(launcher.transitionDuration(), 7.5);
        QCOMPARE(launcher.beatSensitivity(), 2.0);
        QCOMPARE(launcher.hardCutDuration(), 7);

        const QStringList arguments = launcher.buildArguments();
        QVERIFY(arguments.contains(QStringLiteral("--enableSplash=0")));
        QVERIFY(arguments.contains(QStringLiteral("--shuffleEnabled=1")));
        QVERIFY(arguments.contains(QStringLiteral("--fps=0")));
        QVERIFY(arguments.contains(QStringLiteral("--presetDuration=8")));
        QVERIFY(arguments.contains(QStringLiteral("--transitionDuration=7.5")));
        QVERIFY(arguments.contains(QStringLiteral("--beatSensitivity=2.0")));
        QVERIFY(arguments.contains(QStringLiteral("--hardCutDuration=7")));

        launcher.setPresetSource(QStringLiteral("ALL"));
        if (!launcher.fullLibraryAvailable()) {
            QCOMPARE(launcher.presetSource(), QStringLiteral("CURATED"));
        }
    }
};

QTEST_GUILESS_MAIN(AudioPlayerIntegrationTest)
#include "AudioPlayerIntegrationTest.moc"
