#include "PresetPackDownloader.hpp"

#include <QtTest>
#include <QDataStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

namespace {
quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xffffffff;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320 & (0 - (crc & 1)));
    }
    return ~crc;
}

// Minimal, ordinary stored ZIP fixtures: no external archiver or private data.
QByteArray presetZip(int count = 2)
{
    QByteArray bytes, central;
    QDataStream out(&bytes, QIODevice::WriteOnly), directory(&central, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    directory.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < count; ++i) {
        const QByteArray name = "Presets/preset-" + QByteArray::number(i) + ".milk";
        const QByteArray content = "[preset00]\nfRating=3.000\n";
        const quint32 offset = bytes.size(), size = content.size(), crc = crc32(content);
        out << quint32(0x04034b50) << quint16(20) << quint16(0) << quint16(0)
            << quint16(0) << quint16(0) << crc << size << size
            << quint16(name.size()) << quint16(0);
        out.writeRawData(name.constData(), name.size());
        out.writeRawData(content.constData(), content.size());
        directory << quint32(0x02014b50) << quint16(20) << quint16(20) << quint16(0)
                  << quint16(0) << quint16(0) << quint16(0) << crc << size << size
                  << quint16(name.size()) << quint16(0) << quint16(0) << quint16(0)
                  << quint16(0) << quint32(0) << offset;
        directory.writeRawData(name.constData(), name.size());
    }
    const quint32 centralOffset = bytes.size();
    out.writeRawData(central.constData(), central.size());
    out << quint32(0x06054b50) << quint16(0) << quint16(0) << quint16(count)
        << quint16(count) << quint32(central.size()) << centralOffset << quint16(0);
    return bytes;
}

class HttpFixture : public QTcpServer {
public:
    QByteArray body;
    bool stalled = false;
    int requests = 0;
    HttpFixture()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    if (socket->property("responded").toBool()) return;
                    const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n")) return;
                    socket->setProperty("responded", true);
                    ++requests;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Length: " +
                                  QByteArray::number(stalled ? 100000 : body.size()) +
                                  "\r\nConnection: close\r\n\r\n");
                    if (!stalled) { socket->write(body); socket->disconnectFromHost(); }
                });
            }
        });
    }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/pack.zip").arg(serverPort())); }
};

class FailingWriter : public PresetPackDownloader {
public:
    using PresetPackDownloader::PresetPackDownloader;
    int failure = 0;
protected:
    qint64 writeDownload(const QByteArray &bytes) override {
        if (failure == 2) return PresetPackDownloader::writeDownload(bytes);
        if (failure == 1) return PresetPackDownloader::writeDownload(bytes.first(bytes.size() - 1));
        return -1;
    }
    bool flushDownload() override { return false; }
};

PresetPackDownloader::Options options(const QString &root, const QUrl &url)
{
    PresetPackDownloader::Options result;
    result.storageDirectory = root;
    result.downloadUrl = url;
    result.minimumPresetCount = 2;
    result.transferTimeoutMs = 200;
    result.operationTimeoutMs = 3000;
    result.extractionTimeoutMs = 1000;
    return result;
}
void writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) file.write(bytes);
}
}

class PresetPackDownloaderTest : public QObject {
    Q_OBJECT
private slots:
    void completePackActivatesAtomicallyAndKeepsCustomFiles()
    {
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = presetZip();
        const QString root = dir.filePath("visuals");
        writeFile(root + "/custom.milk", "custom");
        PresetPackDownloader downloader(options(root, server.url()));
        QVERIFY(!downloader.isInstalled());
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QVERIFY2(downloader.isInstalled(), qPrintable(downloader.statusMessage()));
        const QString active = PresetPackDownloader::installedPackDirectory(root);
        QVERIFY(!active.isEmpty());
        QVERIFY(QFileInfo::exists(active + "/Presets/preset-0.milk"));
        QCOMPARE(downloader.presetCount(), 2);
        QFile custom(root + "/custom.milk");
        QVERIFY(custom.open(QIODevice::ReadOnly));
        QCOMPARE(custom.readAll(), QByteArray("custom"));
        PresetPackDownloader reopened(options(root, server.url()));
        QVERIFY(reopened.isInstalled());
        QCOMPARE(reopened.presetCount(), 2);
    }

    void repeatedInstallationRetainsOnlyOneRollbackAndAllCustomFiles()
    {
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = presetZip();
        const QString root = dir.filePath("visuals");
        writeFile(root + "/custom.milk", "keep custom");
        PresetPackDownloader downloader(options(root, server.url()));
        QString previous;
        for (int iteration = 0; iteration < 4; ++iteration) {
            downloader.downloadPack();
            QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
            QVERIFY(downloader.isInstalled());
            const QString active = PresetPackDownloader::installedPackDirectory(root);
            QVERIFY(!active.isEmpty());
            if (!previous.isEmpty()) QVERIFY(QFileInfo::exists(previous));
            QTRY_COMPARE_WITH_TIMEOUT(QDir(root + "-managed-packs").entryList({"pack-*"},
                QDir::Dirs | QDir::NoDotAndDotDot).size(), std::min(iteration + 1, 2), 3000);
            previous = active;
        }
        QVERIFY(QFileInfo::exists(root + "/custom.milk"));
    }

