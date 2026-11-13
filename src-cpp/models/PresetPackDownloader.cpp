#include "PresetPackDownloader.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QRunnable>
#include <QThreadPool>

namespace {
constexpr auto activeFile = "active-pack.json";
constexpr auto completedFile = "completed.json";

QString managedDirectory(const QString &visualsRoot)
{
    return visualsRoot.isEmpty() ? QString() : QDir::cleanPath(visualsRoot) + QStringLiteral("-managed-packs");
}

int countPresetsRecursively(const QString &directory)
{
    if (directory.isEmpty() || !QDir(directory).exists()) return 0;
    int count = 0;
    // Managed versions live outside the legacy tree, preserving every custom folder.
    const QFileInfoList entries = QDir(directory).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            count += countPresetsRecursively(entry.absoluteFilePath());
        } else if (entry.suffix().compare(QStringLiteral("milk"), Qt::CaseInsensitive) == 0 ||
                   entry.suffix().compare(QStringLiteral("prjm"), Qt::CaseInsensitive) == 0) {
            ++count;
        }
    }
    return count;
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeObject(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size()) return false;
    return file.commit();
}
} // namespace

PresetPackDownloader::PresetPackDownloader(QObject *parent)
    : PresetPackDownloader(Options{}, parent)
{
}

PresetPackDownloader::PresetPackDownloader(const Options &options, QObject *parent)
    : QObject(parent), m_options(options)
{
    if (m_options.downloadUrl.isEmpty()) {
        m_options.downloadUrl = QUrl(QStringLiteral("https://github.com/binkiewka/Magnetofon/releases/download/v1.0.0/Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip"));
    }
    m_options.transferTimeoutMs = qMax(1, m_options.transferTimeoutMs);
    m_options.operationTimeoutMs = qMax(1, m_options.operationTimeoutMs);
    m_options.extractionTimeoutMs = qMax(1, m_options.extractionTimeoutMs);
    m_options.minimumPresetCount = qMax(1, m_options.minimumPresetCount);
    m_operationTimer.setSingleShot(true);
    m_extractionTimer.setSingleShot(true);
    connect(&m_operationTimer, &QTimer::timeout, this, [this] {
        stopOperation(QStringLiteral("Preset installation timed out. Retry when your connection is stable."));
    });
    connect(&m_extractionTimer, &QTimer::timeout, this, [this] {
        stopOperation(QStringLiteral("Preset extraction timed out. The previous library was kept."));
    });
    checkInstallationStatus();
}

PresetPackDownloader::~PresetPackDownloader()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        delete m_reply;
    }
    if (m_extractProcess && m_extractProcess->state() != QProcess::NotRunning) {
        m_extractProcess->disconnect(this);
        // The owned extractor only writes to our staging directory. Kill before
        // removing that directory; it never touches the active/custom library.
        m_extractProcess->kill();
        m_extractProcess->waitForFinished(1000);
    }
    m_outputFile.close();
}

QString PresetPackDownloader::targetDirectory() const
{
    QString directory = m_options.storageDirectory;
    if (directory.isEmpty()) {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (appData.isEmpty()) return {};
        directory = QDir(appData).filePath(QStringLiteral("visuals"));
    }
    if (!QDir().mkpath(directory)) return {};
    const QFileInfo info(directory);
    return info.isDir() && info.isWritable() ? info.absoluteFilePath() : QString();
}

