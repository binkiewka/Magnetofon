#include "PresetPackDownloader.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QUrl>

PresetPackDownloader::PresetPackDownloader(QObject *parent)
    : QObject(parent)
{
    checkInstallationStatus();
}

QString PresetPackDownloader::targetDirectory() const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appData);
    if (!dir.exists("visuals")) {
        dir.mkdir("visuals");
    }
    return appData + "/visuals";
}

void PresetPackDownloader::checkInstallationStatus()
{
    const QString targetDir = targetDirectory();
    QDir dir(targetDir);

    // Also check local visuals/ folder
    QDir localDir("visuals");
    int milkCount = 0;

    if (dir.exists()) {
        const QStringList files = dir.entryList(QStringList() << "*.milk", QDir::Files, QDir::Unsorted);
        milkCount += files.size();
    }
    if (localDir.exists()) {
        const QStringList files = localDir.entryList(QStringList() << "*.milk", QDir::Files, QDir::Unsorted);
        milkCount += files.size();
    }

    m_presetCount = milkCount;
    m_isInstalled = (milkCount > 100);

    if (m_isInstalled) {
        m_statusMessage = QString("%1 MilkDrop presets installed & ready").arg(m_presetCount);
    } else {
        m_statusMessage = "Preset pack not installed. Click below to download 9,000+ presets pack.";
    }

    emit isInstalledChanged();
    emit presetCountChanged();
    emit statusMessageChanged();
}

void PresetPackDownloader::downloadPack()
{
    if (m_isDownloading) return;

    const QString targetDir = targetDirectory();
    const QString zipPath = targetDir + "/Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip";

    m_outputFile.setFileName(zipPath);
    if (!m_outputFile.open(QIODevice::WriteOnly)) {
        m_statusMessage = "Failed to create output file for preset pack download.";
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

    if (process.waitForFinished(120000)) {
        m_progress = 1.0;
        m_isDownloading = false;
        checkInstallationStatus();
        m_statusMessage = "9,000+ MilkDrop Presets successfully installed & active!";
        emit isDownloadingChanged();
        emit progressChanged();
        emit statusMessageChanged();
    } else {
        m_isDownloading = false;
        m_statusMessage = "Extraction timed out or failed. Please check system zip utility.";
        emit isDownloadingChanged();
        emit statusMessageChanged();
    }
}
