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
#include <QRunnable>
#include <QThreadPool>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <cmath>
#include <QMimeDatabase>
#include <QRegularExpression>

// The worker owns no QObject/model pointer. A GUI timer consumes its bounded
// mailbox, so destruction/cancellation cannot race a callback into a deleted model.
struct PlaylistImportState {
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<QVector<TrackItem>> batches;
    QString error;
    QString phase = QStringLiteral("Discovering media…");
    bool done = false;
    int skipped = 0;
    int duplicates = 0;
};

namespace {
class QueueFilter final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        if (filterRegularExpression().pattern().isEmpty()) return true;
        const auto index = sourceModel()->index(row, 0, parent);
        for (int role : {PlaylistModel::TitleRole, PlaylistModel::ArtistRole,
                         PlaylistModel::AlbumRole, PlaylistModel::FilePathRole})
            if (sourceModel()->data(index, role).toString().contains(filterRegularExpression())) return true;
        return false;
    }
};

QString sessionPath(const QString &requested) {
    return requested.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("session-v1.json")) : requested;
}

QString checkpointPath(const QString &requested) {
    return requested.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("session-position-v1.json")) : requested + QStringLiteral(".position.json");
}

constexpr int kImportBatch = 32;
constexpr int kMaxPendingBatches = 2;
constexpr int kMaxImportFiles = 100000;

QThreadPool &importPool()
{
    static QThreadPool pool;
    static std::once_flag configured;
    std::call_once(configured, [&] { pool.setMaxThreadCount(2); });
    return pool;
}

QStringList playlistEntries(const QString &path, QString &error,
                            const std::function<bool()> &cancel = {})
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Cannot open playlist: %1").arg(file.errorString());
        return {};
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QDir directory(QFileInfo(path).absolutePath());
    QStringList paths;
    while (!stream.atEnd()) {
        if (cancel && cancel()) return {};
        const QString entry = stream.readLine().trimmed();
        if (entry.isEmpty() || entry.startsWith(QLatin1Char('#'))) continue;
        const QUrl url(entry);
        QString media = url.isLocalFile() ? url.toLocalFile() : entry;
        if (QDir::isRelativePath(media)) media = directory.absoluteFilePath(media);
        paths.append(media);
        if (paths.size() > kMaxImportFiles) {
            error = QStringLiteral("Playlist exceeds the 100,000-track import limit.");
            return {};
        }
    }
    if (stream.status() != QTextStream::Ok || file.error() != QFileDevice::NoError) {
        error = QStringLiteral("Cannot read playlist: %1").arg(file.errorString());
        return {};
    }
    return paths;
}
}

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_filter = std::make_unique<QueueFilter>();
    m_filter->setSourceModel(this);
    m_filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_importTimer.setInterval(16);
    connect(&m_importTimer, &QTimer::timeout, this, &PlaylistModel::drainImport);
}

PlaylistModel::~PlaylistModel()
{
    // No waiting for file I/O on the GUI thread. The shared job state outlives
    // the model only until cancellation is observed by the independent worker.
    if (m_importState) {
        m_importState->cancelled.store(true);
        m_importState->ready.notify_all();
    }
}


void PlaylistModel::setSearchText(const QString &text)
{
    if (m_searchText == text) return;
    m_searchText = text;
    m_filter->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(text.trimmed()),
        QRegularExpression::CaseInsensitiveOption));
    emit searchTextChanged();
}

int PlaylistModel::visibleIndexForTrack(int index) const
{
    return m_filter->mapFromSource(this->index(index, 0)).row();
}

int PlaylistModel::trackIndexForVisible(int index) const
{
    return m_filter->mapToSource(m_filter->index(index, 0)).row();
}

void PlaylistModel::activateTrack(int index)
{
    if (index < 0 || index >= count()) return;
    if (m_currentIndex != index) {
        m_currentIndex = index;
        emit currentIndexChanged();
    }
    emit trackSelected(m_tracks[index].filePath);
}

