#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>

#include "VisualizerLauncher.hpp"

class VisualizerLauncherTest : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<QTemporaryDir> m_temp;
    QString m_oldCwd;
    QByteArray m_oldPath;
    QByteArray m_oldDataHome;
    bool m_hadDataHome = false;

    static void writeFile(const QString &path, const QByteArray &bytes, bool executable = false)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
        file.close();
        if (executable) QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                                   | QFile::ReadGroup | QFile::ExeGroup));
    }

    QString path(const QString &name) const { return m_temp->filePath(name); }

    QStringList launches() const
    {
        QFile file(path("launches"));
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    }

private slots:
    void init()
    {
#if !defined(Q_OS_LINUX)
        QSKIP("Fake shell runtime/default-sink fixtures are Linux-specific");
#else
        m_oldCwd = QDir::currentPath();
        m_oldPath = qgetenv("PATH");
        m_temp = std::make_unique<QTemporaryDir>();
        QVERIFY(m_temp->isValid());
        QVERIFY(QDir::setCurrent(m_temp->path()));
        qputenv("PATH", (path("bin") + QStringLiteral(":/usr/bin:/bin")).toUtf8());
        m_hadDataHome = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        m_oldDataHome = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", path("data").toUtf8());
        qputenv("MAGNETOFON_FAKE_SINK", path("sink").toUtf8());
        qputenv("MAGNETOFON_FAKE_AUDIO", path("audio").toUtf8());
        writeFile(path("audio"), "MAGNETOFON_AUDIO unavailable\n");
        qputenv("MAGNETOFON_FAKE_LAUNCHES", path("launches").toUtf8());
        writeFile(path("sink"), "first-output\n");
        writeFile(path("bin/pactl"), "#!/bin/sh\ncat \"$MAGNETOFON_FAKE_SINK\"\n", true);
        writeFile(path("visuals/curated/presets/demo.milk"), "[preset00]\n");
        writeFile(path("resources/projectm/linux-x64/bin/projectMSDL"),
                  "#!/bin/sh\necho \"$*\" >> \"$MAGNETOFON_FAKE_LAUNCHES\"\n"
                  "trap 'exit 0' TERM INT\nwhile :; do cat \"$MAGNETOFON_FAKE_AUDIO\"; sleep 0.1; done\n", true);
#endif
    }

    void cleanup()
    {
        if (!m_temp) return;
        QDir::setCurrent(m_oldCwd);
        qputenv("PATH", m_oldPath);
        if (m_hadDataHome) qputenv("XDG_DATA_HOME", m_oldDataHome);
        else qunsetenv("XDG_DATA_HOME");
        qunsetenv("MAGNETOFON_FAKE_SINK");
        qunsetenv("MAGNETOFON_FAKE_AUDIO");
        qunsetenv("MAGNETOFON_FAKE_LAUNCHES");
        m_temp.reset();
    }

    void stopCancelsRestart_data()
    {
        QTest::addColumn<bool>("queuedStop");
        QTest::newRow("stop-during-notrunning-signal") << false;
        QTest::newRow("stop-before-queued-launch") << true;
    }

    void stopCancelsRestart()
    {
        QFETCH(bool, queuedStop);
        VisualizerLauncher launcher(nullptr, false);
        QVERIFY(launcher.runtimeAvailable());
        launcher.launchVisuals();
        QTRY_COMPARE_WITH_TIMEOUT(launches().size(), 1, 2000);
        bool stoppedDuringRestart = false;
        QObject::connect(&launcher, &VisualizerLauncher::isRunningChanged, this, [&]() {
            if (!launcher.isRunning() && !stoppedDuringRestart) {
                stoppedDuringRestart = true;
                launcher.stopVisuals();
            }
        }, queuedStop ? Qt::QueuedConnection : Qt::DirectConnection);
        launcher.setTargetFps(50);
        QTRY_VERIFY_WITH_TIMEOUT(stoppedDuringRestart, 2500);
        QTest::qWait(500);
        QVERIFY(!launcher.isRunning());
        QCOMPARE(launches().size(), 1);
    }

    void processRestrictedCaptureDoesNotQueryDefaultOutput()
    {
        writeFile(path("bin/pactl"), "#!/bin/sh\nexec sleep 30\n", true);
        VisualizerLauncher launcher(nullptr, false);
        QElapsedTimer elapsed; elapsed.start();
        launcher.launchVisuals();
        QVERIFY(elapsed.elapsed() < 200);
        QTRY_COMPARE_WITH_TIMEOUT(launches().size(), 1, 2000);
        QVERIFY(launches().first().contains("--magnetofonPid=" + QString::number(QCoreApplication::applicationPid())));
        QCOMPARE(launcher.audioSourceName(), QStringLiteral("Magnetofon playback only"));
        writeFile(path("sink"), "second-output\n");
        QTest::qWait(1100);
        QCOMPARE(launches().size(), 1); // Helper follows the app stream without a restart.
        QVERIFY(!launcher.audioLive());
        QVERIFY(launcher.statusMessage().contains("Rendering"));
        launcher.stopVisuals();
        QTRY_VERIFY_WITH_TIMEOUT(!launcher.isRunning(), 2000);
    }

    void freshSilentAndMissingCaptureAreDifferentFromRendering()
    {
        VisualizerLauncher launcher(nullptr, false);
        launcher.launchVisuals();
        QTRY_VERIFY_WITH_TIMEOUT(launcher.isRunning(), 2000);
        QVERIFY(!launcher.audioLive());
        writeFile(path("audio"), "MAGNETOFON_AUDIO live sequence=1 rms=0.125\n");
        QTRY_VERIFY_WITH_TIMEOUT(launcher.audioLive(), 1000);
        QVERIFY(launcher.audioStatus().contains("Magnetofon"));
        writeFile(path("audio"), "MAGNETOFON_AUDIO silent sequence=2 rms=0\n");
        QTRY_VERIFY_WITH_TIMEOUT(launcher.audioStatus().contains("silence"), 1000);
        QVERIFY(launcher.audioLive()); // Digital silence is fresh samples, not capture failure.
        writeFile(path("audio"), "MAGNETOFON_AUDIO unavailable\n");
        QTRY_VERIFY_WITH_TIMEOUT(!launcher.audioLive(), 1000);
        QVERIFY(launcher.isRunning());
        QVERIFY(launcher.audioStatus().contains("unavailable"));
        launcher.stopVisuals();
        QTRY_VERIFY_WITH_TIMEOUT(!launcher.isRunning(), 2000);
    }

    void lostStatusHeartbeatInvalidatesAudioReadiness()
    {
        writeFile(path("resources/projectm/linux-x64/bin/projectMSDL"),
            "#!/bin/sh\necho \"$*\" >> \"$MAGNETOFON_FAKE_LAUNCHES\"\n"
            "echo 'MAGNETOFON_AUDIO live sequence=1 rms=0.125'\n"
            "trap 'exit 0' TERM INT\nwhile :; do sleep 0.1; done\n", true);
        VisualizerLauncher launcher(nullptr, false);
        launcher.launchVisuals();
        QTRY_VERIFY_WITH_TIMEOUT(launcher.audioLive(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!launcher.audioLive(), 3500);
        QVERIFY(launcher.isRunning());
        launcher.stopVisuals();
    }

    void legacyPresetsRemainUsableAndIgnoreManagedStaging()
    {
        const QString userRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/visuals";
        writeFile(userRoot + "/custom.milk", "[preset00]\n");
        writeFile(userRoot + "-managed-packs/.pack-install-interrupted/presets/partial.milk", "[preset00]\n");
        writeFile(userRoot + "-managed-packs/pack-inactive/old.milk", "[preset00]\n");
        VisualizerLauncher launcher(nullptr, false);
        QCOMPARE(launcher.userPresetCount(), 1);
        QCOMPARE(launcher.fullPresetCount(), 0);
        QVERIFY(!launcher.fullLibraryAvailable());
        QVERIFY(launcher.userLibraryAvailable());
        launcher.setPresetSource("ALL");
        QCOMPARE(launcher.presetSource(), QStringLiteral("CURATED"));
        launcher.setPresetSource("USER");
        QCOMPARE(launcher.presetSource(), QStringLiteral("USER"));
        QCOMPARE(launcher.activePresetDirectory(), QFileInfo(userRoot).canonicalFilePath());
    }

    void userPresetsRemainSelectableWithVerifiedPack()
    {
        const QString userRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/visuals";
        const QString managed = userRoot + "-managed-packs";
        const QString active = managed + "/pack-active";
        const QString hash(64, 'a');
        writeFile(userRoot + "/custom.milk", "[preset00]\n");
        writeFile(active + "/presets/one.milk", "[preset00]\n");
        writeFile(active + "/presets/two.milk", "[preset00]\n");
        writeFile(managed + "/pack-inactive/old.milk", "[preset00]\n");
        writeFile(managed + "/.pack-install-interrupted/presets/partial.milk", "[preset00]\n");
        writeFile(active + "/completed.json", QJsonDocument(QJsonObject{
            {"format", 1}, {"presetCount", 2}, {"archiveSha256", hash}}).toJson());
        writeFile(managed + "/active-pack.json", QJsonDocument(QJsonObject{
            {"directory", "pack-active"}, {"archiveSha256", hash}}).toJson());
        VisualizerLauncher launcher(nullptr, false);
        QCOMPARE(launcher.fullPresetCount(), 2);
        QCOMPARE(launcher.userPresetCount(), 1);
        launcher.setPresetSource("ALL");
        QCOMPARE(launcher.activePresetDirectory(), active);
        launcher.setPresetSource("USER");
        QCOMPARE(launcher.presetSource(), QStringLiteral("USER"));
        QCOMPARE(launcher.activePresetDirectory(), QFileInfo(userRoot).canonicalFilePath());
        QVERIFY(launcher.buildArguments().contains("--presetPath=" + QFileInfo(userRoot).canonicalFilePath()));
    }


};

QTEST_GUILESS_MAIN(VisualizerLauncherTest)
#include "VisualizerLauncherTest.moc"