QString PresetPackDownloader::installedPackDirectory(const QString &visualsRoot)
{
    if (visualsRoot.isEmpty()) return {};
    const QString managed = managedDirectory(visualsRoot);
    if (QFileInfo(managed).isSymLink()) return {};
    const QJsonObject active = readObject(QDir(managed).filePath(QString::fromLatin1(activeFile)));
    const QString version = active.value(QStringLiteral("directory")).toString();
    // A basename only; the manifest cannot accidentally select a different tree.
    if (!version.startsWith(QStringLiteral("pack-")) || version.contains('/') ||
        version.contains('\\') || version.contains(QStringLiteral(".."))) return {};
    const QString directory = QDir(managed).filePath(version);
    if (QFileInfo(directory).isSymLink()) return {};
    const QJsonObject completed = readObject(QDir(directory).filePath(QString::fromLatin1(completedFile)));
    const int count = completed.value(QStringLiteral("presetCount")).toInt();
    const QString archiveHash = completed.value(QStringLiteral("archiveSha256")).toString();
    if (completed.value(QStringLiteral("format")).toInt() != 1 || count < 1 ||
        archiveHash.size() != 64 || archiveHash != active.value(QStringLiteral("archiveSha256")).toString() ||
        count != countPresetsRecursively(directory)) return {};
    return directory;
}

void PresetPackDownloader::checkInstallationStatus()
{
    const QString root = targetDirectory();
    const QString installed = installedPackDirectory(root);
    m_isInstalled = !installed.isEmpty();
    m_presetCount = m_isInstalled ? countPresetsRecursively(installed)
        : countPresetsRecursively(root) + countPresetsRecursively(
              QDir::current().absoluteFilePath(QStringLiteral("visuals/presets")));
    m_statusMessage = m_isInstalled
        ? QStringLiteral("%1 MilkDrop presets installed and verified").arg(m_presetCount)
        : m_presetCount > 0
            ? QStringLiteral("%1 legacy/custom presets available; full-pack installation is unverified").arg(m_presetCount)
            : QStringLiteral("Full preset pack is not installed");
    emit isInstalledChanged();
    emit presetCountChanged();
    emit statusMessageChanged();
}

void PresetPackDownloader::downloadPack()
{
    if (m_isDownloading) return;
    const QString root = targetDirectory();
    if (root.isEmpty()) {
        finishOperation(QStringLiteral("Cannot create a writable preset folder."));
        return;
    }
    const QString managed = managedDirectory(root);
    if (!QDir().mkpath(managed)) {
        finishOperation(QStringLiteral("Cannot create managed preset installation folder."));
        return;
    }
    m_staging = std::make_unique<QTemporaryDir>(QDir(managed).filePath(QStringLiteral(".pack-install-XXXXXX")));
    if (!m_staging->isValid()) {
        finishOperation(QStringLiteral("Cannot create preset installation staging folder."));
        return;
    }
    m_outputFile.setFileName(m_staging->filePath(QStringLiteral("download.zip")));
    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finishOperation(QStringLiteral("Cannot create preset download: %1").arg(m_outputFile.errorString()));
        return;
    }
    m_isDownloading = true;
    m_progress = 0.0;
    m_downloadedBytes = 0;
    m_archiveHash.reset();
    m_pendingFailure.clear();
    m_statusMessage = QStringLiteral("Connecting to preset server…");
    emit isDownloadingChanged();
    emit progressChanged();
    emit statusMessageChanged();
    m_operationTimer.start(m_options.operationTimeoutMs);

    QNetworkRequest request(m_options.downloadUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(m_options.transferTimeoutMs);
    m_reply = m_networkManager.get(request);
    m_reply->setReadBufferSize(512 * 1024);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { consumeDownload(); });
    connect(m_reply, &QNetworkReply::finished, this, &PresetPackDownloader::onDownloadFinished);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (!m_pendingFailure.isEmpty()) return;
        if (total > m_options.maximumDownloadBytes) {
            stopOperation(QStringLiteral("Preset archive exceeds the download size limit."));
            return;
        }
        if (total > 0) m_progress = qMin(0.85, static_cast<double>(received) / total * 0.85);
        m_statusMessage = QStringLiteral("Downloading preset pack… %1 MB%2")
            .arg(received / (1024.0 * 1024.0), 0, 'f', 1)
            .arg(total > 0 ? QStringLiteral(" / %1 MB").arg(total / (1024.0 * 1024.0), 0, 'f', 1) : QString());
        emit progressChanged();
        emit statusMessageChanged();
    });
}

qint64 PresetPackDownloader::writeDownload(const QByteArray &bytes)
{
    return m_outputFile.write(bytes);
}