bool PlaylistModel::saveSession(double position, const QString &requestedPath)
{
    const QString path = sessionPath(requestedPath);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setOperationError(QStringLiteral("Save session"), QStringLiteral("Cannot create the session folder."));
        return false;
    }
    QJsonArray entries;
    for (int row = 0; row < count(); ++row) {
        QVariantMap entry = getTrack(row);
        // Never duplicate inline artwork into every session checkpoint.
        if (entry.value("artworkUrl").toString().startsWith("data:")) entry.remove("artworkUrl");
        entries.append(QJsonObject::fromVariantMap(entry));
    }
    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject snapshot{{"version", 1}, {"queueId", sessionId}, {"tracks", entries},
        {"selectedFile", currentFilePath()},
        {"position", std::isfinite(position) ? std::max(0.0, position) : 0.0}};
    const QByteArray bytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (bytes.size() > 64 * 1024 * 1024 || !file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        setOperationError(QStringLiteral("Save session"), bytes.size() > 64 * 1024 * 1024
            ? QStringLiteral("The session exceeds the 64 MiB limit; save a portable playlist instead.")
            : file.errorString());
        return false;
    }
    m_sessionId = sessionId;
    // A new queue identity invalidates any older position file even if this
    // refresh fails; an old checkpoint can never override a newer full save.
    if (!checkpointSessionPosition(position, requestedPath)) return false;
    if (m_operationError.startsWith(QStringLiteral("Save session:"))) setOperationError({}, {});
    return true;
}

bool PlaylistModel::checkpointSessionPosition(double position, const QString &requestedPath)
{
    const QString path = checkpointPath(requestedPath);
    const QJsonObject snapshot{{"version", 1}, {"queueId", m_sessionId},
        {"selectedFile", currentFilePath()},
        {"position", std::isfinite(position) ? std::max(0.0, position) : 0.0}};
    const QByteArray bytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        setOperationError(QStringLiteral("Save session position"), file.errorString());
        return false;
    }
    if (m_operationError.startsWith(QStringLiteral("Save session position:"))) setOperationError({}, {});
    return true;
}

