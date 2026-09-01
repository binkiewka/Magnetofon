#ifndef TRACK_METADATA_READER_HPP
#define TRACK_METADATA_READER_HPP

#include <QByteArray>
#include <QString>

struct TrackMetadata {
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

class TrackMetadataReader {
public:
    static TrackMetadata read(const QString &filePath);
};

#endif // TRACK_METADATA_READER_HPP