    void cancelledAndTimedOutTransfersAllowRetry()
    {
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.stalled = true;
        const QString root = dir.filePath("visuals");
        PresetPackDownloader downloader(options(root, server.url()));
        downloader.downloadPack();
        QTRY_COMPARE(server.requests, 1);
        downloader.cancelDownload();
        QVERIFY(!downloader.isDownloading());
        QVERIFY(downloader.statusMessage().contains("cancelled"));
        downloader.downloadPack();
        QTRY_COMPARE(server.requests, 2);
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QVERIFY(downloader.statusMessage().contains("failed") || downloader.statusMessage().contains("timed out"));
        server.stalled = false;
        server.body = presetZip();
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QVERIFY2(downloader.isInstalled(), qPrintable(downloader.statusMessage()));
        QCOMPARE(QDir(root + "-managed-packs").entryList({".pack-install-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    }

    void shortWriteAndFlushFailureAreStorageErrors()
    {
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = presetZip();
        for (int failure : {0, 1, 2}) {
            FailingWriter downloader(options(dir.filePath(QString::number(failure)), server.url()));
            downloader.failure = failure;
            downloader.downloadPack();
            QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
            QVERIFY(!downloader.isInstalled());
            QVERIFY2(downloader.statusMessage().contains("storage"), qPrintable(downloader.statusMessage()));
            QVERIFY(PresetPackDownloader::installedPackDirectory(dir.path()).isEmpty());
        }
    }

    void failedReplacementKeepsPreviousVersionAndCustomPresets()
    {
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = presetZip();
        const QString root = dir.filePath("visuals");
        PresetPackDownloader downloader(options(root, server.url()));
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QVERIFY(downloader.isInstalled());
        const QString previous = PresetPackDownloader::installedPackDirectory(root);
        writeFile(root + "/custom.milk", "keep me");
        // Valid ZIP but insufficient preset count must not become active.
        server.body = presetZip(1);
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QCOMPARE(PresetPackDownloader::installedPackDirectory(root), previous);
        QVERIFY(downloader.isInstalled());
        QVERIFY(downloader.statusMessage().contains("incomplete"));
        // A truncated ordinary download must fail extraction without replacement.
        server.body = presetZip().left(50);
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QCOMPARE(PresetPackDownloader::installedPackDirectory(root), previous);
        QFile custom(root + "/custom.milk");
        QVERIFY(custom.open(QIODevice::ReadOnly));
        QCOMPARE(custom.readAll(), QByteArray("keep me"));
    }

    void interruptedStagingAndLegacyCountAreNotCertified()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath("visuals");
        writeFile(root + "-managed-packs/.pack-install-interrupted/content/a.milk", "pending");
        writeFile(root + "-managed-packs/pack-unactivated/a.milk", "not active");
        for (int i = 0; i < 1002; ++i) writeFile(root + QStringLiteral("/custom-%1.milk").arg(i), "legacy");
        PresetPackDownloader downloader(options(root, QUrl("http://127.0.0.1:1/unused")));
        QVERIFY(!downloader.isInstalled());
        QCOMPARE(downloader.presetCount(), 1002);
        QVERIFY(downloader.statusMessage().contains("unverified"));
    }

    void extractionTimeoutAndCancelDoNotActivate()
    {
#ifndef Q_OS_WIN
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = presetZip();
        const QString helper = dir.filePath("extractor");
        // Shell builtin loop: no child process survives killing this fake extractor.
        writeFile(helper, "#!/bin/sh\nwhile :; do :; done\n");
        QVERIFY(QFile::setPermissions(helper, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        auto config = options(dir.filePath("visuals"), server.url());
        config.extractorProgram = helper;
        config.extractionTimeoutMs = 150;
        PresetPackDownloader downloader(config);
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QVERIFY(downloader.statusMessage().contains("extraction timed out"));
        QVERIFY(PresetPackDownloader::installedPackDirectory(config.storageDirectory).isEmpty());
        downloader.downloadPack();
        QTRY_VERIFY(downloader.statusMessage().contains("Extracting"));
        downloader.cancelDownload();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        QVERIFY(downloader.statusMessage().contains("cancelled"));
#endif
    }

    void missingPresetInvalidatesCompletionMarker()
    {
        QTemporaryDir dir;
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = presetZip();
        const QString root = dir.filePath("visuals");
        writeFile(root + "/pack-custom/my-preset.milk", "keep custom");
        PresetPackDownloader downloader(options(root, server.url()));
        downloader.downloadPack();
        QTRY_VERIFY_WITH_TIMEOUT(!downloader.isDownloading(), 3000);
        const QString active = PresetPackDownloader::installedPackDirectory(root);
        QVERIFY(!active.isEmpty());
        QVERIFY(QFile::remove(active + "/Presets/preset-1.milk"));
        QVERIFY(PresetPackDownloader::installedPackDirectory(root).isEmpty());
        downloader.checkInstallationStatus();
        QVERIFY(!downloader.isInstalled());
        QCOMPARE(downloader.presetCount(), 1);
        QVERIFY(QFileInfo::exists(root + "/pack-custom/my-preset.milk"));
    }
};

QTEST_GUILESS_MAIN(PresetPackDownloaderTest)
#include "PresetPackDownloaderTest.moc"