bool PlaylistModel::restoreSession(const QString &requestedPath)
{
    QFile file(sessionPath(requestedPath));
    if (!file.exists()) return false;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024) {
        setOperationError(QStringLiteral("Restore session"), QStringLiteral("Cannot read the saved session."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto snapshot = document.object();
    const auto entries = snapshot.value("tracks").toArray();
    if (parseError.error != QJsonParseError::NoError || snapshot.value("version").toInt() != 1
        || !snapshot.value("tracks").isArray() || entries.size() > kMaxImportFiles) {
        setOperationError(QStringLiteral("Restore session"), QStringLiteral("The saved session is invalid; the queue was kept."));
        return false;
    }
    QVector<TrackItem> restored;
    QSet<QString> paths;
    const QString selected = snapshot.value("selectedFile").toString();
    const QString queueId = snapshot.value("queueId").toString();
    QJsonObject checkpoint;
    QFile positionFile(checkpointPath(requestedPath));
    if (positionFile.open(QIODevice::ReadOnly) && positionFile.size() <= 64 * 1024) {
        QJsonParseError error;
        const auto positionDocument = QJsonDocument::fromJson(positionFile.readAll(), &error);
        const auto candidate = positionDocument.object();
        const double position = candidate.value("position").toDouble(-1);
        if (error.error == QJsonParseError::NoError && candidate.value("version").toInt() == 1
            && candidate.value("queueId").toString() == queueId
            && candidate.value("position").isDouble() && std::isfinite(position) && position >= 0)
            checkpoint = candidate;
    }
    int selectedIndex = -1, missing = 0;
    for (const auto &value : entries) {
        const QVariantMap entry = value.toObject().toVariantMap();
        const QFileInfo info(entry.value("filePath").toString());
        if (!info.exists() || !info.isFile() || !info.isReadable()) { ++missing; continue; }
        const QString path = info.absoluteFilePath();
        if (paths.contains(path)) continue;
        TrackItem track;
        track.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        track.filePath = path;
#define RESTORE_STRING(member) track.member = entry.value(#member).toString()
        RESTORE_STRING(title); RESTORE_STRING(artist); RESTORE_STRING(album);
        RESTORE_STRING(albumArtist); RESTORE_STRING(genre); RESTORE_STRING(year);
        RESTORE_STRING(trackNumber); RESTORE_STRING(codec); RESTORE_STRING(container);
        RESTORE_STRING(formatLabel); RESTORE_STRING(artworkUrl); RESTORE_STRING(videoCodec);
#undef RESTORE_STRING
#define RESTORE_INT(member) track.member = entry.value(#member).toInt()
        RESTORE_INT(sampleRate); RESTORE_INT(channels); RESTORE_INT(bitDepth);
        RESTORE_INT(videoWidth); RESTORE_INT(videoHeight);
#undef RESTORE_INT
        track.bitrate = entry.value("bitrate").toLongLong();
        track.duration = std::max(0.0, entry.value("duration").toDouble());
        track.frameRate = entry.value("frameRate").toDouble();
        track.hasVideo = entry.value("hasVideo").toBool();
        if (track.title.isEmpty()) track.title = info.completeBaseName();
        if (track.artist.isEmpty()) track.artist = QStringLiteral("Unknown Artist");
        if (track.album.isEmpty()) track.album = QStringLiteral("Unknown Album");
        if (path == selected) selectedIndex = restored.size();
        paths.insert(path);
        restored.append(std::move(track));
    }
    double restoredPosition = snapshot.value("position").toDouble();
    // A checkpoint may select a different song since the full queue was saved,
    // but only an entry that survived validation can take over selection.
    if (!checkpoint.isEmpty()) {
        const QString checkpointFile = checkpoint.value("selectedFile").toString();
        for (int index = 0; index < restored.size(); ++index) {
            if (restored[index].filePath == checkpointFile) {
                selectedIndex = index;
                restoredPosition = checkpoint.value("position").toDouble();
                break;
            }
        }
    }
    cancelImport();
    beginResetModel();
    m_tracks = std::move(restored);
    m_trackPaths = std::move(paths);
    m_currentIndex = selectedIndex >= 0 ? selectedIndex : (m_tracks.isEmpty() ? -1 : 0);
    m_sessionId = queueId;
    m_restoredPosition = selectedIndex >= 0 && std::isfinite(restoredPosition) ? std::max(0.0, restoredPosition) : 0;
    endResetModel();
    emit emptied();
    emit countChanged();
    emit currentIndexChanged();
    if (missing) setOperationError(QStringLiteral("Restore session"),
        QStringLiteral("%1 missing/unreadable tracks were skipped.").arg(missing));
    else setOperationError({}, {});
    // Deliberately no trackSelected: restoring metadata must never start audio.
    return true;
}

bool PlaylistModel::isSupportedMediaFile(const QFileInfo &file, bool explicitSelection,
                                          const std::function<bool()> &shouldCancel)
{
    static const QSet<QString> extensions = {
        "mp3", "mp2", "dts", "dtshd", "ac3", "eac3", "truehd", "thd", "mlp", "alac", "flac", "wav", "ogg", "m4a", "aac", "aiff", "aif", "opus", "wma",
        "mkv", "mka", "mp4", "m4v", "mov", "webm", "m2ts", "mts", "ts", "vob", "avi"
    };
    if (!file.exists() || !file.isFile() || !file.isReadable()) return false;
    if (extensions.contains(file.suffix().toLower())) return true;
    // Explicit selection is content-based; discovery also probes extensionless
    // media without wasting time probing every unrelated document in a folder.
    if (!explicitSelection && !file.suffix().isEmpty()) return false;
    const auto metadata = TrackMetadataReader::read(file.absoluteFilePath(), shouldCancel);
    if (metadata.channels > 0 || metadata.hasVideo) return true;
    const QString mime = QMimeDatabase().mimeTypeForFile(file, QMimeDatabase::MatchContent).name();
    return mime.startsWith(QStringLiteral("audio/")) || mime.startsWith(QStringLiteral("video/"));
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
    case SourceIndexRole: return index.row();
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
    roles[SourceIndexRole] = "sourceIndex";
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
    const QStringList files = QFileDialog::getOpenFileNames(
        nullptr,
        "Select Music or Video Files",
        QDir::homePath(),
        "Media Files (*.mp3 *.mp2 *.dts *.dtshd *.ac3 *.eac3 *.truehd *.thd *.mlp *.alac *.flac *.wav *.ogg *.m4a *.aac *.aiff *.aif *.opus *.wma "
        "*.mkv *.mka *.mp4 *.m4v *.mov *.webm *.m2ts *.mts *.ts *.vob *.avi);;All Files (*)"
    );
    QList<QUrl> urls;
    for (const auto &file : files) urls.append(QUrl::fromLocalFile(file));
    if (!urls.isEmpty()) importFiles(urls);
}

void PlaylistModel::openFolderDialog()
{
    const QString directory = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("Discover Music Library"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly
    );
    if (!directory.isEmpty()) importFolder(directory);
}

void PlaylistModel::loadPlaylistDialog()
{
    const QString playlist = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("Load Playlist"),
        QDir::homePath(),
        QStringLiteral("M3U Playlists (*.m3u8 *.m3u)")
    );
    if (!playlist.isEmpty()) importPlaylist(playlist);
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

TrackItem PlaylistModel::buildTrackItem(const QFileInfo &fi,
                                       const std::function<bool()> &shouldCancel)
{
    const QString absolutePath = fi.absoluteFilePath();
    const TrackMetadata metadata = TrackMetadataReader::read(absolutePath, shouldCancel);

    TrackItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.filePath = absolutePath;
    item.title = metadata.title.isEmpty() ? fi.completeBaseName() : metadata.title;
    item.artist = metadata.artist.isEmpty() ? QStringLiteral("Unknown Artist") : metadata.artist;
    item.album = metadata.album.isEmpty() ? QStringLiteral("Unknown Album") : metadata.album;
    item.albumArtist = metadata.albumArtist;
    item.genre = metadata.genre;
    item.year = metadata.year;
    item.trackNumber = metadata.trackNumber;
    item.comment = metadata.comment;
    item.codec = metadata.codec;
    item.container = metadata.container;
    item.formatLabel = metadata.formatLabel.isEmpty() ? QStringLiteral("AUDIO") : metadata.formatLabel;
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
    return item;
}

int PlaylistModel::addFileList(const QStringList &filePaths)
{
    QVector<TrackItem> newItems;
    newItems.reserve(filePaths.size());
    QSet<QString> batchSeen;

    for (const QString &filePath : filePaths) {
        const QFileInfo fi(filePath);
        if (!isSupportedMediaFile(fi, true)) continue;
        const QString absolutePath = fi.absoluteFilePath();
        if (m_trackPaths.contains(absolutePath) || batchSeen.contains(absolutePath)) continue;
        batchSeen.insert(absolutePath);
        newItems.append(buildTrackItem(fi));
    }

    return appendTrackItems(std::move(newItems));
}

int PlaylistModel::appendTrackItems(QVector<TrackItem> items)
{
    // Imports may overlap ordinary queue edits. Re-check duplicates on the
    // model thread before inserting, not just against the worker's snapshot.
    QVector<TrackItem> newItems;
    for (auto &item : items) {
        if (m_trackPaths.contains(item.filePath)) continue;
        m_trackPaths.insert(item.filePath);
        newItems.append(std::move(item));
    }
    if (newItems.isEmpty()) return 0;

    const int startRow = m_tracks.size();
    const int endRow = startRow + newItems.size() - 1;
    beginInsertRows(QModelIndex(), startRow, endRow);
    for (TrackItem &item : newItems) {
        m_tracks.append(std::move(item));
    }
    endInsertRows();

    emit countChanged();
    if (m_currentIndex == -1 && !m_tracks.isEmpty()) {
        setCurrentIndex(0);
    }
    return newItems.size();
}

void PlaylistModel::addFile(const QString &filePath)
{
    addFileList(QStringList{filePath});
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
    QCollator collator{QLocale{QLocale::English}};
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(mediaFiles.begin(), mediaFiles.end(), [&root, &collator](const QString &left,
                                                                       const QString &right) {
        return collator.compare(root.relativeFilePath(left), root.relativeFilePath(right)) < 0;
    });

    const int addedCount = addFileList(mediaFiles);
    emit folderDiscovered(root.absolutePath(), addedCount);
}

void PlaylistModel::addFiles(const QList<QUrl> &urls)
{
    QStringList directFiles;
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isDir()) {
            if (!directFiles.isEmpty()) {
                addFileList(directFiles);
                directFiles.clear();
            }
            addFolder(path);
        } else {
            directFiles.append(path);
        }
    }
    if (!directFiles.isEmpty()) {
        addFileList(directFiles);
    }
}

