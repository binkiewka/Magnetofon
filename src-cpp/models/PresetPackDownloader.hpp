#ifndef PRESET_PACK_DOWNLOADER_HPP
#define PRESET_PACK_DOWNLOADER_HPP

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>

class PresetPackDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDownloading READ isDownloading NOTIFY isDownloadingChanged)
    Q_PROPERTY(bool isInstalled READ isInstalled NOTIFY isInstalledChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int presetCount READ presetCount NOTIFY presetCountChanged)

public:
    explicit PresetPackDownloader(QObject *parent = nullptr);
    ~PresetPackDownloader() override = default;

    bool isDownloading() const { return m_isDownloading; }
    bool isInstalled() const { return m_isInstalled; }
    double progress() const { return m_progress; }
    QString statusMessage() const { return m_statusMessage; }
    int presetCount() const { return m_presetCount; }

    Q_INVOKABLE void downloadPack();
    Q_INVOKABLE void checkInstallationStatus();

signals:
    void isDownloadingChanged();
    void isInstalledChanged();
    void progressChanged();
    void statusMessageChanged();
    void presetCountChanged();

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void extractZip(const QString &zipPath, const QString &targetDir);
    QString targetDirectory() const;

    QNetworkAccessManager m_networkManager;
    QNetworkReply *m_reply = nullptr;
    QFile m_outputFile;

    bool m_isDownloading = false;
    bool m_isInstalled = false;
    double m_progress = 0.0;
    QString m_statusMessage;
    int m_presetCount = 0;
};

#endif // PRESET_PACK_DOWNLOADER_HPP