bool PresetPackDownloader::flushDownload()
{
    return m_outputFile.flush();
}

bool PresetPackDownloader::consumeDownload()
{
    if (!m_reply || !m_outputFile.isOpen() || !m_pendingFailure.isEmpty()) return false;
    const QByteArray bytes = m_reply->readAll();
    if (bytes.isEmpty()) return true;
    if (bytes.size() > m_options.maximumDownloadBytes - m_downloadedBytes) {
        stopOperation(QStringLiteral("Preset archive exceeds the download size limit."));
        return false;
    }
    if (writeDownload(bytes) != bytes.size()) {
        stopOperation(QStringLiteral("Cannot write preset download to storage: %1").arg(m_outputFile.errorString()));
        return false;
    }
    m_archiveHash.addData(bytes);
    m_downloadedBytes += bytes.size();
    return true;
}

void PresetPackDownloader::onDownloadFinished()
{
    if (!m_reply) return;
    if (!m_pendingFailure.isEmpty()) {
        finishOperation(m_pendingFailure);
        return;
    }
    if (m_reply->error() != QNetworkReply::NoError) {
        finishOperation(QStringLiteral("Preset download failed: %1").arg(m_reply->errorString()));
        return;
    }
    if (!consumeDownload()) return;
    if (!flushDownload()) {
        finishOperation(QStringLiteral("Cannot flush preset download to storage: %1").arg(m_outputFile.errorString()));
        return;
    }
    m_outputFile.close();
    m_reply->disconnect(this);
    m_reply->deleteLater();
    m_reply = nullptr;
    QFile archive(m_outputFile.fileName());
    if (!archive.open(QIODevice::ReadOnly) || archive.read(4) != QByteArrayLiteral("PK\x03\x04")) {
        finishOperation(QStringLiteral("The downloaded preset archive is invalid."));
        return;
    }
    archive.close();
    extractZip();
}

void PresetPackDownloader::extractZip()
{
    const QString destination = m_staging->filePath(QStringLiteral("content"));
    if (!QDir().mkpath(destination)) {
        finishOperation(QStringLiteral("Cannot create preset extraction folder."));
        return;
    }
    m_progress = 0.9;
    m_statusMessage = QStringLiteral("Extracting and checking preset pack…");
    emit progressChanged();
    emit statusMessageChanged();
    m_extractProcess = new QProcess(this);
    m_extractProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(m_extractProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_extractionTimer.stop();
        if (!m_pendingFailure.isEmpty()) finishOperation(m_pendingFailure);
        else if (status != QProcess::NormalExit || code != 0) {
            finishOperation(QStringLiteral("Preset extraction failed; the previous library was kept."));
        } else activateStagedPack();
    });
    connect(m_extractProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishOperation(QStringLiteral("Cannot start preset extractor. Install %1 and retry.")
#if defined(Q_OS_WIN)
                .arg(QStringLiteral("PowerShell")));
#else
                .arg(QStringLiteral("unzip")));
#endif
        }
    });
    m_extractionTimer.start(m_options.extractionTimeoutMs);
#if defined(Q_OS_WIN)
    QString zip = m_outputFile.fileName(); zip.replace('\'', QStringLiteral("''"));
    QString directory = destination; directory.replace('\'', QStringLiteral("''"));
    const QString command = QStringLiteral("$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force").arg(zip, directory);
    m_extractProcess->start(m_options.extractorProgram.isEmpty() ? QStringLiteral("powershell") : m_options.extractorProgram,
                           {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), command});
#else
    m_extractProcess->start(m_options.extractorProgram.isEmpty() ? QStringLiteral("unzip") : m_options.extractorProgram,
                           {QStringLiteral("-q"), m_outputFile.fileName(), QStringLiteral("-d"), destination});
#endif
}