bool PlaylistModel::loadPlaylist(const QString &playlistPath)
{
    QString error;
    const QStringList entries = playlistEntries(playlistPath, error);
    if (!error.isEmpty()) {
        setOperationError(QStringLiteral("Load playlist"), error);
        return false;
    }
    QStringList mediaFiles;
    QSet<QString> seenPaths;
    for (const auto &path : entries) {
        const QFileInfo file(path);
        if (!isSupportedMediaFile(file, true)) continue;
        const QString absolutePath = file.absoluteFilePath();
        if (!seenPaths.contains(absolutePath)) {
            seenPaths.insert(absolutePath);
            mediaFiles.append(absolutePath);
        }
    }

    if (mediaFiles.isEmpty()) {
        setOperationError(QStringLiteral("Load playlist"), QStringLiteral("No usable media entries were found."));
        return false;
    }
    clear();
    addFileList(mediaFiles);
    setOperationError({}, {});
    return true;
}

bool PlaylistModel::savePlaylist(const QString &playlistPath)
{
    if (m_tracks.isEmpty()) {
        setOperationError(QStringLiteral("Save playlist"), QStringLiteral("The playlist is empty."));
        return false;
    }

    QSaveFile playlistFile(playlistPath);
    if (!playlistFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setOperationError(QStringLiteral("Save playlist"), playlistFile.errorString());
        return false;
    }

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
        QString entry = QDir::fromNativeSeparators(playlistDirectory.relativeFilePath(track.filePath));
        // Keep ordinary relative entries portable, but do not serialize a valid
        // filename as a comment/URL or lose significant trailing whitespace.
        if (entry.trimmed() != entry || entry.contains(QLatin1Char('\n')) || entry.contains(QLatin1Char('\r'))) {
            entry = QUrl::fromLocalFile(track.filePath).toString(QUrl::FullyEncoded);
        } else if (QDir::isRelativePath(entry)) {
            entry.prepend(QStringLiteral("./"));
        }
        stream << entry << '\n';
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !playlistFile.commit()) {
        setOperationError(QStringLiteral("Save playlist"), playlistFile.errorString());
        return false;
    }
    setOperationError({}, {});
    return true;
}

