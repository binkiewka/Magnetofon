#include "TrackMetadataReader.hpp"

#if __has_include(<libavcodec/avcodec.h>) && __has_include(<libavformat/avformat.h>)
#define HAVE_FFMPEG 1
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
}
#else
#define HAVE_FFMPEG 0
#endif


#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QStringList>

#include <algorithm>
#include <mutex>

namespace {

#if HAVE_FFMPEG
QString tagValue(const AVDictionary *dictionary, const QStringList &keys)
{
    if (!dictionary) return {};

    for (const QString &key : keys) {
        const QByteArray utf8Key = key.toUtf8();
        const AVDictionaryEntry *entry = av_dict_get(dictionary, utf8Key.constData(), nullptr, 0);
        if (entry && entry->value) {
            const QString value = QString::fromUtf8(entry->value).trimmed();
            if (!value.isEmpty()) return value;
        }
    }
    return {};
}

QString firstTag(const AVFormatContext *context, const AVStream *audioStream,
                 const QStringList &keys)
{
    QString value = tagValue(context ? context->metadata : nullptr, keys);
    if (value.isEmpty() && audioStream) value = tagValue(audioStream->metadata, keys);
    return value;
}

QString codecLabel(AVCodecID codecId)
{
    const char *rawName = avcodec_get_name(codecId);
    QString name = rawName ? QString::fromLatin1(rawName) : QString();
    if (name.startsWith("pcm_", Qt::CaseInsensitive)) return QStringLiteral("PCM");
    if (name.compare("mp3", Qt::CaseInsensitive) == 0) return QStringLiteral("MP3");
    if (name.compare("flac", Qt::CaseInsensitive) == 0) return QStringLiteral("FLAC");
    if (name.compare("aac", Qt::CaseInsensitive) == 0) return QStringLiteral("AAC");
    if (name.compare("alac", Qt::CaseInsensitive) == 0) return QStringLiteral("ALAC");
    if (name.compare("vorbis", Qt::CaseInsensitive) == 0) return QStringLiteral("VORBIS");
    if (name.compare("opus", Qt::CaseInsensitive) == 0) return QStringLiteral("OPUS");
    if (name.compare("wmav1", Qt::CaseInsensitive) == 0 ||
        name.compare("wmav2", Qt::CaseInsensitive) == 0 ||
        name.compare("wmapro", Qt::CaseInsensitive) == 0) return QStringLiteral("WMA");
    return name.toUpper();
}
#endif

QString channelLabel(int channels)
{
    if (channels == 1) return QStringLiteral("MONO");
    if (channels == 2) return QStringLiteral("STEREO");
    if (channels > 2) return QString::number(channels) + QStringLiteral(" CH");
    return {};
}

#if HAVE_FFMPEG
QString artworkMimeType(AVCodecID codecId)
{
    switch (codecId) {
    case AV_CODEC_ID_PNG: return QStringLiteral("image/png");
    case AV_CODEC_ID_WEBP: return QStringLiteral("image/webp");
    case AV_CODEC_ID_BMP: return QStringLiteral("image/bmp");
    case AV_CODEC_ID_GIF: return QStringLiteral("image/gif");
    case AV_CODEC_ID_MJPEG:
    case AV_CODEC_ID_JPEG2000:
        return QStringLiteral("image/jpeg");
    default:
        return QStringLiteral("image/jpeg");
    }
}
#endif


QString folderArtworkUrl(const QString &audioFilePath)
{
    const QFileInfo audioFile(audioFilePath);
    const QFileInfoList directoryFiles = QDir(audioFile.absolutePath()).entryInfoList(
        QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    const QString trackStem = audioFile.completeBaseName().toLower();
    static const QStringList preferredNames = {
        QStringLiteral("cover"), QStringLiteral("folder"), QStringLiteral("front"),
        QStringLiteral("album"), QStringLiteral("albumart"), QStringLiteral("album_art"),
        QStringLiteral("artwork"), QStringLiteral("thumbnail")
    };
    static const QStringList imageExtensions = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("webp"), QStringLiteral("bmp")
    };

    QFileInfoList images;
    for (const QFileInfo &candidate : directoryFiles) {
        if (imageExtensions.contains(candidate.suffix().toLower())) images.append(candidate);
    }

    auto asUrl = [](const QFileInfo &file) {
        return QUrl::fromLocalFile(file.absoluteFilePath()).toString();
    };

    for (const QFileInfo &image : images) {
        if (image.completeBaseName().compare(trackStem, Qt::CaseInsensitive) == 0) return asUrl(image);
    }
    for (const QString &preferredName : preferredNames) {
        for (const QFileInfo &image : images) {
            if (image.completeBaseName().compare(preferredName, Qt::CaseInsensitive) == 0)
                return asUrl(image);
        }
    }
    for (const QString &preferredName : preferredNames) {
        for (const QFileInfo &image : images) {
            if (image.completeBaseName().startsWith(preferredName, Qt::CaseInsensitive))
                return asUrl(image);
        }
    }
    return images.size() == 1 ? asUrl(images.constFirst()) : QString();
}

} // namespace

