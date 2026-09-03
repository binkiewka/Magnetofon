#include "PlaylistModel.hpp"
#include "TrackMetadataReader.hpp"
#include <QFileInfo>
#include <QUuid>
#include <QFileDialog>
#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>
#include <algorithm>
#include <utility>

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

bool PlaylistModel::isSupportedMediaFile(const QFileInfo &file)
{
    static const QSet<QString> extensions = {
        "mp3", "flac", "wav", "ogg", "m4a", "aac", "aiff", "aif", "opus", "wma",
        "mkv", "mka", "mp4", "m4v", "mov", "webm", "m2ts", "mts", "ts", "vob", "avi"
    };
    return file.exists() && file.isFile() && extensions.contains(file.suffix().toLower());
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_tracks.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size()) {
        return QVariant();
    }

    const TrackItem &track = m_tracks[index.row()];
    switch (role) {
    case IdRole: return track.id;
    case FilePathRole: return track.filePath;
    case TitleRole: return track.title;
    case ArtistRole: return track.artist;
    case AlbumRole: return track.album;
    case DurationRole: return track.duration;
    case AlbumArtistRole: return track.albumArtist;
    case GenreRole: return track.genre;
    case YearRole: return track.year;
    case TrackNumberRole: return track.trackNumber;
    case CommentRole: return track.comment;
    case CodecRole: return track.codec;
    case ContainerRole: return track.container;
    case FormatLabelRole: return track.formatLabel;
    case ArtworkUrlRole: return track.artworkUrl;
    case SampleRateRole: return track.sampleRate;
    case ChannelsRole: return track.channels;
    case BitDepthRole: return track.bitDepth;
    case BitrateRole: return track.bitrate;
    case HasVideoRole: return track.hasVideo;
    case VideoCodecRole: return track.videoCodec;
    case VideoWidthRole: return track.videoWidth;
    case VideoHeightRole: return track.videoHeight;
    case FrameRateRole: return track.frameRate;
    default: return QVariant();
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[FilePathRole] = "filePath";
    roles[TitleRole] = "title";
    roles[ArtistRole] = "artist";
    roles[AlbumRole] = "album";
    roles[DurationRole] = "duration";
    roles[AlbumArtistRole] = "albumArtist";
    roles[GenreRole] = "genre";
    roles[YearRole] = "year";
    roles[TrackNumberRole] = "trackNumber";
    roles[CommentRole] = "comment";
    roles[CodecRole] = "codec";
    roles[ContainerRole] = "container";
    roles[FormatLabelRole] = "formatLabel";
    roles[ArtworkUrlRole] = "artworkUrl";
    roles[SampleRateRole] = "sampleRate";
    roles[ChannelsRole] = "channels";
    roles[BitDepthRole] = "bitDepth";
    roles[BitrateRole] = "bitrate";
    roles[HasVideoRole] = "hasVideo";
    roles[VideoCodecRole] = "videoCodec";
    roles[VideoWidthRole] = "videoWidth";
    roles[VideoHeightRole] = "videoHeight";
    roles[FrameRateRole] = "frameRate";
    return roles;
}

void PlaylistModel::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tracks.size()) return;
    if (m_currentIndex == index) return;
    m_currentIndex = index;
    emit currentIndexChanged();
    emit trackSelected(m_tracks[m_currentIndex].filePath);
}

void PlaylistModel::openFileDialog()
{
    QStringList files = QFileDialog::getOpenFileNames(
        nullptr,
        "Select Music or Video Files",
        QDir::homePath(),
        "Media Files (*.mp3 *.flac *.wav *.ogg *.m4a *.aac *.aiff *.aif *.opus *.wma "
        "*.mkv *.mka *.mp4 *.m4v *.mov *.webm *.m2ts *.mts *.ts *.vob *.avi)"
    );
    for (const QString &file : files) {
        addFile(file);
    }
}

void PlaylistModel::openFolderDialog()
{
    const QString directory = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("Discover Music Library"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly
    );
    if (!directory.isEmpty()) addFolder(directory);
}

void PlaylistModel::loadPlaylistDialog()
{
    const QString playlist = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("Load Playlist"),
        QDir::homePath(),
        QStringLiteral("M3U Playlists (*.m3u8 *.m3u)")
    );
    if (!playlist.isEmpty()) loadPlaylist(playlist);
}

