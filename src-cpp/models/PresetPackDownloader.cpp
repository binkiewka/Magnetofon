#include "PresetPackDownloader.hpp"
#include <QDebug>
#include <QDirIterator>
#include <QFileInfo>
#include <QUrl>

namespace {

int countPresetsRecursively(const QString &directory)
{
    if (directory.isEmpty() || !QDir(directory).exists()) return 0;

    QDirIterator iterator(directory,
                          {QStringLiteral("*.milk"), QStringLiteral("*.prjm")},
                          QDir::Files,
                          QDirIterator::Subdirectories);
    int count = 0;
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

} // namespace

PresetPackDownloader::PresetPackDownloader(QObject *parent)
    : QObject(parent)
{
    checkInstallationStatus();
}

QString PresetPackDownloader::targetDirectory() const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) return {};

    const QString directory = QDir(appData).filePath(QStringLiteral("visuals"));
    // AppDataLocation itself normally does not exist on a fresh install. mkdir()
    // cannot create its missing parents, so packaged builds failed before the
    // network request even started. mkpath() creates the complete user-owned path.
    if (!QDir().mkpath(directory)) {
        qWarning() << "[PresetPackDownloader] Failed to create preset directory:" << directory;
        return {};
    }

    const QFileInfo info(directory);
    if (!info.isDir() || !info.isWritable()) {
        qWarning() << "[PresetPackDownloader] Preset directory is not writable:" << directory;
        return {};
    }
    return QDir::cleanPath(directory);
}

void PresetPackDownloader::checkInstallationStatus()
{
    const QString targetDir = targetDirectory();
    const QString localFullLibrary = QDir::current().absoluteFilePath(QStringLiteral("visuals/presets"));
    const int milkCount = countPresetsRecursively(targetDir) + countPresetsRecursively(localFullLibrary);

    m_presetCount = milkCount;
    m_isInstalled = milkCount > 1000;

    if (m_isInstalled) {
        m_statusMessage = QString("%1 MilkDrop presets installed and ready").arg(m_presetCount);
    } else {
        m_statusMessage = milkCount > 0
            ? QString("Only %1 presets found; the full pack is incomplete").arg(milkCount)
            : QStringLiteral("Full preset pack is not installed");
    }

    emit isInstalledChanged();
    emit presetCountChanged();
    emit statusMessageChanged();
}

void PresetPackDownloader::downloadPack()
{
    if (m_isDownloading) return;

    const QString targetDir = targetDirectory();
    if (targetDir.isEmpty()) {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_statusMessage = QStringLiteral("Failed to create writable preset folder: %1")
                              .arg(QDir(appData).filePath(QStringLiteral("visuals")));
        emit statusMessageChanged();
        return;
    }
    const QString zipPath = targetDir + "/Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip";

    m_outputFile.setFileName(zipPath);
    if (!m_outputFile.open(QIODevice::WriteOnly)) {
        m_statusMessage = QStringLiteral("Failed to create preset download: %1")
                              .arg(m_outputFile.errorString());
        emit statusMessageChanged();
        return;
    }

    m_isDownloading = true;
    m_progress = 0.0;
    m_statusMessage = "Connecting to GitHub release server...";
    emit isDownloadingChanged();
    emit progressChanged();
    emit statusMessageChanged();

    const QUrl url("https://github.com/binkiewka/Magnetofon/releases/download/v1.0.0/Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_networkManager.get(request);

    connect(m_reply, &QNetworkReply::downloadProgress, this, &PresetPackDownloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &PresetPackDownloader::onDownloadFinished);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_reply && m_outputFile.isOpen()) {
            m_outputFile.write(m_reply->readAll());
        }
    });
}

void PresetPackDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        m_progress = (static_cast<double>(bytesReceived) / static_cast<double>(bytesTotal)) * 0.85;
        const double mbReceived = bytesReceived / (1024.0 * 1024.0);
        const double mbTotal = bytesTotal / (1024.0 * 1024.0);
        m_statusMessage = QString("Downloading preset pack... %1 MB / %2 MB (%3%)")
                              .arg(mbReceived, 0, 'f', 1)
                              .arg(mbTotal, 0, 'f', 1)
                              .arg(static_cast<int>(m_progress * 100));
        emit progressChanged();
        emit statusMessageChanged();
    }
}

void PresetPackDownloader::onDownloadFinished()
{
    if (!m_reply) return;

    if (m_outputFile.isOpen()) {
        m_outputFile.write(m_reply->readAll());
        m_outputFile.close();
    }

    if (m_reply->error() != QNetworkReply::NoError) {
        m_statusMessage = QString("Download error: %1").arg(m_reply->errorString());
        m_isDownloading = false;
        emit isDownloadingChanged();
        emit statusMessageChanged();
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    const QString zipPath = m_outputFile.fileName();
    const QString targetDir = targetDirectory();

    m_statusMessage = "Extracting 9,000+ MilkDrop presets...";
    m_progress = 0.90;
    emit statusMessageChanged();
    emit progressChanged();

    extractZip(zipPath, targetDir);
}

void PresetPackDownloader::extractZip(const QString &zipPath, const QString &targetDir)
{
    QProcess process;
#if defined(Q_OS_WIN)
    const QString cmd = QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipPath, targetDir);
    process.start("powershell", QStringList() << "-Command" << cmd);
#else
    process.start("unzip", QStringList() << "-o" << zipPath << "-d" << targetDir);
#endif

    if (process.waitForFinished(120000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        m_progress = 1.0;
        m_isDownloading = false;
        checkInstallationStatus();
        if (m_isInstalled) {
            m_statusMessage = QString("%1 MilkDrop presets installed and ready").arg(m_presetCount);
        } else {
            m_statusMessage = "Extraction finished, but the full preset library was not found";
        }
        emit isDownloadingChanged();
        emit progressChanged();
        emit statusMessageChanged();
    } else {
        m_isDownloading = false;
        m_statusMessage = "Preset extraction failed. Check that the unzip utility is installed.";
        emit isDownloadingChanged();
        emit statusMessageChanged();
    }
}
