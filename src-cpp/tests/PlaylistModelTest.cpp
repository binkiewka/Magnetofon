#include "PlaylistModel.hpp"
#include "TrackMetadataReader.hpp"

#include <QtTest>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

class PlaylistModelTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_directory;

    QString tone(const QString &name)
    {
        const QString path = m_directory.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        constexpr quint32 bytes = 1024 * 4;
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + bytes);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(2) << quint32(48000)
               << quint32(192000) << quint16(4) << quint16(16);
        stream.writeRawData("data", 4);
        stream << bytes;
        file.write(QByteArray(bytes, '\0'));
        return path;
    }

    QString library()
    {
        const auto folder = m_directory.filePath("library");
        if (QFileInfo::exists(QDir(folder).filePath("track1999.wav"))) return folder;
        if (!QDir().mkpath(folder)) return {};
        const auto source = tone("seed.wav");
        for (int index = 0; index < 2000; ++index) {
            const auto target = QDir(folder).filePath(QStringLiteral("track%1.wav").arg(index));
            if (!QFileInfo::exists(target) && !QFile::copy(source, target)) return {};
        }
        return folder;
    }

private slots:

    void tinyCheckpointTracksSelectionWithoutRewritingQueueAndRejectsStaleState()
    {
        PlaylistModel model;
        const auto first = tone("checkpoint-first.wav");
        const auto second = tone("checkpoint-second.wav");
        model.addFile(first); model.addFile(second);
        const auto path = m_directory.filePath("checkpoint-session.json");
        QVERIFY(model.saveSession(1.5, path));
        QFile full(path); QVERIFY(full.open(QIODevice::ReadOnly));
        const QByteArray before = full.readAll(); full.close();
        model.activateTrack(1);
        QVERIFY(model.checkpointSessionPosition(9.5, path));
        QVERIFY(full.open(QIODevice::ReadOnly)); QCOMPARE(full.readAll(), before); full.close();
        QFile position(path + ".position.json"); QVERIFY(position.open(QIODevice::ReadOnly));
        const QByteArray oldCheckpoint = position.readAll(); position.close();
        QVERIFY(oldCheckpoint.size() < 1024);
        PlaylistModel restored;
        QSignalSpy selected(&restored, &PlaylistModel::trackSelected);
        QVERIFY(restored.restoreSession(path));
        QCOMPARE(restored.currentFilePath(), second);
        QCOMPARE(restored.restoredPosition(), 9.5);
        QCOMPARE(selected.count(), 0);
        QVERIFY(model.saveSession(4.0, path));
        QVERIFY(position.open(QIODevice::WriteOnly)); position.write(oldCheckpoint); position.close();
        QVERIFY(restored.restoreSession(path));
        QCOMPARE(restored.restoredPosition(), 4.0); // Old checkpoint cannot override newer full save.
        QVERIFY(position.open(QIODevice::WriteOnly)); position.write(QByteArray(65537, ' ')); position.close();
        QVERIFY(restored.restoreSession(path));
        QCOMPARE(restored.restoredPosition(), 4.0);
        QVERIFY(!model.checkpointSessionPosition(5.0, first + "/not-a-folder.json"));
        QVERIFY(model.operationError().startsWith("Save session position:"));
    }

    void extensionlessContentAndUnknownExplicitSuffixAreAccepted()
    {
        const auto source = tone("content-source.wav");
        const auto bare = m_directory.filePath("09-Halcyon_and_On_and_On");
        const auto unknown = m_directory.filePath("album.custom");
        QVERIFY(QFile::copy(source, bare));
        QVERIFY(QFile::copy(source, unknown));
        PlaylistModel model;
        model.importFiles({QUrl::fromLocalFile(bare), QUrl::fromLocalFile(unknown)});
        QTRY_VERIFY_WITH_TIMEOUT(!model.importing(), 5000);
        QCOMPARE(model.count(), 2);
        QCOMPARE(model.currentFilePath(), bare);
        QVERIFY(model.getTrack(0).value("channels").toInt() > 0);
    }

    void activatingCurrentRowEmitsAgainAndSearchMapsStableSourceRows()
    {
        PlaylistModel model;
        model.addFile(tone("alpha.wav"));
        model.addFile(tone("beta.wav"));
        model.addFile(tone("gamma.wav"));
        QSignalSpy selected(&model, &PlaylistModel::trackSelected);
        model.activateTrack(0);
        model.activateTrack(0);
        QCOMPARE(selected.count(), 2);
        model.setSearchText("BETA");
        QCOMPARE(model.filteredModel()->rowCount(), 1);
        QCOMPARE(model.trackIndexForVisible(0), 1);
        QCOMPARE(model.visibleIndexForTrack(1), 0);
        QCOMPARE(model.visibleIndexForTrack(0), -1);
        model.activateTrack(model.trackIndexForVisible(0));
        QVERIFY(model.currentFilePath().endsWith("beta.wav"));
        model.moveTrack(1, 0);
        QCOMPARE(model.trackIndexForVisible(0), 0);
        QCOMPARE(model.filteredModel()->data(model.filteredModel()->index(0, 0), PlaylistModel::SourceIndexRole).toInt(), 0);
        QVERIFY(model.currentFilePath().endsWith("beta.wav"));
    }

    void sessionRestoresMetadataSelectionPositionWithoutAutoplay()
    {
        const auto first = tone("session-first.wav");
        const auto second = tone("session-second.wav");
        const auto path = m_directory.filePath("session.json");
        PlaylistModel model;
        model.addFile(first); model.addFile(second); model.setCurrentIndex(1);
        QVERIFY(model.saveSession(12.25, path));
        PlaylistModel restored;
        QSignalSpy selected(&restored, &PlaylistModel::trackSelected);
        QVERIFY(restored.restoreSession(path));
        QCOMPARE(restored.count(), 2);
        QCOMPARE(restored.currentFilePath(), second);
        QCOMPARE(restored.restoredPosition(), 12.25);
        QCOMPARE(selected.count(), 0);
        QVERIFY(QFile::remove(second));
        QVERIFY(restored.restoreSession(path));
        QCOMPARE(restored.count(), 1);
        QCOMPARE(restored.currentFilePath(), first);
        QCOMPARE(restored.restoredPosition(), 0.0);
        QCOMPARE(selected.count(), 0);
        QVERIFY(restored.operationError().contains("skipped"));
        QFile invalid(path); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("bad json"); invalid.close();
        QVERIFY(!restored.restoreSession(path));
        QCOMPARE(restored.count(), 1);
    }
    void filenameBoundariesRoundTrip()
    {
        PlaylistModel original;
        QStringList names{QStringLiteral("#intro.wav"), QStringLiteral("  lead.wav"),
                          QStringLiteral("percent%track.wav"), QString::fromUtf8("Zażółć.wav")};
#ifndef Q_OS_WIN
        names << QStringLiteral("new\nline.wav");
#endif
        QStringList expected;
        for (const auto &name : names) {
            const auto file = tone(name);
            QVERIFY(!file.isEmpty());
            original.addFile(file);
            expected.append(file);
        }
        QCOMPARE(original.count(), expected.size());
        const auto playlist = m_directory.filePath("boundary.m3u8");
        QVERIFY(original.savePlaylist(playlist));
        QFile saved(playlist);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto content = saved.readAll();
        QVERIFY(content.contains("./#intro.wav"));
        PlaylistModel restored;
        QVERIFY(restored.loadPlaylist(playlist));
        QCOMPARE(restored.count(), original.count());
        for (int index = 0; index < expected.size(); ++index)
            QCOMPARE(restored.getTrack(index).value("filePath").toString(), expected[index]);
    }

    void failuresAreVisibleAndKeepExistingQueue()
    {
        PlaylistModel model;
        model.addFile(tone("keep.wav"));
        const auto before = model.currentFilePath();
        QSignalSpy errors(&model, &PlaylistModel::operationFailed);
        QVERIFY(!model.savePlaylist(m_directory.filePath("missing/subdir/output.m3u8")));
        QVERIFY(!model.operationError().isEmpty());
        QCOMPARE(errors.count(), 1);
        QVERIFY(!model.loadPlaylist(m_directory.filePath("missing.m3u8")));
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.currentFilePath(), before);
        QCOMPARE(errors.count(), 2);
        QVERIFY(model.savePlaylist(m_directory.filePath("valid.m3u8")));
        QVERIFY(model.operationError().isEmpty());
    }

    void folderImportKeepsEventLoopResponsiveAndModelOnOwnerThread()
    {
        const auto folder = library();
        QVERIFY(!folder.isEmpty());
        PlaylistModel model;
        QSignalSpy finished(&model, &PlaylistModel::importFinished);
        bool ownerThreadOnly = true;
        connect(&model, &QAbstractItemModel::rowsInserted, &model,
                [&] { ownerThreadOnly &= QThread::currentThread() == model.thread(); });
        bool eventLoopRanDuringImport = false;
        model.importFolder(folder);
        QVERIFY(model.importing());
        QCOMPARE(model.count(), 0);
        QTimer::singleShot(0, &model, [&] { eventLoopRanDuringImport = model.importing(); });
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QVERIFY(eventLoopRanDuringImport);
        QVERIFY(ownerThreadOnly);
        QCOMPARE(model.count(), 2000);
        QCOMPARE(model.importedCount(), 2000);
        QVERIFY(!model.importing());
        QVERIFY(model.operationError().isEmpty());
        QCOMPARE(model.getTrack(2).value("filePath").toString(), QDir(folder).filePath("track2.wav"));
    }

    void clearCancelsOldGenerationWithoutLateInsertions()
    {
        PlaylistModel model;
        QSignalSpy finished(&model, &PlaylistModel::importFinished);
        const auto folder = library();
        QVERIFY(!folder.isEmpty());
        model.importFolder(folder);
        model.clear();
        QVERIFY(!model.importing());
        QCOMPARE(model.count(), 0);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.first().at(1).toBool());
        model.importFiles({QUrl::fromLocalFile(tone("after-clear.wav"))});
        QTRY_VERIFY_WITH_TIMEOUT(!model.importing(), 5000);
        QCOMPARE(model.count(), 1);
        QTest::qWait(200);
        QCOMPARE(model.count(), 1);
        QVERIFY(model.currentFilePath().endsWith(QStringLiteral("after-clear.wav")));
    }

    void cancelKeepsAlreadyAddedTracksWithoutLateInsertions()
    {
        PlaylistModel model;
        const auto folder = library();
        QVERIFY(!folder.isEmpty());
        model.importFolder(folder);
        QTRY_VERIFY_WITH_TIMEOUT(model.count() > 0, 5000);
        QVERIFY(model.importing());
        model.cancelImport();
        const auto kept = model.count();
        QVERIFY(kept > 0 && kept < 2000);
        QTest::qWait(200);
        QCOMPARE(model.count(), kept);
        QVERIFY(!model.importing());
    }

    void invalidAsyncPlaylistKeepsQueueAndReportsFailure()
    {
        PlaylistModel model;
        model.addFile(tone("existing.wav"));
        const auto original = model.currentFilePath();
        model.importPlaylist(m_directory.filePath("does-not-exist.m3u8"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.importing(), 5000);
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.currentFilePath(), original);
        QVERIFY(!model.operationError().isEmpty());
    }

    void asyncPlaylistReplacesOnlyAfterValidation()
    {
        PlaylistModel model;
        model.addFile(tone("replace-me.wav"));
        PlaylistModel desired;
        desired.addFile(tone("#intro.wav"));
        desired.addFile(tone("next.wav"));
        const auto playlist = m_directory.filePath("async.m3u8");
        QVERIFY(desired.savePlaylist(playlist));
        model.importPlaylist(playlist);
        QCOMPARE(model.count(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!model.importing(), 5000);
        QCOMPARE(model.count(), 2);
        QVERIFY(model.currentFilePath().endsWith(QStringLiteral("#intro.wav")));
    }

    void destructionDuringImportHasNoModelCallback()
    {
        const auto folder = library();
        QVERIFY(!folder.isEmpty());
        for (int count = 0; count < 20; ++count) {
            auto *model = new PlaylistModel;
            model->importFolder(folder);
            delete model;
        }
        // Let independent workers observe cancellation. A late callback/UAF
        // would be detected here by the sanitizer build as well.
        QTest::qWait(100);
    }

    void metadataCancellationIsObservedBeforeProbing()
    {
        const auto path = tone("cancelled-probe.wav");
        const auto metadata = TrackMetadataReader::read(path, [] { return true; });
        QCOMPARE(metadata.channels, 0);
        QVERIFY(metadata.artworkUrl.isEmpty());
    }

    void fileOpenRequestsAreQueuedAndClearDiscardsPendingRequests()
    {
        PlaylistModel model;
        const auto first = tone("request-one.wav");
        const auto second = tone("request-two.wav");
        QSignalSpy finished(&model, &PlaylistModel::importFinished);
        model.importFiles({QUrl::fromLocalFile(first)});
        model.importFiles({QUrl::fromLocalFile(second)});
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 5000);
        QCOMPARE(model.count(), 2);
        QCOMPARE(model.getTrack(0).value("filePath").toString(), first);
        QCOMPARE(model.getTrack(1).value("filePath").toString(), second);
        QVERIFY(model.operationError().isEmpty());

        model.importFiles({QUrl::fromLocalFile(tone("cancel-request-one.wav"))});
        model.importFiles({QUrl::fromLocalFile(tone("cancel-request-two.wav"))});
        model.clear();
        QTest::qWait(200);
        QCOMPARE(model.count(), 0);
        QVERIFY(!model.importing());
    }
};

QTEST_GUILESS_MAIN(PlaylistModelTest)
#include "PlaylistModelTest.moc"
