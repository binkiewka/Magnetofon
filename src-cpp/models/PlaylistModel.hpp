#ifndef PLAYLIST_MODEL_HPP
#define PLAYLIST_MODEL_HPP

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>
#include <QQueue>
#include <QSortFilterProxyModel>
#include <atomic>
#include <functional>
#include <memory>

struct PlaylistImportState;

struct TrackItem {
    QString id;
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString year;
    QString trackNumber;
    QString comment;
    QString codec;
    QString container;
    QString formatLabel;
    QString artworkUrl;
    QString videoCodec;
    double duration = 0.0;
    double frameRate = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitDepth = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    qint64 bitrate = 0;
    bool hasVideo = false;
};

class PlaylistModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QAbstractItemModel* filteredModel READ filteredModel CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QVariantMap currentTrack READ currentTrack NOTIFY currentIndexChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY importChanged)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY importChanged)
    Q_PROPERTY(int importedCount READ importedCount NOTIFY importChanged)
    Q_PROPERTY(QString operationError READ operationError NOTIFY operationErrorChanged)

public:
    enum TrackRoles {
        IdRole = Qt::UserRole + 1,
        FilePathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        DurationRole,
        AlbumArtistRole,
        GenreRole,
        YearRole,
        TrackNumberRole,
        CommentRole,
        CodecRole,
        ContainerRole,
        FormatLabelRole,
        ArtworkUrlRole,
        SampleRateRole,
        ChannelsRole,
        BitDepthRole,
        BitrateRole,
        HasVideoRole,
        VideoCodecRole,
        VideoWidthRole,
        VideoHeightRole,
        FrameRateRole,
        SourceIndexRole
    };

    explicit PlaylistModel(QObject *parent = nullptr);
    ~PlaylistModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::UserRole + 1) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);
    QAbstractItemModel *filteredModel() const { return m_filter.get(); }
    double restoredPosition() const { return m_restoredPosition; }
    int count() const { return m_tracks.size(); }
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    QVariantMap currentTrack() const { return getTrack(m_currentIndex); }
    bool importing() const { return m_importing; }
    QString importStatus() const { return m_importStatus; }
    int importedCount() const { return m_importedCount; }
    QString operationError() const { return m_operationError; }

public slots:
    void activateTrack(int index);
    int visibleIndexForTrack(int index) const;
    int trackIndexForVisible(int index) const;
    bool saveSession(double position, const QString &path = {});
    bool restoreSession(const QString &path = {});
    bool checkpointSessionPosition(double position, const QString &path = {});
    void openFileDialog();
    void openFolderDialog();
    void loadPlaylistDialog();
    void savePlaylistDialog();
    void addFile(const QString &filePath);
    void addFolder(const QString &directoryPath);
    void addFiles(const QList<QUrl> &urls);
    // UI/IPC entrypoints: filesystem traversal and metadata never run on the UI thread.
    void importFiles(const QList<QUrl> &urls);
    void importFolder(const QString &directoryPath);
    void importPlaylist(const QString &playlistPath);
    void cancelImport();
    bool loadPlaylist(const QString &playlistPath);
    bool savePlaylist(const QString &playlistPath);
    void removeTrack(int index);
    void clear();
    void moveTrack(int from, int to);

    QVariantMap getTrack(int index) const;
    QString currentFilePath() const;
    void followPlayback(const QString &filePath);
    void nextTrack();
    void previousTrack();

signals:
    void searchTextChanged();
    void countChanged();
    void currentIndexChanged();
    void trackSelected(const QString &filePath);
    void folderDiscovered(const QString &directoryPath, int addedCount);
    void emptied();
    void importChanged();
    void operationErrorChanged();
    void operationFailed(const QString &operation, const QString &error);
    void importFinished(int addedCount, bool cancelled);

private:
    static bool isSupportedMediaFile(const QFileInfo &file, bool explicitSelection = false,
                                     const std::function<bool()> &shouldCancel = {});
    static TrackItem buildTrackItem(const QFileInfo &fi,
                                   const std::function<bool()> &shouldCancel = {});
    int addFileList(const QStringList &filePaths);
    int appendTrackItems(QVector<TrackItem> items);
    void resetTracks();
    void beginImport(const QList<QUrl> &urls, const QString &playlistPath);
    void drainImport();
    void setOperationError(const QString &operation, const QString &error);

    std::unique_ptr<QSortFilterProxyModel> m_filter;
    QString m_searchText;
    QString m_sessionId;
    double m_restoredPosition = 0;
    QVector<TrackItem> m_tracks;
    QSet<QString> m_trackPaths;
    int m_currentIndex = -1;
    QTimer m_importTimer;
    std::shared_ptr<PlaylistImportState> m_importState;
    QQueue<QList<QUrl>> m_pendingImports;
    bool m_importing = false;
    bool m_replacePending = false;
    int m_importedCount = 0;
    QString m_importStatus;
    QString m_operationError;
};

#endif // PLAYLIST_MODEL_HPP