void PlaylistModel::savePlaylistDialog()
{
    if (m_tracks.isEmpty()) return;

    QString playlist = QFileDialog::getSaveFileName(
        nullptr,
        QStringLiteral("Save Playlist"),
        QDir::home().filePath(QStringLiteral("Magnetofon Playlist.m3u8")),
        QStringLiteral("M3U8 Playlist (*.m3u8)")
    );
    if (playlist.isEmpty()) return;
    if (QFileInfo(playlist).suffix().isEmpty()) playlist += QStringLiteral(".m3u8");
    savePlaylist(playlist);
}

void PlaylistModel::addFile(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!isSupportedMediaFile(fi)) return;

    const QString absolutePath = fi.absoluteFilePath();
    for (const auto &track : std::as_const(m_tracks)) {
        if (track.filePath == absolutePath) return;
    }

    const TrackMetadata metadata = TrackMetadataReader::read(absolutePath);

    beginInsertRows(QModelIndex(), m_tracks.size(), m_tracks.size());
    TrackItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.filePath = absolutePath;
    item.title = metadata.title.isEmpty() ? fi.completeBaseName() : metadata.title;
    item.artist = metadata.artist.isEmpty() ? "Unknown Artist" : metadata.artist;
    item.album = metadata.album.isEmpty() ? "Unknown Album" : metadata.album;
    item.albumArtist = metadata.albumArtist;
    item.genre = metadata.genre;
    item.year = metadata.year;
    item.trackNumber = metadata.trackNumber;
    item.comment = metadata.comment;
    item.codec = metadata.codec;
    item.container = metadata.container;
    item.formatLabel = metadata.formatLabel.isEmpty() ? "AUDIO" : metadata.formatLabel;
    item.artworkUrl = metadata.artworkUrl;
    item.videoCodec = metadata.videoCodec;
    item.duration = metadata.duration;
    item.frameRate = metadata.frameRate;
    item.sampleRate = metadata.sampleRate;
    item.channels = metadata.channels;
    item.bitDepth = metadata.bitDepth;
    item.videoWidth = metadata.videoWidth;
    item.videoHeight = metadata.videoHeight;
    item.bitrate = metadata.bitrate;
    item.hasVideo = metadata.hasVideo;
    m_tracks.append(item);
    endInsertRows();

    emit countChanged();

    if (m_currentIndex == -1) {
        setCurrentIndex(0);
    }
}

void PlaylistModel::addFolder(const QString &directoryPath)
{
    const QDir root(directoryPath);
    if (!root.exists()) return;

    QStringList mediaFiles;
    QDirIterator iterator(root.absolutePath(),
                          QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QFileInfo file(iterator.next());
        if (isSupportedMediaFile(file)) mediaFiles.append(file.absoluteFilePath());
    }

    // The C locale ignores QCollator's numeric mode. Use a Unicode-aware locale
    // explicitly so numbered album and track names retain natural order.
    QCollator collator(QLocale(QLocale::English));
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(mediaFiles.begin(), mediaFiles.end(), [&root, &collator](const QString &left,
                                                                       const QString &right) {
        return collator.compare(root.relativeFilePath(left), root.relativeFilePath(right)) < 0;
    });

    const int previousCount = m_tracks.size();
    for (const QString &file : std::as_const(mediaFiles)) addFile(file);
    emit folderDiscovered(root.absolutePath(), m_tracks.size() - previousCount);
}

void PlaylistModel::addFiles(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isDir()) addFolder(path);
        else addFile(path);
    }
}