TrackMetadata TrackMetadataReader::read(const QString &filePath)
{
    TrackMetadata metadata;
    metadata.artworkUrl = folderArtworkUrl(filePath);

#if HAVE_FFMPEG
    static std::once_flag logSetup;
    std::call_once(logSetup, [] { av_log_set_level(AV_LOG_ERROR); });

    AVFormatContext *context = nullptr;
    const QByteArray encodedPath = QFile::encodeName(filePath);
    if (avformat_open_input(&context, encodedPath.constData(), nullptr, nullptr) < 0) {
        return metadata;
    }

    if (avformat_find_stream_info(context, nullptr) < 0) {
        avformat_close_input(&context);
        return metadata;
    }

    AVStream *audioStream = nullptr;
    const int audioIndex = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioIndex >= 0) audioStream = context->streams[audioIndex];

    metadata.title = firstTag(context, audioStream, {QStringLiteral("title")});
    metadata.artist = firstTag(context, audioStream, {QStringLiteral("artist")});
    metadata.album = firstTag(context, audioStream, {QStringLiteral("album")});
    metadata.albumArtist = firstTag(context, audioStream,
                                    {QStringLiteral("album_artist"), QStringLiteral("albumartist"),
                                     QStringLiteral("album artist")});
    metadata.genre = firstTag(context, audioStream, {QStringLiteral("genre")});
    metadata.year = firstTag(context, audioStream,
                             {QStringLiteral("date"), QStringLiteral("year")});
    metadata.trackNumber = firstTag(context, audioStream,
                                    {QStringLiteral("track"), QStringLiteral("tracknumber")});
    metadata.comment = firstTag(context, audioStream,
                                {QStringLiteral("comment"), QStringLiteral("description")});

    if (context->duration != AV_NOPTS_VALUE && context->duration > 0) {
        metadata.duration = static_cast<double>(context->duration) / AV_TIME_BASE;
    }
    if (context->iformat) {
        const char *containerName = context->iformat->long_name
                                        ? context->iformat->long_name
                                        : context->iformat->name;
        if (containerName) metadata.container = QString::fromUtf8(containerName);
    }

    if (audioStream && audioStream->codecpar) {
        const AVCodecParameters *parameters = audioStream->codecpar;
        metadata.codec = codecLabel(parameters->codec_id);
        metadata.sampleRate = parameters->sample_rate;
        metadata.channels = parameters->ch_layout.nb_channels;
        metadata.bitDepth = parameters->bits_per_raw_sample > 0
                                ? parameters->bits_per_raw_sample
                                : parameters->bits_per_coded_sample;
        if (metadata.bitDepth <= 0) metadata.bitDepth = av_get_bits_per_sample(parameters->codec_id);
        metadata.bitrate = parameters->bit_rate > 0 ? parameters->bit_rate : context->bit_rate;

        const bool highQualityFlac = metadata.codec == QStringLiteral("FLAC")
                                     && (metadata.bitDepth >= 24 || metadata.sampleRate >= 48000);
        if (highQualityFlac) {
            metadata.formatLabel = QStringLiteral("HQ FLAC");
        } else {
            const QString channels = channelLabel(metadata.channels);
            metadata.formatLabel = metadata.codec;
            if (!channels.isEmpty()) metadata.formatLabel += QStringLiteral(" ") + channels;
        }
    }

    for (unsigned int i = 0; i < context->nb_streams; ++i) {
        AVStream *stream = context->streams[i];
        if (!stream || !stream->codecpar || !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC)) continue;
        const AVPacket &picture = stream->attached_pic;
        if (!picture.data || picture.size <= 0) continue;

        const QByteArray bytes(reinterpret_cast<const char *>(picture.data), picture.size);
        metadata.artworkUrl = QStringLiteral("data:") + artworkMimeType(stream->codecpar->codec_id)
                              + QStringLiteral(";base64,")
                              + QString::fromLatin1(bytes.toBase64());
        break;
    }

    avformat_close_input(&context);
#else
    const QFileInfo fileInfo(filePath);
    metadata.title = fileInfo.completeBaseName();
    metadata.formatLabel = fileInfo.suffix().toUpper();
#endif

    return metadata;
}

