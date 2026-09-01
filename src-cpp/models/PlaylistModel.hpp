#ifndef PLAYLIST_MODEL_HPP
#define PLAYLIST_MODEL_HPP

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QUrl>
#include <QFileInfo>

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
    double duration = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitDepth = 0;
    qint64 bitrate = 0;
};

class PlaylistModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

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
        BitrateRole
    };

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::UserRole + 1) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_tracks.size(); }
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

public slots:
    void openFileDialog();
    void addFile(const QString &filePath);
    void addFiles(const QList<QUrl> &urls);
    void removeTrack(int index);
    void clear();
    void moveTrack(int from, int to);

    QVariantMap getTrack(int index) const;
    QString currentFilePath() const;
    void nextTrack();
    void previousTrack();

signals:
    void countChanged();
    void currentIndexChanged();
    void trackSelected(const QString &filePath);
    void emptied();

private:
    static bool isSupportedAudioFile(const QFileInfo &file);
    QVector<TrackItem> m_tracks;
    int m_currentIndex = -1;
};

#endif // PLAYLIST_MODEL_HPP