bool PlaylistModel::loadPlaylist(const QString &playlistPath)
{
    QFile playlistFile(playlistPath);
    if (!playlistFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream stream(&playlistFile);
    stream.setEncoding(QStringConverter::Utf8);
    const QDir playlistDirectory(QFileInfo(playlistPath).absolutePath());
    QStringList mediaFiles;
    QSet<QString> seenPaths;

    while (!stream.atEnd()) {
        const QString entry = stream.readLine().trimmed();
        if (entry.isEmpty() || entry.startsWith(QLatin1Char('#'))) continue;

        const QUrl url(entry);
        QString path = url.isLocalFile() ? url.toLocalFile() : entry;
        if (QDir::isRelativePath(path)) path = playlistDirectory.absoluteFilePath(path);

        const QFileInfo file(path);
        if (!isSupportedMediaFile(file)) continue;
        const QString absolutePath = file.absoluteFilePath();
        if (!seenPaths.contains(absolutePath)) {
            seenPaths.insert(absolutePath);
            mediaFiles.append(absolutePath);
        }
    }

    if (mediaFiles.isEmpty()) return false;
    clear();
    for (const QString &path : std::as_const(mediaFiles)) addFile(path);
    return true;
}

bool PlaylistModel::savePlaylist(const QString &playlistPath) const
{
    if (m_tracks.isEmpty()) return false;

    QSaveFile playlistFile(playlistPath);
    if (!playlistFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream stream(&playlistFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "#EXTM3U\n";

    const QDir playlistDirectory(QFileInfo(playlistPath).absolutePath());
    for (const TrackItem &track : m_tracks) {
        QString displayName = track.title;
        if (!track.artist.isEmpty() && track.artist != QStringLiteral("Unknown Artist")) {
            displayName.prepend(track.artist + QStringLiteral(" - "));
        }
        displayName.replace(QLatin1Char('\n'), QLatin1Char(' '));
        displayName.replace(QLatin1Char('\r'), QLatin1Char(' '));

        stream << "#EXTINF:" << qRound64(track.duration) << ',' << displayName << '\n';
        stream << QDir::fromNativeSeparators(playlistDirectory.relativeFilePath(track.filePath)) << '\n';
    }
    stream.flush();
    return stream.status() == QTextStream::Ok && playlistFile.commit();
}

void PlaylistModel::removeTrack(int index)
{
    if (index < 0 || index >= m_tracks.size()) return;

    const bool removedCurrent = index == m_currentIndex;

    beginRemoveRows(QModelIndex(), index, index);
    m_tracks.removeAt(index);
    endRemoveRows();

    emit countChanged();

    if (m_tracks.isEmpty()) {
        m_currentIndex = -1;
        emit currentIndexChanged();
        emit emptied();
    } else if (removedCurrent) {
        m_currentIndex = std::min(index, static_cast<int>(m_tracks.size()) - 1);
        emit currentIndexChanged();
        emit trackSelected(m_tracks[m_currentIndex].filePath);
    } else if (index < m_currentIndex) {
        --m_currentIndex;
        emit currentIndexChanged();
    }
}

void PlaylistModel::clear()
{
    beginResetModel();
    m_tracks.clear();
    m_currentIndex = -1;
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
    emit emptied();
}

void PlaylistModel::moveTrack(int from, int to)
{
    if (from < 0 || from >= m_tracks.size() || to < 0 || to >= m_tracks.size() || from == to) {
        return;
    }

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_tracks.move(from, to);
    endMoveRows();

    if (m_currentIndex == from) {
        m_currentIndex = to;
        emit currentIndexChanged();
    }
}

QVariantMap PlaylistModel::getTrack(int index) const
{
    QVariantMap map;
    if (index >= 0 && index < m_tracks.size()) {
        const auto &t = m_tracks[index];
        map["id"] = t.id;
        map["filePath"] = t.filePath;
        map["title"] = t.title;
        map["artist"] = t.artist;
        map["album"] = t.album;
        map["duration"] = t.duration;
        map["albumArtist"] = t.albumArtist;
        map["genre"] = t.genre;
        map["year"] = t.year;
        map["trackNumber"] = t.trackNumber;
        map["comment"] = t.comment;
        map["codec"] = t.codec;
        map["container"] = t.container;
        map["formatLabel"] = t.formatLabel;
        map["artworkUrl"] = t.artworkUrl;
        map["sampleRate"] = t.sampleRate;
        map["channels"] = t.channels;
        map["bitDepth"] = t.bitDepth;
        map["bitrate"] = t.bitrate;
        map["hasVideo"] = t.hasVideo;
        map["videoCodec"] = t.videoCodec;
        map["videoWidth"] = t.videoWidth;
        map["videoHeight"] = t.videoHeight;
        map["frameRate"] = t.frameRate;
    }
    return map;
}

QString PlaylistModel::currentFilePath() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_tracks.size()) {
        return m_tracks[m_currentIndex].filePath;
    }
    return QString();
}

void PlaylistModel::nextTrack()
{
    if (m_tracks.isEmpty()) return;
    int nextIdx = (m_currentIndex + 1) % m_tracks.size();
    setCurrentIndex(nextIdx);
}

void PlaylistModel::previousTrack()
{
    if (m_tracks.isEmpty()) return;
    int prevIdx = (m_currentIndex - 1 + m_tracks.size()) % m_tracks.size();
    setCurrentIndex(prevIdx);
}