void PresetPackDownloader::activateStagedPack()
{
    const QString content = m_staging->filePath(QStringLiteral("content"));
    const int count = countPresetsRecursively(content);
    if (count < m_options.minimumPresetCount) {
        finishOperation(QStringLiteral("Preset pack is incomplete (%1 presets); the previous library was kept.").arg(count));
        return;
    }
    const QString hash = QString::fromLatin1(m_archiveHash.result().toHex());
    if (!writeObject(QDir(content).filePath(QString::fromLatin1(completedFile)),
                     {{QStringLiteral("format"), 1}, {QStringLiteral("presetCount"), count},
                      {QStringLiteral("archiveSha256"), hash}, {QStringLiteral("archiveBytes"), m_downloadedBytes}})) {
        finishOperation(QStringLiteral("Cannot save preset completion manifest; the previous library was kept."));
        return;
    }
    const QString version = QStringLiteral("pack-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString root = managedDirectory(targetDirectory());
    if (root.isEmpty() || !QDir().rename(content, QDir(root).filePath(version))) {
        finishOperation(QStringLiteral("Cannot finalize preset pack; the previous library was kept."));
        return;
    }
    const QString previousVersion = QFileInfo(installedPackDirectory(targetDirectory())).fileName();
    // Only this tiny pointer is replaced. One previous version and all custom
    // presets remain; interrupted activation never overwrites the active pack.
    if (!writeObject(QDir(root).filePath(QString::fromLatin1(activeFile)),
                     {{QStringLiteral("directory"), version}, {QStringLiteral("archiveSha256"), hash},
                      {QStringLiteral("previousDirectory"), previousVersion}})) {
        finishOperation(QStringLiteral("Cannot activate preset pack; the previous library was kept."));
        return;
    }
    // Retain current + one rollback only. Prune completed managed versions on
    // a worker; thousands of unlink calls must not block the GUI. Loose/custom,
    // incomplete staging and symlinks are never cleanup candidates.
    QStringList obsolete;
    for (const QFileInfo &entry : QDir(root).entryInfoList({QStringLiteral("pack-*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.fileName() != version && entry.fileName() != previousVersion && !entry.isSymLink()
            && QFileInfo::exists(QDir(entry.absoluteFilePath()).filePath(QString::fromLatin1(completedFile))))
            obsolete.append(entry.absoluteFilePath());
    }
    if (!obsolete.isEmpty()) QThreadPool::globalInstance()->start(QRunnable::create([obsolete] {
        for (const QString &directory : obsolete) QDir(directory).removeRecursively();
    }));
    checkInstallationStatus();
    m_progress = 1.0;
    emit progressChanged();
    finishOperation(QStringLiteral("%1 MilkDrop presets installed and verified").arg(count));
}

void PresetPackDownloader::cancelDownload()
{
    if (m_isDownloading) stopOperation(QStringLiteral("Preset installation cancelled; the previous library was kept."));
}

void PresetPackDownloader::stopOperation(const QString &message)
{
    if (!m_isDownloading || !m_pendingFailure.isEmpty()) return;
    m_pendingFailure = message;
    if (m_reply) {
        // abort emits finished, which centralizes cleanup. Disconnect readyRead
        // first so abort cannot attempt another storage write.
        m_reply->disconnect(this);
        m_reply->abort();
        finishOperation(message);
    } else if (m_extractProcess && m_extractProcess->state() != QProcess::NotRunning) {
        m_extractProcess->kill(); // finished will clean staging after the child exits
    } else finishOperation(message);
}

void PresetPackDownloader::finishOperation(QString message)
{
    m_operationTimer.stop();
    m_extractionTimer.stop();
    if (m_reply) {
        m_reply->disconnect(this);
        if (!m_reply->isFinished()) m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_extractProcess) {
        m_extractProcess->disconnect(this);
        m_extractProcess->deleteLater();
        m_extractProcess = nullptr;
    }
    m_outputFile.close();
    m_staging.reset();
    m_pendingFailure.clear();
    const bool wasDownloading = m_isDownloading;
    m_isDownloading = false;
    m_statusMessage = message;
    if (wasDownloading) emit isDownloadingChanged();
    emit statusMessageChanged();
}