void PlaylistModel::setOperationError(const QString &operation, const QString &error)
{
    const QString message = error.isEmpty() ? QString() : operation + QStringLiteral(": ") + error;
    if (m_operationError != message) {
        m_operationError = message;
        emit operationErrorChanged();
    }
    if (!error.isEmpty()) emit operationFailed(operation, error);
}

void PlaylistModel::importFiles(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) return;
    // File-manager open requests are additive even during an existing import.
    // Bound the FIFO rather than silently dropping a request or spawning workers.
    if (m_importing || !m_pendingImports.isEmpty()) {
        if (m_pendingImports.size() >= 8) {
            setOperationError(QStringLiteral("Import media"), QStringLiteral("Eight file selections are already queued; wait or cancel before adding more."));
            return;
        }
        m_pendingImports.enqueue(urls);
        if (!m_importing) beginImport(m_pendingImports.dequeue(), {});
        else {
            m_importStatus = QStringLiteral("Import in progress · %1 file selections queued").arg(m_pendingImports.size());
            emit importChanged();
        }
        return;
    }
    beginImport(urls, {});
}

void PlaylistModel::importFolder(const QString &directoryPath)
{
    beginImport({QUrl::fromLocalFile(directoryPath)}, {});
}

void PlaylistModel::importPlaylist(const QString &playlistPath)
{
    beginImport({}, playlistPath);
}

