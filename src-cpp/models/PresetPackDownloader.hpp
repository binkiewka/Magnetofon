#ifndef PRESET_PACK_DOWNLOADER_HPP
#define PRESET_PACK_DOWNLOADER_HPP

#include <QObject>
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <memory>

class QTemporaryDir;

class PresetPackDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDownloading READ isDownloading NOTIFY isDownloadingChanged)
    Q_PROPERTY(bool isInstalled READ isInstalled NOTIFY isInstalledChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int presetCount READ presetCount NOTIFY presetCountChanged)

public:
    // Dependency configuration is also used by isolated local HTTP/storage tests.
    struct Options {
        QUrl downloadUrl;
        QString storageDirectory;
        QString extractorProgram;
        int transferTimeoutMs = 30000;
        int operationTimeoutMs = 15 * 60 * 1000;
        int extractionTimeoutMs = 120000;
        int minimumPresetCount = 1001;
        qint64 maximumDownloadBytes = 512 * 1024 * 1024;
    };

    explicit PresetPackDownloader(QObject *parent = nullptr);
    explicit PresetPackDownloader(const Options &options, QObject *parent = nullptr);
    ~PresetPackDownloader() override;

    bool isDownloading() const { return m_isDownloading; }
    bool isInstalled() const { return m_isInstalled; }
    double progress() const { return m_progress; }
    QString statusMessage() const { return m_statusMessage; }
    int presetCount() const { return m_presetCount; }

    // Argument is the legacy/custom visuals root; managed versions are stored
    // in its sibling <visualsRoot>-managed-packs, outside projectM's recursive scan.
    // Only returns an activated, completely extracted pack. Never a staging folder.
    static QString installedPackDirectory(const QString &visualsRoot);

    Q_INVOKABLE void downloadPack();
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void checkInstallationStatus();

signals:
    void isDownloadingChanged();
    void isInstalledChanged();
    void progressChanged();
    void statusMessageChanged();
    void presetCountChanged();

protected:
    // Small I/O boundary permits deterministic storage-failure regression tests.
    virtual qint64 writeDownload(const QByteArray &bytes);
    virtual bool flushDownload();

private:
    void extractZip();
    bool consumeDownload();
    void onDownloadFinished();
    void activateStagedPack();
    void finishOperation(QString message);
    void stopOperation(const QString &message);
    QString targetDirectory() const;

    Options m_options;
    QNetworkAccessManager m_networkManager;
    QNetworkReply *m_reply = nullptr;
    QProcess *m_extractProcess = nullptr;
    QFile m_outputFile;
    QTimer m_operationTimer;
    QTimer m_extractionTimer;
    std::unique_ptr<QTemporaryDir> m_staging;
    QCryptographicHash m_archiveHash{QCryptographicHash::Sha256};
    qint64 m_downloadedBytes = 0;
    QString m_pendingFailure;

    bool m_isDownloading = false;
    bool m_isInstalled = false;
    double m_progress = 0.0;
    QString m_statusMessage;
    int m_presetCount = 0;
};

#endif // PRESET_PACK_DOWNLOADER_HPP
