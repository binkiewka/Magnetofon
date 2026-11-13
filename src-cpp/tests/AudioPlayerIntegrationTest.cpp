#include <QtTest>

#include "AudioPlayer.hpp"
#include "AudioRouting.hpp"
#include "PlaylistModel.hpp"
#include "PresetPackDownloader.hpp"
#include "TrackMetadataReader.hpp"
#include "VisualizerLauncher.hpp"
#include "VuMeterItem.hpp"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <cmath>
#include <limits>

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

    QString createSurroundTone(const QString &name, double seconds = 1.0)
    {
        const QString path = m_tempDir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};

        constexpr quint32 sampleRate = 48000;
        constexpr quint16 channels = 6;
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

        const double frequencies[channels] = {220.0, 330.0, 440.0, 55.0, 660.0, 880.0};
        for (quint32 i = 0; i < frames; ++i) {
            const double time = static_cast<double>(i) / sampleRate;
            for (double frequency : frequencies) {
                stream << static_cast<qint16>(std::sin(time * frequency * 2.0 * M_PI) * 8000);
            }
        }
        return path;
    }

private slots:
    void init()
    {
        QSettings().clear(); // Each scenario owns a fresh profile; persistence is tested within a scenario.
    }

    void missingSelectionStopsOldIdentityAndCanRetry()
    {
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        const auto first = createTone("missing-first.wav", 220, 330, 3);
        const auto second = createTone("missing-second.wav", 440, 550, 3);
        playlist.addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QVERIFY(QFile::remove(second));
        playlist.setCurrentIndex(1);
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackError().isEmpty(), 3000);
        QVERIFY(!player.isPlaying());
        QVERIFY(!player.hasLoadedMedia());
        QVERIFY(player.currentFile().isEmpty());
        QCOMPARE(playlist.currentIndex(), 1);
        QVERIFY(QFile::copy(first, second));
        player.retry();
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QCOMPARE(player.currentFile(), second);
        QVERIFY(player.playbackError().isEmpty());
        player.stop();
    }

    void corruptMediaShowsErrorAndStopsLoading()
    {
        QFile broken(m_tempDir.filePath("corrupt.wav"));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("This is not a media stream");
        broken.close();
        AudioPlayer player;
        QVERIFY(player.load(broken.fileName()));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackError().isEmpty(), 3000);
        QVERIFY(!player.loading());
        QVERIFY(!player.hasLoadedMedia());
        QVERIFY(!player.isPlaying());
    }

    void selectedLoadedRowResumesAndStoppedRowRestarts()
    {
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        const auto file = createTone("reactivate.wav", 220, 330, 3);
        playlist.addFile(file);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QTest::qWait(200);
        player.pause();
        const double paused = player.position();
        playlist.trackSelected(file); // The model's explicit activateTrack emits even for the same index.
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QVERIFY(player.position() >= paused - 0.05);
        player.stop();
        QVERIFY(!player.hasLoadedMedia());
        playlist.trackSelected(file);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QCOMPARE(player.currentFile(), file);
        player.stop();
    }

    void rapidLoadReplacementAndStopRetireOldEvents()
    {
        AudioPlayer player;
        const auto first = createTone("rapid-first.wav", 220, 330, 3);
        const auto last = createTone("rapid-last.wav", 440, 550, 3);
        QFile corrupt(m_tempDir.filePath("rapid-corrupt.wav"));
        QVERIFY(corrupt.open(QIODevice::WriteOnly)); corrupt.write("bad media"); corrupt.close();
        QVERIFY(player.load(first)); player.play();
        QVERIFY(player.load(corrupt.fileName())); player.play();
        QVERIFY(player.load(last)); player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia() && player.isPlaying(), 3000);
        QCOMPARE(player.currentFile(), last);
        QVERIFY(player.playbackError().isEmpty());
        QTest::qWait(150);
        QCOMPARE(player.currentFile(), last);
        player.stop();
        QTest::qWait(150);
        QVERIFY(!player.isPlaying());
        QVERIFY(!player.hasLoadedMedia());
        QVERIFY(!player.loading());
    }

    void retiredNativeTerminalEventCannotCancelNewLoad_data()
    {
        QTest::addColumn<QString>("terminal");
        QTest::newRow("old-eof-new-path") << QString("EOF");
        QTest::newRow("old-eof-same-path-reload") << QString("SAME");
        QTest::newRow("old-error-new-path") << QString("ERROR");
        QTest::newRow("stop-then-new-load") << QString("STOP");
    }

    void retiredNativeTerminalEventCannotCancelNewLoad()
    {
        QFETCH(QString, terminal);
        AudioPlayer player;
        player.setRepeatMode("OFF");
        const auto first = createTone("retired-native.wav", 220, 330, 0.35);
        QVERIFY(player.load(first));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QSignalSpy finished(&player, &AudioPlayer::trackEnded);
        if (terminal == "ERROR") {
            QFile bad(m_tempDir.filePath("retired-error.wav"));
            QVERIFY(bad.open(QIODevice::WriteOnly));
            bad.write("not media");
            bad.close();
            QVERIFY(player.load(bad.fileName()));
            player.play();
            QThread::msleep(300); // Queue native ERROR without pumping Qt.
        } else if (terminal == "STOP") {
            player.stop();
        } else {
            QThread::msleep(900); // Queue native EOF before selecting another song.
        }
        const auto requested = createTone(terminal == "SAME" ? "retired-native.wav" : "accepted-native.wav",
                                          440, 550, 4);
        QVERIFY(player.load(requested));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia() && player.isPlaying(), 3000);
        QTest::qWait(250);
        QCOMPARE(player.currentFile(), requested);
        QVERIFY(player.hasLoadedMedia());
        QVERIFY(player.isPlaying());
        QVERIFY(!player.loading());
        QVERIFY(player.playbackError().isEmpty());
        QCOMPARE(finished.count(), 0); // A retired terminal event is not an album advance.
        player.stop();
    }

    void gaplessErrorIdentifiesFailedEntryAndSkipIsExplicit()
    {
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        player.setGaplessEnabled(true);
        player.setRepeatMode("OFF");
        const auto first = createTone("error-album-first.wav", 220, 330, 0.6);
        QFile bad(m_tempDir.filePath("error-album-broken.wav"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("not an audio stream");
        bad.close();
        const auto third = createTone("error-album-third.wav", 440, 550, 4);
        playlist.addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(bad.fileName()),
                           QUrl::fromLocalFile(third)});
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackError().isEmpty(), 3000);
        QCOMPARE(playlist.currentIndex(), 1);
        QCOMPARE(player.currentFile(), bad.fileName());
        QVERIFY(player.playbackError().contains("error-album-broken.wav"));
        QVERIFY(!player.playbackError().contains("error-album-first.wav"));
        QTest::qWait(150);
        QVERIFY(!player.isPlaying());
        QVERIFY(!player.loading());
        player.retry();
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackError().isEmpty() && !player.loading(), 3000);
        QCOMPARE(playlist.currentIndex(), 1);
        QCOMPARE(player.currentFile(), bad.fileName());
        player.next();
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        QCOMPARE(playlist.currentIndex(), 2);
        QCOMPARE(player.currentFile(), third);
        QVERIFY(player.playbackError().isEmpty());
        player.stop();
        player.setGaplessEnabled(false);
    }

    void soundProfilePersistsButStartupVolumeDoesNot()
    {
        const QVariantList bands{1,2,3,4,5,6,5,4,3,2};
        {
            AudioPlayer player;
            player.setSurroundMode("SURROUND");
            player.setEqEnabled(true);
            player.setPreamp(-7);
            player.setEqBands(bands);
            player.setRepeatMode("ALL");
            player.setProtectionEnabled(false);
            player.setVolume(0.25);
        }
        AudioPlayer restored;
        QCOMPARE(restored.surroundMode(), QString("SURROUND"));
        QVERIFY(restored.eqEnabled());
        QCOMPARE(restored.preamp(), -7.0);
        QCOMPARE(restored.eqBands(), bands);
        QCOMPARE(restored.repeatMode(), QString("ALL"));
        QVERIFY(!restored.protectionEnabled());
        QCOMPARE(restored.volume(), 0.5);
        QVERIFY(std::abs(restored.volumeDb() + 18.0617997) < 0.00001);
        restored.setSurroundMode("INVALID");
        QCOMPARE(restored.surroundMode(), QString("SURROUND"));
        restored.setPreamp(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(restored.preamp(), -7.0);
        restored.resetSoundProfile();
        QCOMPARE(restored.surroundMode(), QString("AUTO"));
        QVERIFY(!restored.eqEnabled());
        QVERIFY(restored.protectionEnabled());
    }

    void restoredMediaStaysPausedAtPosition()
    {
        AudioPlayer player;
        const auto file = createTone("restored.wav", 220, 330, 3);
        QVERIFY(player.restorePaused(file, 1.25));
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 1.1, 3000);
        QVERIFY(!player.isPlaying());
        QVERIFY(!player.loading());
        player.play();
        QTRY_VERIFY(player.isPlaying());
        QVERIFY(player.position() >= 1.1);
    }

    void repeatIsIndependentOfGapless_data()
    {
        QTest::addColumn<bool>("gapless");
        QTest::addColumn<QString>("repeat");
        for (bool gapless : {false, true})
            for (const QString &repeat : {QString("OFF"), QString("ALL"), QString("ONE")})
                QTest::newRow(qPrintable(QString("%1-%2").arg(gapless ? "gapless" : "normal", repeat))) << gapless << repeat;
    }

    void repeatIsIndependentOfGapless()
    {
        QFETCH(bool, gapless);
        QFETCH(QString, repeat);
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        player.setGaplessEnabled(gapless);
        player.setRepeatMode(repeat);
        QSignalSpy finished(&player, &AudioPlayer::trackEnded);
        int changes = 0;
        connect(&playlist, &PlaylistModel::currentIndexChanged, &playlist, [&] { ++changes; });
        playlist.addFiles({QUrl::fromLocalFile(createTone("repeat-a.wav",220,330,0.6)),
                           QUrl::fromLocalFile(createTone("repeat-b.wav",440,550,0.6))});
        QCOMPARE(playlist.count(), 2);
        QCOMPARE(playlist.currentIndex(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 3000);
        if (repeat == "OFF") {
            QTRY_COMPARE_WITH_TIMEOUT(playlist.currentIndex(), 1, 3000);
            // A normal queue briefly stops while selecting the next file. Wait
            // for the album's final EOF, not that intermediate loading state.
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), gapless ? 1 : 2, 4000);
            QVERIFY(!player.isPlaying());
            QTest::qWait(300);
            QCOMPARE(playlist.currentIndex(), 1);
            QVERIFY(!player.isPlaying());
        } else {
            QTest::qWait(2300);
            QVERIFY(player.isPlaying());
            if (repeat == "ONE") QCOMPARE(playlist.currentIndex(), 0);
            else QVERIFY(changes >= 4); // More than one album cycle; not a one-off wrap.
        }
        player.stop();
    }

    void initTestCase()
    {
        std::setlocale(LC_NUMERIC, "C");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tempDir.path());
    }

    void gaplessAlbumProducesCompletePcmWithoutReloads()
    {
        const QString output = m_tempDir.filePath("gapless-output.wav");
        qputenv("MAGNETOFON_TEST_PCM_FILE", QFile::encodeName(output));
        {
            PlaylistModel playlist;
            AudioPlayer player;
            qunsetenv("MAGNETOFON_TEST_PCM_FILE");
            player.setPlaylist(&playlist);
            player.setGaplessEnabled(true);
            player.setVolume(1);
            QSignalSpy selections(&playlist, &PlaylistModel::trackSelected);
            QSignalSpy finished(&player, &AudioPlayer::trackEnded);
            playlist.addFiles({QUrl::fromLocalFile(createTone("gapless-1.wav", 100, 200, 0.8)),
                               QUrl::fromLocalFile(createTone("gapless-2.wav", 100, 200, 0.8)),
                               QUrl::fromLocalFile(createTone("gapless-3.wav", 100, 200, 0.8))});
            QTRY_COMPARE_WITH_TIMEOUT(playlist.currentIndex(), 2, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
            QVERIFY(!player.isPlaying());
            QCOMPARE(selections.count(), 1); // The following tracks never take the manual load path.
            player.setGaplessEnabled(false);
        } // Close output and finalize WAV header.
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(bytes.startsWith("RIFF"));
        quint16 blockAlign = 0;
        quint32 dataSize = 0;
        for (int offset = 12; offset + 8 <= bytes.size();) {
            QDataStream sizeStream(bytes.mid(offset + 4, 4));
            sizeStream.setByteOrder(QDataStream::LittleEndian);
            quint32 size; sizeStream >> size;
            if (bytes.mid(offset, 4) == "fmt ") {
                QDataStream formatStream(bytes.mid(offset + 8 + 12, 2));
                formatStream.setByteOrder(QDataStream::LittleEndian);
                formatStream >> blockAlign;
            }
            if (bytes.mid(offset, 4) == "data") { dataSize = size; break; }
            offset += 8 + size + (size % 2);
        }
        QVERIFY(blockAlign > 0);
        QCOMPARE(dataSize / blockAlign, quint32(3 * 38400)); // No added or dropped samples.
    }

    void gaplessQueueTracksEditsAndToggle()
    {
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        player.setVolume(0);
        player.setGaplessEnabled(true);
        const QString first = createTone("gap-edit1.wav", 100, 200, 0.8);
        const QString second = createTone("gap-edit2.wav", 200, 300, 2);
        const QString third = createTone("gap-edit3.wav", 300, 400, 2);
        playlist.addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second), QUrl::fromLocalFile(third)});
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);
        playlist.moveTrack(2, 1);
        QTRY_COMPARE_WITH_TIMEOUT(player.currentFile(), third, 3000);
        QCOMPARE(playlist.currentIndex(), 1);
        player.pause();
        QTRY_VERIFY(!player.isPlaying());
        player.setGaplessEnabled(false);
        QVERIFY(!player.gaplessEnabled());
        player.setGaplessEnabled(true);
        playlist.removeTrack(2);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(!player.hasLoadedMedia(), 4000);
        QCOMPARE(player.currentFile(), third);
        player.setGaplessEnabled(false);
        playlist.clear();
        QVERIFY(!player.isPlaying());
    }

    void manualSelectionStillStartsPlayback()
    {
        for (bool gapless : {false, true}) {
            PlaylistModel playlist;
            AudioPlayer player;
            player.setPlaylist(&playlist);
            player.setVolume(0);
            player.setGaplessEnabled(gapless);
            const QString first = createTone("manual-1.wav", 100, 200, 2);
            const QString second = createTone("manual-2.wav", 200, 300, 2);
            playlist.addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
            QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);
            playlist.setCurrentIndex(1);
            QTRY_COMPARE_WITH_TIMEOUT(player.currentFile(), second, 2000);
            QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);
            QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.05, 2000);
            player.stop();
            player.setGaplessEnabled(false);
        }
    }

    void gaplessStopAndSavedPreference()
    {
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        player.setVolume(0);
        player.setGaplessEnabled(true);
        { AudioPlayer restored; QVERIFY(restored.gaplessEnabled()); }
        const QString first = createTone("gap-stop1.wav", 100, 200, 2);
        playlist.addFiles({QUrl::fromLocalFile(first),
                           QUrl::fromLocalFile(createTone("gap-stop2.wav", 200, 300, 2))});
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);
        player.stop();
        QTest::qWait(150);
        QVERIFY(!player.isPlaying());
        QVERIFY(!player.hasLoadedMedia());
        QCOMPARE(playlist.currentIndex(), 0);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);
        QCOMPARE(player.currentFile(), first);
        playlist.clear();
        QTest::qWait(100);
        QVERIFY(!player.hasLoadedMedia());
        player.setGaplessEnabled(false);
    }

    void speakerProfilesAreIndependentAndPersisted()
    {
        AudioPlayer player;
        player.setSpeakerCutoff(65);
        player.setSubwooferCutoff(90);
        player.setSubwooferGain(-2);
        for (int i = 0; i < 5; ++i) player.setSurroundCutoff(i, 50 + i * 20);
        player.setSurroundSubwooferCutoff(130);
        player.setSurroundSubwooferGain(1.5);
        player.setSurroundCutoff(-1, 180);
        player.setSurroundCutoff(5, 180);
        AudioPlayer restored;
        QCOMPARE(restored.speakerCutoff(), 65);
        QCOMPARE(restored.subwooferCutoff(), 90);
        QCOMPARE(restored.subwooferGain(), -2.0);
        QCOMPARE(restored.surroundCutoffs(), QVariantList({50, 70, 90, 110, 130}));
        QCOMPARE(restored.surroundSubwooferCutoff(), 130);
        QCOMPARE(restored.surroundSubwooferGain(), 1.5);
        restored.resetSpeakerProfile(true);
        QCOMPARE(restored.speakerCutoff(), 65);
        QCOMPARE(restored.subwooferCutoff(), 90);
        QCOMPARE(restored.surroundCutoffs(), QVariantList({80, 80, 80, 80, 80}));
        QCOMPARE(restored.surroundSubwooferCutoff(), 120);
        restored.setSurroundCutoff(0, 95);
        restored.resetSpeakerProfile(false);
        QCOMPARE(restored.surroundCutoffs()[0].toInt(), 95);
        QCOMPARE(restored.speakerCutoff(), 80);
        restored.resetSpeakerProfile(true);
    }

    void surroundToStereoTrackTransition()
    {
        AudioPlayer player;
        player.setVolume(0);
        player.setSurroundMode("SURROUND");
        player.load(createSurroundTone("transition51.wav", 2));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.1, 3000);
        QCOMPARE(player.sourceChannels(), 6);
        player.load(createTone("transition2.wav", 100, 300, 2));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.1, 3000);
        QCOMPARE(player.sourceChannels(), 2);
        player.stop();
    }

    void reorderPreservesActiveTrack()
    {
        PlaylistModel playlist;
        for (int i = 0; i < 4; ++i) playlist.addFile(createTone(QString("move%1.wav").arg(i), 100, 200, 0.1));
        playlist.setCurrentIndex(1);
        const QString current = playlist.currentFilePath();
        QSignalSpy selected(&playlist, &PlaylistModel::trackSelected);
        playlist.moveTrack(0, 3);
        QCOMPARE(playlist.currentIndex(), 0);
        QCOMPARE(playlist.currentFilePath(), current);
        playlist.moveTrack(3, 0);
        QCOMPARE(playlist.currentIndex(), 1);
        QCOMPARE(playlist.currentFilePath(), current);
        playlist.moveTrack(1, 3);
        QCOMPARE(playlist.currentIndex(), 3);
        QCOMPARE(playlist.currentFilePath(), current);
        QCOMPARE(selected.count(), 0);
    }

    void stereoBassModePlays()
    {
        AudioPlayer player;
        QCOMPARE(player.volume(), 0.5);
        player.setVolume(0.0);
        player.setSurroundMode("2.1");
        player.load(createTone("bass-mode.wav", 60, 1000));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.1, 5000);
        QCOMPARE(player.sourceChannels(), 2);
        player.setSurroundMode("SURROUND");
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.3, 5000);
        player.setSurroundMode("AUTO");
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.5, 5000);
        player.stop();
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

    void routingModesPreserveNativeSurround()
    {
        QCOMPARE(AudioRouting::outputChannels(QStringLiteral("AUTO")), QStringLiteral("auto-safe"));
        QCOMPARE(AudioRouting::outputChannels(QStringLiteral("SURROUND")), QStringLiteral("auto-safe"));
        QCOMPARE(AudioRouting::outputChannels(QStringLiteral("STEREO")), QStringLiteral("stereo"));

        QVERIFY(AudioRouting::shouldUpmixToSurround(QStringLiteral("SURROUND"), 2));
        QVERIFY(!AudioRouting::shouldUpmixToSurround(QStringLiteral("SURROUND"), 6));
        QVERIFY(!AudioRouting::shouldUpmixToSurround(QStringLiteral("AUTO"), 2));
        QVERIFY(!AudioRouting::shouldUpmixToSurround(QStringLiteral("STEREO"), 6));
    }

    void playerDetectsNativeSurroundSource()
    {
        const QString tone = createSurroundTone("native-surround.wav");
        QVERIFY(!tone.isEmpty());

        AudioPlayer player;
        player.setSurroundMode(QStringLiteral("SURROUND"));
        player.load(tone);
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 3000);
        QCOMPARE(player.sourceChannels(), 6);
    }

    void externalSurroundFlacIsPreserved()
    {
        const QString path = qEnvironmentVariable("MAGNETOFON_SURROUND_TEST_FILE");
        if (path.isEmpty()) QSKIP("Set MAGNETOFON_SURROUND_TEST_FILE to test a real file");
        QVERIFY2(QFileInfo::exists(path), qPrintable(QStringLiteral("Missing test file: ") + path));

        const TrackMetadata metadata = TrackMetadataReader::read(path);
        QCOMPARE(metadata.codec, QStringLiteral("FLAC"));
        QVERIFY(metadata.channels > 2);
        QVERIFY(metadata.formatLabel.contains(QStringLiteral("5.1"))
                || metadata.formatLabel.contains(QStringLiteral("7.1"))
                || metadata.formatLabel.contains(QStringLiteral(" CH")));

        AudioPlayer player;
        player.setSurroundMode(QStringLiteral("SURROUND"));
        player.load(path);
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 5000);
        QCOMPARE(player.sourceChannels(), metadata.channels);
        QVERIFY(!AudioRouting::shouldUpmixToSurround(player.surroundMode(),
                                                      player.sourceChannels()));
    }

    void videoContainerIsAcceptedByPlaylist()
    {
        const QString path = m_tempDir.filePath(QStringLiteral("concert.mkv"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not-a-real-container");
        file.close();

        PlaylistModel playlist;
        playlist.addFile(path);
        QCOMPARE(playlist.count(), 1);
        QCOMPARE(playlist.getTrack(0).value(QStringLiteral("filePath")).toString(), path);
    }

    void externalCompressedSurroundVideoDecodesToPcm()
    {
        const QString path = qEnvironmentVariable("MAGNETOFON_VIDEO_TEST_FILE");
        if (path.isEmpty()) QSKIP("Set MAGNETOFON_VIDEO_TEST_FILE to test a real video");
        QVERIFY2(QFileInfo::exists(path), qPrintable(QStringLiteral("Missing test file: ") + path));

        const TrackMetadata metadata = TrackMetadataReader::read(path);
        QVERIFY(metadata.hasVideo);
        QVERIFY(metadata.videoWidth > 0);
        QVERIFY(metadata.videoHeight > 0);
        QVERIFY(metadata.channels > 2);
        QVERIFY(metadata.codec.startsWith(QStringLiteral("DTS"))
                || metadata.codec.contains(QStringLiteral("TRUEHD"))
                || metadata.codec.contains(QStringLiteral("DOLBY DIGITAL")));

        PlaylistModel playlist;
        playlist.addFile(path);
        QCOMPARE(playlist.count(), 1);
        QVERIFY(playlist.getTrack(0).value(QStringLiteral("hasVideo")).toBool());

        AudioPlayer player;
        player.setVolume(0.0);
        player.setSurroundMode(QStringLiteral("SURROUND"));
        player.load(path);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasLoadedMedia(), 7000);
        QTRY_VERIFY_WITH_TIMEOUT(player.hasVideo(), 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!player.audioTracks().isEmpty(), 7000);
        QCOMPARE(player.sourceChannels(), metadata.channels);
        QVERIFY(!AudioRouting::shouldUpmixToSurround(player.surroundMode(), player.sourceChannels()));
        QTRY_VERIFY_WITH_TIMEOUT(player.outputChannels() > 0, 7000);
        QVERIFY(player.decodedAudioLabel().contains(QStringLiteral("PCM")));

        player.hideVideo();
        QTRY_VERIFY_WITH_TIMEOUT(!player.videoVisible(), 1000);
        QVERIFY(player.isPlaying());
        player.showVideo();
        QTRY_VERIFY_WITH_TIMEOUT(player.videoVisible(), 1000);
        player.stop();
    }

    void externalCompressedVideoLibraryIsRecognized()
    {
        const QString directoryPath = qEnvironmentVariable("MAGNETOFON_VIDEO_TEST_DIR");
        if (directoryPath.isEmpty()) QSKIP("Set MAGNETOFON_VIDEO_TEST_DIR to scan a video library");

        const QDir directory(directoryPath);
        QVERIFY2(directory.exists(), qPrintable(QStringLiteral("Missing directory: ") + directoryPath));
        const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.mkv")},
                                                             QDir::Files | QDir::Readable,
                                                             QDir::Name | QDir::IgnoreCase);
        QVERIFY(!files.isEmpty());

        for (const QFileInfo &file : files) {
            const TrackMetadata metadata = TrackMetadataReader::read(file.absoluteFilePath());
            QVERIFY2(metadata.hasVideo, qPrintable(file.fileName() + QStringLiteral(" has no video stream")));
            QVERIFY2(metadata.videoWidth > 0 && metadata.videoHeight > 0,
                     qPrintable(file.fileName() + QStringLiteral(" has invalid video dimensions")));
            QVERIFY2(metadata.channels > 2,
                     qPrintable(file.fileName() + QStringLiteral(" is not multichannel")));
            QVERIFY2(!metadata.codec.isEmpty(),
                     qPrintable(file.fileName() + QStringLiteral(" has no recognized audio codec")));
        }
    }

    void externalVideoAudioTrackCanBeSwitched()
    {
        const QString path = qEnvironmentVariable("MAGNETOFON_MULTITRACK_VIDEO_TEST_FILE");
        if (path.isEmpty()) QSKIP("Set MAGNETOFON_MULTITRACK_VIDEO_TEST_FILE to test track selection");
        QVERIFY2(QFileInfo::exists(path), qPrintable(QStringLiteral("Missing test file: ") + path));

        AudioPlayer player;
        player.setVolume(0.0);
        player.load(path);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.audioTracks().size() >= 2, 7000);

        const QVariantMap target = player.audioTracks().constLast().toMap();
        const int targetId = target.value(QStringLiteral("id")).toInt();
        player.selectAudioTrack(targetId);
        QTRY_COMPARE_WITH_TIMEOUT(player.selectedAudioTrackId(), targetId, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(player.sourceChannels(), target.value(QStringLiteral("channels")).toInt(), 5000);
        player.stop();
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

    void nestedFolderDiscoveryUsesNaturalOrderAndAvoidsDuplicates()
    {
        const QString library = m_tempDir.filePath(QStringLiteral("discovery-library"));
        QVERIFY(QDir().mkpath(library + QStringLiteral("/Album 2")));
        QVERIFY(QDir().mkpath(library + QStringLiteral("/Album 10/Disc 1")));

        const QString first = createTone(QStringLiteral("discovery-library/Album 2/02 - Second.wav"),
                                         220.0, 330.0, 0.2);
        const QString second = createTone(QStringLiteral("discovery-library/Album 2/10 - Tenth.flac"),
                                          330.0, 440.0, 0.2);
        const QString third = createTone(QStringLiteral("discovery-library/Album 10/Disc 1/01 - Finale.wav"),
                                         440.0, 550.0, 0.2);
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        QVERIFY(!third.isEmpty());

        QFile ignored(library + QStringLiteral("/Album 2/cover.jpg"));
        QVERIFY(ignored.open(QIODevice::WriteOnly));
        ignored.write("not media");
        ignored.close();

        PlaylistModel playlist;
        QSignalSpy discovered(&playlist, &PlaylistModel::folderDiscovered);
        playlist.addFolder(library);

        QCOMPARE(playlist.count(), 3);
        QCOMPARE(discovered.count(), 1);
        QCOMPARE(discovered.constFirst().at(1).toInt(), 3);
        QCOMPARE(playlist.getTrack(0).value(QStringLiteral("filePath")).toString(), first);
        QCOMPARE(playlist.getTrack(1).value(QStringLiteral("filePath")).toString(), second);
        QCOMPARE(playlist.getTrack(2).value(QStringLiteral("filePath")).toString(), third);

        playlist.addFiles({QUrl::fromLocalFile(library)});
        QCOMPARE(playlist.count(), 3);
        QCOMPARE(discovered.count(), 2);
        QCOMPARE(discovered.constLast().at(1).toInt(), 0);
    }

    void playlistM3u8RoundTripPreservesOrder()
    {
        const QString first = createTone(QStringLiteral("playlist-first.wav"), 220.0, 330.0, 0.2);
        const QString second = createTone(QStringLiteral("playlist-second.wav"), 440.0, 550.0, 0.2);
        const QString playlistPath = m_tempDir.filePath(QStringLiteral("saved-playlist.m3u8"));

        PlaylistModel source;
        source.addFile(second);
        source.addFile(first);
        QVERIFY(source.savePlaylist(playlistPath));

        QFile savedPlaylist(playlistPath);
        QVERIFY(savedPlaylist.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray contents = savedPlaylist.readAll();
        QVERIFY(contents.startsWith("#EXTM3U\n"));
        QVERIFY(contents.contains("playlist-second.wav"));
        QVERIFY(contents.contains("playlist-first.wav"));

        PlaylistModel restored;
        QVERIFY(restored.loadPlaylist(playlistPath));
        QCOMPARE(restored.count(), 2);
        QCOMPARE(restored.getTrack(0).value(QStringLiteral("filePath")).toString(), second);
        QCOMPARE(restored.getTrack(1).value(QStringLiteral("filePath")).toString(), first);

        QVERIFY(restored.loadPlaylist(playlistPath));
        QCOMPARE(restored.count(), 2);
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

    void presetDownloaderCreatesNestedUserStorage()
    {
        const bool hadDataHome = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
        const QString dataHome = m_tempDir.filePath(QStringLiteral("missing/parents/user-data"));
        QVERIFY(!QDir(dataHome).exists());
        QVERIFY(qputenv("XDG_DATA_HOME", dataHome.toUtf8()));

        PresetPackDownloader downloader;
        const QString expected = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                     .filePath(QStringLiteral("visuals"));
        const QFileInfo storage(expected);
        QVERIFY2(storage.isDir(), qPrintable(QStringLiteral("Preset directory was not created: ") + expected));
        QVERIFY2(storage.isWritable(), qPrintable(QStringLiteral("Preset directory is not writable: ") + expected));

        if (hadDataHome) qputenv("XDG_DATA_HOME", previousDataHome);
        else qunsetenv("XDG_DATA_HOME");
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
        // Null-output test builds deliberately cannot capture audio. Unavailable
        // measurement must remain truthful, not make up readings to pass this test.
        QVERIFY(!player.analysisAvailable());
        QCOMPARE(player.leftMeter(), 0.0);
        QCOMPARE(player.rightMeter(), 0.0);
        for (const QVariant &band : player.spectrum()) QCOMPARE(band.toDouble(), 0.0);

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

    void clearedOrRemovedLastTrackCannotBeReplayed()
    {
        for (bool removeLast : {false, true}) {
            PlaylistModel playlist;
            AudioPlayer player;
            player.setPlaylist(&playlist);
            playlist.addFile(createTone(removeLast ? "remove-last.wav" : "clear-play.wav", 0, 0, 4));
            QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 2000);
            if (removeLast) playlist.removeTrack(0); else playlist.clear();
            QCOMPARE(playlist.count(), 0);
            QCOMPARE(player.currentFile(), QString());
            QCOMPARE(player.duration(), 0.0);
            player.togglePlayPause();
            QTest::qWait(250);
            QVERIFY(!player.isPlaying());
            QVERIFY(!player.hasLoadedMedia());
            QCOMPARE(player.currentFile(), QString());
            QCOMPARE(player.leftMeter(), 0.0);
        }
    }

    void clearBeforeQueuedSelectionDoesNotResurrectMedia()
    {
        PlaylistModel playlist;
        AudioPlayer player;
        player.setPlaylist(&playlist);
        playlist.addFile(createTone("pending-clear.wav", 240, 960, 2));
        playlist.clear();
        player.play();
        QTest::qWait(200);
        QVERIFY(!player.hasLoadedMedia());
        QVERIFY(!player.isPlaying());
        QCOMPARE(player.currentFile(), QString());
    }

    void realQmlConsoleLoadsAndShowsRuntimeState()
    {
        qmlRegisterType<VuMeterItem>("Magnetofon", 1, 0, "VuMeterItem");
        QQuickStyle::setStyle("Basic");
        AudioPlayer player;
        PlaylistModel playlist;
        PresetPackDownloader packs;
        VisualizerLauncher visuals(nullptr, false);
        player.setPlaylist(&playlist);
        QStringList problems;
        QQmlApplicationEngine engine;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &errors) {
            for (const auto &error : errors) problems.append(error.toString());
        });
        engine.rootContext()->setContextProperty("audioPlayer", &player);
        engine.rootContext()->setContextProperty("playlistModel", &playlist);
        engine.rootContext()->setContextProperty("presetPackDownloader", &packs);
        engine.rootContext()->setContextProperty("visualizerLauncher", &visuals);
        engine.load(QUrl("qrc:/ui/qml/main.qml"));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto *controls = window->findChild<QObject *>("visualsControlPanel");
        QVERIFY(controls);
        controls->setProperty("visible", true);
        auto *launch = controls->findChild<QObject *>("launchVisualsButton");
        QVERIFY(launch);
        QCOMPARE(launch->property("enabled").toBool(), visuals.runtimeAvailable());
        QTest::qWait(100);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join('\n')));
        const QString captures = qEnvironmentVariable("MAGNETOFON_TEST_SCREENSHOT_DIR");
        if (!captures.isEmpty()) {
            QDir().mkpath(captures);
            const QImage controlsImage = window->grabWindow();
            QVERIFY(!controlsImage.isNull());
            QVERIFY(controlsImage.save(QDir(captures).filePath("visual-controls.png")));
            controls->setProperty("visible", false);
            QTest::qWait(60);
            const QImage consoleImage = window->grabWindow();
            QVERIFY(!consoleImage.isNull());
            QVERIFY(consoleImage.save(QDir(captures).filePath("console.png")));
        }
        // Qt 6.4's software render loop retains image textures if their items
        // are destroyed directly. A standalone Window+Image reproduces it without
        // app code. Retire image sources while renderable before fixture teardown;
        // no sanitizer suppression or production behavior change is needed.
        for (auto *item : window->findChildren<QQuickItem *>()) {
            if (item->inherits("QQuickImageBase")) item->setProperty("source", QUrl());
        }
        QTest::qWait(100);
        // A short-lived offscreen test window needs an event-loop turn to
        // release software scene-graph textures before the engine is destroyed.
        window->setPersistentGraphics(false);
        window->setPersistentSceneGraph(false);
        window->hide();
        window->releaseResources();
        QTest::qWait(100);
        delete window;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        engine.collectGarbage();
        engine.clearComponentCache();
        QTest::qWait(50);
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
        launcher.setTransitionDuration(0.0);
        QCOMPARE(launcher.transitionDuration(), 0.5);
        launcher.setTransitionDuration(20.0);
        launcher.setBeatSensitivity(9.0);
        launcher.setHardCutDuration(99);
        launcher.setBorderlessWindow(false);

        QCOMPARE(launcher.transitionDuration(), 7.5);
        QCOMPARE(launcher.beatSensitivity(), 2.0);
        QCOMPARE(launcher.hardCutDuration(), 7);

        const QStringList arguments = launcher.buildArguments();
        QVERIFY(arguments.contains(QStringLiteral("--enableSplash=0")));
        QVERIFY(arguments.contains(QStringLiteral("--borderless=0")));
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

QTEST_MAIN(AudioPlayerIntegrationTest)
#include "AudioPlayerIntegrationTest.moc"