void PlaylistModel::beginImport(const QList<QUrl> &urls, const QString &playlistPath)
{
    if (m_importing) {
        setOperationError(QStringLiteral("Import media"), QStringLiteral("Another import is in progress; finish or cancel it first."));
        return;
    }
    const auto state = std::make_shared<PlaylistImportState>();
    m_importState = state;
    m_importing = true;
    m_importedCount = 0;
    m_importStatus = state->phase;
    m_replacePending = !playlistPath.isEmpty();
    const auto existingPaths = m_replacePending ? QSet<QString>() : m_trackPaths;
    setOperationError({}, {});
    m_importTimer.start();
    emit importChanged();

    importPool().start(QRunnable::create([state, urls, playlistPath, existingPaths] {
        const auto cancelled = [state] { return state->cancelled.load(); };
        const auto finish = [state](const QString &error = {}) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->error = error;
            state->done = true;
        };
        try {
            QStringList paths;
            QString error;
            QSet<QString> seen = existingPaths;
            auto addPath = [&](const QString &path, bool explicitSelection = false) {
                const QFileInfo file(path);
                if (!isSupportedMediaFile(file, explicitSelection, cancelled)) {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    ++state->skipped;
                    return;
                }
                const auto absolute = file.absoluteFilePath();
                if (!seen.contains(absolute)) {
                    seen.insert(absolute);
                    paths.append(absolute);
                } else {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    ++state->duplicates;
                }
            };
            if (!playlistPath.isEmpty()) {
                const auto entries = playlistEntries(playlistPath, error, cancelled);
                for (const auto &path : entries) {
                    if (cancelled()) { finish(); return; }
                    addPath(path, true);
                }
            } else {
                for (const QUrl &url : urls) {
                    if (cancelled()) { finish(); return; }
                    if (!url.isLocalFile()) continue;
                    const QFileInfo input(url.toLocalFile());
                    if (!input.isDir()) { addPath(input.absoluteFilePath(), true); continue; }
                    const QDir directory(input.absoluteFilePath());
                    QStringList discovered;
                    QDirIterator iterator(directory.absolutePath(), QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                                          QDirIterator::Subdirectories);
                    while (!cancelled() && iterator.hasNext()) {
                        const QFileInfo file(iterator.next());
                        if (isSupportedMediaFile(file, false, cancelled)) discovered.append(file.absoluteFilePath());
                        if (discovered.size() + paths.size() > kMaxImportFiles) break;
                    }
                    if (cancelled()) { finish(); return; }
                    if (discovered.size() + paths.size() > kMaxImportFiles) {
                        error = QStringLiteral("Import exceeds the 100,000-track limit; select smaller folders.");
                        break;
                    }
                    QCollator collator{QLocale{QLocale::English}};
                    collator.setCaseSensitivity(Qt::CaseInsensitive);
                    collator.setNumericMode(true);
                    std::sort(discovered.begin(), discovered.end(), [&directory, &collator](const QString &a, const QString &b) {
                        return collator.compare(directory.relativeFilePath(a), directory.relativeFilePath(b)) < 0;
                    });
                    for (const auto &path : discovered) addPath(path);
                }
            }
            if (paths.size() > kMaxImportFiles) error = QStringLiteral("Import exceeds the 100,000-track limit.");
            if (!error.isEmpty()) { finish(error); return; }
            if (paths.isEmpty()) {
                finish(state->duplicates > 0 && state->skipped == 0
                    ? QStringLiteral("All selected tracks are already in the queue.")
                    : QStringLiteral("No readable supported media was found; %1 entries skipped.").arg(state->skipped));
                return;
            }
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->phase = QStringLiteral("Reading %1 tracks…").arg(paths.size());
            }
            QVector<TrackItem> batch;
            const auto publish = [&] {
                std::unique_lock<std::mutex> lock(state->mutex);
                state->ready.wait(lock, [&] { return cancelled() || state->batches.size() < kMaxPendingBatches; });
                if (cancelled()) return false;
                state->batches.push_back(std::move(batch));
                batch = {};
                return true;
            };
            for (const auto &path : paths) {
                if (cancelled()) { finish(); return; }
                batch.append(buildTrackItem(QFileInfo(path), cancelled));
                if (cancelled()) { finish(); return; }
                if (batch.size() >= kImportBatch && !publish()) { finish(); return; }
            }
            if (!batch.isEmpty() && !publish()) { finish(); return; }
            finish();
        } catch (const std::exception &) {
            finish(QStringLiteral("Import failed while reading media. Try a smaller selection."));
        }
    }));
}

void PlaylistModel::drainImport()
{
    const auto state = m_importState;
    if (!state) return;
    std::deque<QVector<TrackItem>> batches;
    QString phase, error;
    bool done;
    int skipped, duplicates;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        batches.swap(state->batches);
        phase = state->phase;
        error = state->error;
        done = state->done;
        skipped = state->skipped;
        duplicates = state->duplicates;
    }
    state->ready.notify_all();
    for (auto &batch : batches) {
        if (m_importState != state) return;
        if (m_replacePending) {
            m_replacePending = false;
            resetTracks();
        }
        if (m_importState != state) return;
        m_importedCount += appendTrackItems(std::move(batch));
    }
    if (m_importState != state) return;
    m_importStatus = done ? QStringLiteral("Imported %1 tracks").arg(m_importedCount)
                          : phase + QStringLiteral(" · %1 added").arg(m_importedCount);
    if (done && duplicates > 0) m_importStatus += QStringLiteral(" · %1 duplicates skipped").arg(duplicates);
    if (done && skipped > 0) m_importStatus += QStringLiteral(" · %1 unreadable/unsupported skipped").arg(skipped);
    if (!done && !m_pendingImports.isEmpty())
        m_importStatus += QStringLiteral(" · %1 selections queued").arg(m_pendingImports.size());
    if (done) {
        m_importTimer.stop();
        m_importState.reset();
        m_importing = false;
        if (!error.isEmpty()) setOperationError(QStringLiteral("Import media"), error);
        emit importFinished(m_importedCount, false);
    }
    emit importChanged();
    if (done && !m_importing && !m_pendingImports.isEmpty())
        beginImport(m_pendingImports.dequeue(), {});
}

void PlaylistModel::cancelImport()
{
    m_pendingImports.clear();
    if (!m_importState) return;
    m_importState->cancelled.store(true);
    m_importState->ready.notify_all();
    m_importState.reset();
    m_importTimer.stop();
    m_importing = false;
    m_replacePending = false;
    m_importStatus = QStringLiteral("Import cancelled · %1 tracks kept").arg(m_importedCount);
    emit importChanged();
    emit importFinished(m_importedCount, true);
}

void PlaylistModel::removeTrack(int index)
{
    if (index < 0 || index >= m_tracks.size()) return;

    const bool removedCurrent = index == m_currentIndex;

    beginRemoveRows(QModelIndex(), index, index);
    m_trackPaths.remove(m_tracks[index].filePath);
    m_tracks.removeAt(index);
    endRemoveRows();
    if (!m_tracks.isEmpty()) emit dataChanged(this->index(0), this->index(m_tracks.size() - 1), {SourceIndexRole});

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
    cancelImport();
    resetTracks();
    m_importedCount = 0;
    m_importStatus.clear();
    emit importChanged();
}

void PlaylistModel::resetTracks()
{
    beginResetModel();
    m_tracks.clear();
    m_trackPaths.clear();
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
    emit dataChanged(this->index(std::min(from, to)), this->index(std::max(from, to)), {SourceIndexRole});

    const int previousIndex = m_currentIndex;
    if (m_currentIndex == from) m_currentIndex = to;
    else if (from < m_currentIndex && to >= m_currentIndex) --m_currentIndex;
    else if (from > m_currentIndex && to <= m_currentIndex) ++m_currentIndex;
    if (m_currentIndex != previousIndex) emit currentIndexChanged();
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

void PlaylistModel::followPlayback(const QString &filePath)
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].filePath == filePath) {
            if (m_currentIndex != i) {
                m_currentIndex = i;
                emit currentIndexChanged();
            }
            return; // Engine has already started this track; do not reload it.
        }
    }
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
