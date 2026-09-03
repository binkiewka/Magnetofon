#include "AudioPlayer.hpp"
#include "AudioRouting.hpp"
#include "PulseAudioAnalyzer.hpp"
#include "VideoWindow.hpp"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace {

const mpv_node *mapValue(const mpv_node &node, const char *key)
{
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list) return nullptr;
    for (int i = 0; i < node.u.list->num; ++i) {
        if (node.u.list->keys[i] && strcmp(node.u.list->keys[i], key) == 0) {
            return &node.u.list->values[i];
        }
    }
    return nullptr;
}

QString nodeString(const mpv_node &node, const char *key)
{
    const mpv_node *value = mapValue(node, key);
    return value && value->format == MPV_FORMAT_STRING && value->u.string
        ? QString::fromUtf8(value->u.string)
        : QString();
}

int nodeInt(const mpv_node &node, const char *key, int fallback = 0)
{
    const mpv_node *value = mapValue(node, key);
    return value && value->format == MPV_FORMAT_INT64
        ? static_cast<int>(value->u.int64)
        : fallback;
}

bool nodeFlag(const mpv_node &node, const char *key)
{
    const mpv_node *value = mapValue(node, key);
    return value && value->format == MPV_FORMAT_FLAG && value->u.flag != 0;
}

QString codecDisplayName(const QString &codec)
{
    const QString normalized = codec.trimmed().toLower();
    if (normalized == QStringLiteral("dts")) return QStringLiteral("DTS");
    if (normalized == QStringLiteral("truehd")) return QStringLiteral("DOLBY TRUEHD");
    if (normalized == QStringLiteral("ac3")) return QStringLiteral("DOLBY DIGITAL");
    if (normalized == QStringLiteral("eac3")) return QStringLiteral("DOLBY DIGITAL PLUS");
    if (normalized == QStringLiteral("mlp")) return QStringLiteral("MLP");
    if (normalized == QStringLiteral("flac")) return QStringLiteral("FLAC");
    if (normalized.startsWith(QStringLiteral("pcm_"))) return QStringLiteral("PCM");
    return normalized.toUpper();
}

QString sampleFormatLabel(const QString &format)
{
    const QString normalized = format.toLower();
    if (normalized.startsWith(QStringLiteral("s16"))) return QStringLiteral("16-BIT PCM");
    if (normalized.startsWith(QStringLiteral("s24"))) return QStringLiteral("24-BIT PCM");
    if (normalized.startsWith(QStringLiteral("s32"))) return QStringLiteral("32-BIT PCM");
    if (normalized.startsWith(QStringLiteral("float")) || normalized.startsWith(QStringLiteral("flt")))
        return QStringLiteral("32-BIT FLOAT PCM");
    if (normalized.startsWith(QStringLiteral("double")) || normalized.startsWith(QStringLiteral("dbl")))
        return QStringLiteral("64-BIT FLOAT PCM");
    return format.isEmpty() ? QStringLiteral("PCM") : format.toUpper() + QStringLiteral(" PCM");
}

} // namespace

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_videoWindow(std::make_unique<VideoWindow>())
{
    m_eqBands = QVariantList{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 16; ++i) m_spectrum.append(0.0);

    connect(m_videoWindow.get(), &VideoWindow::windowVisibilityChanged, this,
            [this](bool visible) {
                if (m_videoVisible == visible) return;
                m_videoVisible = visible;
                emit videoVisibleChanged();
            });

    initMpv();
    m_audioAnalyzer = std::make_unique<PulseAudioAnalyzer>();

    m_eventTimer = new QTimer(this);
    connect(m_eventTimer, &QTimer::timeout, this, &AudioPlayer::processMpvEvents);
    m_eventTimer->start(20);

    m_analysisTimer = new QTimer(this);
    connect(m_analysisTimer, &QTimer::timeout, this, &AudioPlayer::updateAudioAnalysis);
    m_analysisTimer->start(33);
}

AudioPlayer::~AudioPlayer()
{
    m_audioAnalyzer.reset();
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void AudioPlayer::initMpv()
{
    std::setlocale(LC_NUMERIC, "C");
    m_mpv = mpv_create();

    if (!m_mpv) {
        qCritical() << "[AudioPlayer] Failed to create mpv instance";
        return;
    }

    mpv_set_option_string(m_mpv, "no-config", "yes");
    const QString platformName = QGuiApplication::platformName();
    const bool nativeVideoWindow = platformName != QStringLiteral("offscreen")
                                   && platformName != QStringLiteral("minimal");
    mpv_set_option_string(m_mpv, "vo", nativeVideoWindow ? "gpu-next,gpu,xv,x11,null" : "null");
    mpv_set_option_string(m_mpv, "ao", "pulse,pipewire,wasapi,alsa,null");

    if (nativeVideoWindow) {
        int64_t videoWindowId = static_cast<int64_t>(m_videoWindow->renderTargetId());
        mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &videoWindowId);
    }
    mpv_set_option_string(m_mpv, "force-window", "no");
    mpv_set_option_string(m_mpv, "keep-open", "no");
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "osc", "no");

    mpv_set_option_string(m_mpv, "audio-client-name", "Magnetofon");
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "audio-channels", "auto-safe");
    mpv_set_option_string(m_mpv, "audio-spdif", "");
    mpv_set_option_string(m_mpv, "ad-lavc-downmix", "no");
    mpv_set_option_string(m_mpv, "gapless-audio", "no");

    const int status = mpv_initialize(m_mpv);
    if (status < 0) {
        qCritical() << "[AudioPlayer] Failed to initialize mpv:" << mpv_error_string(status);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "aid", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "audio-params/channel-count", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "audio-out-params/channel-count", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "audio-out-params/samplerate", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "audio-out-params/hr-channels", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "audio-out-params/format", MPV_FORMAT_STRING);
}

void AudioPlayer::setFileLoaded(bool loaded)
{
    if (m_fileLoaded == loaded) return;
    m_fileLoaded = loaded;
    emit hasLoadedMediaChanged();
    if (!loaded) setPlaying(false);
}

void AudioPlayer::setPlaying(bool playing)
{
    playing = playing && m_fileLoaded && !m_currentFile.isEmpty();
    if (m_isPlaying == playing) return;
    m_isPlaying = playing;
    emit isPlayingChanged();
}

void AudioPlayer::queueLoadCurrentFile()
{
    if (!m_mpv || m_currentFile.isEmpty() || m_loadPending) return;
    const QByteArray encodedPath = QFile::encodeName(m_currentFile);
    const char *command[] = {"loadfile", encodedPath.constData(), "replace", nullptr};
    const int status = mpv_command_async(m_mpv, 0, command);
    if (status < 0) {
        qWarning() << "[AudioPlayer] Failed to queue file:" << mpv_error_string(status);
        return;
    }
    m_loadPending = true;
}

void AudioPlayer::processMpvEvents()
{
    if (!m_mpv) return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
        case MPV_EVENT_START_FILE:
            setFileLoaded(false);
            break;

        case MPV_EVENT_FILE_LOADED: {
            m_loadPending = false;
            setFileLoaded(true);
            updateAudioTracks();
            updateSourceAudioParams();
            updateOutputAudioParams();
            updateVideoInfo();
            if (m_hasVideo) showVideo();
            if (m_playWhenLoaded) {
                int paused = 0;
                m_paused = false;
                mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
            }
            setPlaying(!m_paused);
            applyAudioFilters();
            break;
        }

        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *property = static_cast<mpv_event_property *>(event->data);
            if (strcmp(property->name, "time-pos") == 0 && property->format == MPV_FORMAT_DOUBLE) {
                if (property->data) {
                    const double value = *static_cast<double *>(property->data);
                    if (std::abs(m_position - value) > 0.03) {
                        m_position = value;
                        emit positionChanged();
                    }
                }
            } else if (strcmp(property->name, "duration") == 0 && property->format == MPV_FORMAT_DOUBLE) {
                if (property->data) {
                    const double value = *static_cast<double *>(property->data);
                    if (std::abs(m_duration - value) > 0.03) {
                        m_duration = value;
                        emit durationChanged();
                    }
                }
            } else if (strcmp(property->name, "pause") == 0 && property->format == MPV_FORMAT_FLAG) {
                if (property->data) {
                    m_paused = *static_cast<int *>(property->data) != 0;
                    setPlaying(!m_paused);
                }
            } else if (strcmp(property->name, "aid") == 0) {
                updateAudioTracks();
                updateSourceAudioParams();
                applyAudioFilters();
            } else if (strcmp(property->name, "audio-params/channel-count") == 0) {
                updateSourceAudioParams();
                applyAudioFilters();
            } else if (strncmp(property->name, "audio-out-params/", 17) == 0) {
                updateOutputAudioParams();
            }
            break;
        }

        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            const bool reachedEnd = end && end->reason == MPV_END_FILE_REASON_EOF;
            if (end && end->reason == MPV_END_FILE_REASON_ERROR) {
                qWarning() << "[AudioPlayer] Playback failed:" << mpv_error_string(end->error);
            }
            m_loadPending = false;
            m_playWhenLoaded = false;
            setFileLoaded(false);
            clearMediaInfo();
            resetAnalysis();
            if (reachedEnd) emit trackEnded();
            break;
        }

        default:
            break;
        }
    }
}

void AudioPlayer::load(const QString &filePath)
{
    if (!m_mpv) return;

    const QFileInfo file(filePath);
    if (!file.exists() || !file.isFile()) {
        qWarning() << "[AudioPlayer] File does not exist:" << filePath;
        return;
    }

    if (m_currentFile != file.absoluteFilePath()) {
        m_currentFile = file.absoluteFilePath();
        emit currentFileChanged();
    }

    m_playWhenLoaded = false;
    m_loadPending = false;
    clearMediaInfo();
    if (m_sourceChannels != 0) {
        m_sourceChannels = 0;
        emit sourceChannelsChanged();
    }
    setFileLoaded(false);
    setPlaying(false);

    if (m_position != 0.0) {
        m_position = 0.0;
        emit positionChanged();
    }
    if (m_duration != 0.0) {
        m_duration = 0.0;
        emit durationChanged();
    }

    int paused = 1;
    m_paused = true;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
    queueLoadCurrentFile();
    setVolume(m_volume);
}

void AudioPlayer::play()
{
    if (!m_mpv || m_currentFile.isEmpty()) return;
    m_playWhenLoaded = true;
    if (!m_fileLoaded) queueLoadCurrentFile();

    int paused = 0;
    m_paused = false;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
    setPlaying(true);
}

void AudioPlayer::pause()
{
    if (!m_mpv) return;
    m_playWhenLoaded = false;
    int paused = 1;
    m_paused = true;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
    setPlaying(false);
}

void AudioPlayer::togglePlayPause()
{
    if (m_isPlaying) pause();
    else play();
}

void AudioPlayer::stop()
{
    if (!m_mpv) return;
    m_playWhenLoaded = false;
    m_loadPending = false;
    const char *command[] = {"stop", nullptr};
    mpv_command_async(m_mpv, 0, command);
    setFileLoaded(false);
    setPlaying(false);
    clearMediaInfo();
    if (m_position != 0.0) {
        m_position = 0.0;
        emit positionChanged();
    }
    resetAnalysis();
}

void AudioPlayer::seek(double seconds)
{
    if (!m_mpv || m_currentFile.isEmpty()) return;
    const QByteArray encodedValue = QByteArray::number(std::max(0.0, seconds), 'f', 3);
    const char *command[] = {"seek", encodedValue.constData(), "absolute", nullptr};
    mpv_command_async(m_mpv, 0, command);
}

void AudioPlayer::setVolume(double volume)
{
    const double clamped = std::clamp(volume, 0.0, 1.0);
    if (std::abs(m_volume - clamped) > 0.0001) {
        m_volume = clamped;
        emit volumeChanged();
    }
    if (!m_mpv) return;
    double mpvVolume = m_volume * 100.0;
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &mpvVolume);
}

void AudioPlayer::setSurroundMode(const QString &mode)
{
    if (m_surroundMode == mode) return;
    m_surroundMode = mode;
    emit surroundModeChanged();
    applyAudioFilters();
}

void AudioPlayer::setEqEnabled(bool enabled)
{
    if (m_eqEnabled == enabled) return;
    m_eqEnabled = enabled;
    emit eqEnabledChanged();
    applyAudioFilters();
}

void AudioPlayer::setPreamp(double preamp)
{
    if (std::abs(m_preamp - preamp) < 0.01) return;
    m_preamp = preamp;
    emit preampChanged();
    applyAudioFilters();
}

void AudioPlayer::setEqBands(const QVariantList &bands)
{
    if (m_eqBands == bands) return;
    m_eqBands = bands;
    emit eqBandsChanged();
    applyAudioFilters();
}

void AudioPlayer::applyAudioFilters()
{
    if (!m_mpv) return;

    const QByteArray outputChannels = AudioRouting::outputChannels(m_surroundMode).toUtf8();
    const int channelsStatus = mpv_set_property_string(m_mpv, "audio-channels",
                                                        outputChannels.constData());
    if (channelsStatus < 0) {
        qWarning() << "[AudioPlayer] Failed to set output channels:"
                   << mpv_error_string(channelsStatus);
    }

    QStringList filters;
    if (m_eqEnabled) {
        if (std::abs(m_preamp) > 0.01) {
            filters.append(QString("volume=%1dB").arg(m_preamp, 0, 'f', 2));
        }

        static const int frequencies[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        for (int i = 0; i < std::min(10, static_cast<int>(m_eqBands.size())); ++i) {
            const double gain = m_eqBands[i].toDouble();
            if (std::abs(gain) > 0.01) {
                filters.append(QString("equalizer=f=%1:width_type=o:width=1:g=%2")
                                   .arg(frequencies[i])
                                   .arg(gain, 0, 'f', 2));
            }
        }
    }

    if (AudioRouting::shouldUpmixToSurround(m_surroundMode, m_sourceChannels)) {
        filters.append(AudioRouting::surroundUpmixFilter());
    }

    const QString filter = filters.isEmpty() ? QString() : QString("lavfi=[%1]").arg(filters.join(','));
    const QByteArray encodedFilter = filter.toUtf8();
    const int status = mpv_set_property_string(m_mpv, "af", encodedFilter.constData());
    if (status < 0) qWarning() << "[AudioPlayer] Failed to apply filters:" << mpv_error_string(status);
}

void AudioPlayer::updateSourceAudioParams()
{
    if (!m_mpv) return;

    int64_t channels = 0;
    const int status = mpv_get_property(m_mpv, "audio-params/channel-count",
                                        MPV_FORMAT_INT64, &channels);
    const int detectedChannels = status >= 0 ? static_cast<int>(channels) : 0;
    if (m_sourceChannels == detectedChannels) return;

    m_sourceChannels = detectedChannels;
    emit sourceChannelsChanged();
    qInfo() << "[AudioPlayer] Source audio channels:" << m_sourceChannels;
}

void AudioPlayer::updateAudioTracks()
{
    if (!m_mpv) return;

    mpv_node tracksNode{};
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracksNode) < 0) return;

    QVariantList tracks;
    int selectedId = -1;
    if (tracksNode.format == MPV_FORMAT_NODE_ARRAY && tracksNode.u.list) {
        for (int i = 0; i < tracksNode.u.list->num; ++i) {
            const mpv_node &track = tracksNode.u.list->values[i];
            if (nodeString(track, "type") != QStringLiteral("audio")) continue;

            const int id = nodeInt(track, "id", -1);
            const QString codec = nodeString(track, "codec");
            const QString title = nodeString(track, "title");
            const QString language = nodeString(track, "lang");
            const QString layout = nodeString(track, "demux-channels");
            const int channels = nodeInt(track, "demux-channel-count");
            const int sampleRate = nodeInt(track, "demux-samplerate");
            const bool selected = nodeFlag(track, "selected");

            QStringList description;
            description.append(codecDisplayName(codec));
            if (!layout.isEmpty()) description.append(layout.toUpper());
            else if (channels > 0) description.append(QStringLiteral("%1 CH").arg(channels));
            if (sampleRate > 0) description.append(QStringLiteral("%1 kHz").arg(sampleRate / 1000.0, 0, 'f', sampleRate % 1000 ? 1 : 0));
            if (!title.isEmpty()) description.append(title);
            else if (!language.isEmpty()) description.append(language.toUpper());

            QVariantMap item;
            item.insert(QStringLiteral("id"), id);
            item.insert(QStringLiteral("codec"), codec);
            item.insert(QStringLiteral("codecLabel"), codecDisplayName(codec));
            item.insert(QStringLiteral("title"), title);
            item.insert(QStringLiteral("language"), language);
            item.insert(QStringLiteral("channels"), channels);
            item.insert(QStringLiteral("channelLayout"), layout);
            item.insert(QStringLiteral("sampleRate"), sampleRate);
            item.insert(QStringLiteral("isDefault"), nodeFlag(track, "default"));
            item.insert(QStringLiteral("selected"), selected);
            item.insert(QStringLiteral("label"), description.join(QStringLiteral(" · ")));
            tracks.append(item);
            if (selected) selectedId = id;
        }
    }
    mpv_free_node_contents(&tracksNode);

    if (m_audioTracks == tracks && m_selectedAudioTrackId == selectedId) return;
    m_audioTracks = tracks;
    m_selectedAudioTrackId = selectedId;
    emit audioTracksChanged();
}

QString AudioPlayer::sourceAudioLabel() const
{
    for (const QVariant &entry : m_audioTracks) {
        const QVariantMap track = entry.toMap();
        if (!track.value(QStringLiteral("selected")).toBool()) continue;
        QStringList parts;
        QString codec = track.value(QStringLiteral("codec")).toString().toUpper();
        if (codec == QStringLiteral("AC3")) codec = QStringLiteral("AC-3");
        else if (codec == QStringLiteral("EAC3")) codec = QStringLiteral("E-AC-3");
        else if (codec.startsWith(QStringLiteral("PCM_"))) codec = QStringLiteral("PCM");
        parts.append(codec);
        const QString layout = track.value(QStringLiteral("channelLayout")).toString();
        const int channels = track.value(QStringLiteral("channels")).toInt();
        if (!layout.isEmpty()) parts.append(layout.toUpper());
        else if (channels > 0) parts.append(QStringLiteral("%1 CH").arg(channels));
        return parts.join(QStringLiteral(" "));
    }
    return {};
}

void AudioPlayer::updateOutputAudioParams()
{
    if (!m_mpv) return;

    auto integerProperty = [this](const char *name) {
        int64_t value = 0;
        return mpv_get_property(m_mpv, name, MPV_FORMAT_INT64, &value) >= 0
            ? static_cast<int>(value)
            : 0;
    };
    auto stringProperty = [this](const char *name) {
        char *raw = mpv_get_property_string(m_mpv, name);
        const QString value = raw ? QString::fromUtf8(raw) : QString();
        mpv_free(raw);
        return value;
    };

    const int channels = integerProperty("audio-out-params/channel-count");
    const int sampleRate = integerProperty("audio-out-params/samplerate");
    const QString layout = stringProperty("audio-out-params/hr-channels");
    const QString format = stringProperty("audio-out-params/format");
    if (channels == m_outputChannels && sampleRate == m_outputSampleRate
        && layout == m_outputChannelLayout && format == m_outputSampleFormat) return;

    m_outputChannels = channels;
    m_outputSampleRate = sampleRate;
    m_outputChannelLayout = layout;
    m_outputSampleFormat = format;
    emit outputAudioChanged();
}

QString AudioPlayer::decodedAudioLabel() const
{
    if (m_outputChannels <= 0) return QStringLiteral("PCM OUTPUT PENDING");
    QStringList parts;
    parts.append(sampleFormatLabel(m_outputSampleFormat));
    parts.append(m_outputChannelLayout.isEmpty()
                     ? QStringLiteral("%1 CH").arg(m_outputChannels)
                     : m_outputChannelLayout.toUpper());
    if (m_outputSampleRate > 0) {
        parts.append(QStringLiteral("%1 kHz").arg(m_outputSampleRate / 1000.0, 0, 'f',
                                                   m_outputSampleRate % 1000 ? 1 : 0));
    }
    return parts.join(QStringLiteral(" · "));
}

void AudioPlayer::updateVideoInfo()
{
    if (!m_mpv) return;

    int width = 0;
    int height = 0;
    auto videoDimension = [this](const char *preferred, const char *fallback) {
        int64_t value = 0;
        if (mpv_get_property(m_mpv, preferred, MPV_FORMAT_INT64, &value) >= 0 && value > 0)
            return static_cast<int>(value);
        value = 0;
        return mpv_get_property(m_mpv, fallback, MPV_FORMAT_INT64, &value) >= 0
            ? static_cast<int>(value)
            : 0;
    };
    width = videoDimension("video-params/dw", "video-params/w");
    height = videoDimension("video-params/dh", "video-params/h");

    QString codec;
    mpv_node tracksNode{};
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracksNode) >= 0) {
        if (tracksNode.format == MPV_FORMAT_NODE_ARRAY && tracksNode.u.list) {
            for (int i = 0; i < tracksNode.u.list->num; ++i) {
                const mpv_node &track = tracksNode.u.list->values[i];
                if (nodeString(track, "type") == QStringLiteral("video")
                    && nodeFlag(track, "selected")) {
                    codec = codecDisplayName(nodeString(track, "codec"));
                    if (width <= 0) width = nodeInt(track, "demux-w");
                    if (height <= 0) height = nodeInt(track, "demux-h");
                    break;
                }
            }
        }
        mpv_free_node_contents(&tracksNode);
    }

    const bool hasVideo = width > 0 && height > 0;
    if (hasVideo == m_hasVideo && width == m_videoWidth && height == m_videoHeight
        && codec == m_videoCodec) return;
    m_hasVideo = hasVideo;
    m_videoWidth = width;
    m_videoHeight = height;
    m_videoCodec = codec;
    emit videoInfoChanged();
}

void AudioPlayer::clearMediaInfo()
{
    bool audioTracksChanged = !m_audioTracks.isEmpty() || m_selectedAudioTrackId != -1;
    bool outputChanged = m_outputChannels != 0 || m_outputSampleRate != 0
                         || !m_outputChannelLayout.isEmpty() || !m_outputSampleFormat.isEmpty();
    bool videoChanged = m_hasVideo || !m_videoCodec.isEmpty() || m_videoWidth != 0 || m_videoHeight != 0;

    m_audioTracks.clear();
    m_selectedAudioTrackId = -1;
    m_outputChannels = 0;
    m_outputSampleRate = 0;
    m_outputChannelLayout.clear();
    m_outputSampleFormat.clear();
    if (m_sourceChannels != 0) {
        m_sourceChannels = 0;
        emit sourceChannelsChanged();
    }
    m_hasVideo = false;
    m_videoCodec.clear();
    m_videoWidth = 0;
    m_videoHeight = 0;
    hideVideo();

    if (audioTracksChanged) emit this->audioTracksChanged();
    if (outputChanged) emit outputAudioChanged();
    if (videoChanged) emit videoInfoChanged();
}

void AudioPlayer::selectAudioTrack(int trackId)
{
    if (!m_mpv || trackId < 0 || trackId == m_selectedAudioTrackId) return;
    int64_t id = trackId;
    const int status = mpv_set_property(m_mpv, "aid", MPV_FORMAT_INT64, &id);
    if (status < 0) qWarning() << "[AudioPlayer] Failed to select audio track:" << mpv_error_string(status);
}

void AudioPlayer::showVideo()
{
    if (!m_hasVideo || !m_videoWindow) return;
    m_videoWindow->showMedia(QFileInfo(m_currentFile).completeBaseName(), m_videoWidth, m_videoHeight);
}

void AudioPlayer::hideVideo()
{
    if (m_videoWindow) m_videoWindow->hide();
}

void AudioPlayer::toggleVideo()
{
    if (m_videoVisible) hideVideo();
    else showVideo();
}

void AudioPlayer::resetAnalysis(bool immediate)
{
    if (immediate) {
        m_leftMeter = 0.0;
        m_rightMeter = 0.0;
        for (int i = 0; i < m_spectrum.size(); ++i) m_spectrum[i] = 0.0;
        emit metersChanged();
        emit spectrumChanged();
    }
}

void AudioPlayer::updateAudioAnalysis()
{
    if (!m_isPlaying || !m_fileLoaded) {
        bool metersChanged = false;
        if (m_leftMeter > 0.0005 || m_rightMeter > 0.0005) {
            m_leftMeter *= 0.72;
            m_rightMeter *= 0.72;
            if (m_leftMeter < 0.001) m_leftMeter = 0.0;
            if (m_rightMeter < 0.001) m_rightMeter = 0.0;
            metersChanged = true;
        }

        bool spectrumChanged = false;
        for (int i = 0; i < m_spectrum.size(); ++i) {
            double value = m_spectrum[i].toDouble();
            if (value > 0.0005) {
                value *= 0.68;
                if (value < 0.001) value = 0.0;
                m_spectrum[i] = value;
                spectrumChanged = true;
            }
        }
        if (metersChanged) emit this->metersChanged();
        if (spectrumChanged) emit this->spectrumChanged();
        return;
    }

    double targetLeft = 0.0;
    double targetRight = 0.0;
    std::array<double, 16> targetSpectrum{};
    const bool hasRealAudio = m_audioAnalyzer &&
                              m_audioAnalyzer->snapshot(targetLeft, targetRight, targetSpectrum);

    if (!hasRealAudio) {
        static std::mt19937 random(1337);
        static std::uniform_real_distribution<double> energy(0.42, 0.92);
        const double base = energy(random) * m_volume;
        targetLeft = base * (0.86 + 0.14 * std::sin(m_position * 4.2));
        targetRight = base * (0.86 + 0.14 * std::cos(m_position * 4.5));
        for (int i = 0; i < 16; ++i) {
            const double weighting = 1.0 - i / 21.0;
            targetSpectrum[i] = base * weighting *
                                (0.5 + 0.5 * std::sin(m_position * 8.0 + i * 0.42));
        }
    }

    const double leftRate = targetLeft > m_leftMeter ? 0.55 : 0.18;
    const double rightRate = targetRight > m_rightMeter ? 0.55 : 0.18;
    m_leftMeter += (targetLeft - m_leftMeter) * leftRate;
    m_rightMeter += (targetRight - m_rightMeter) * rightRate;

    for (int i = 0; i < 16; ++i) {
        const double current = m_spectrum[i].toDouble();
        const double rate = targetSpectrum[i] > current ? 0.52 : 0.2;
        m_spectrum[i] = current + (targetSpectrum[i] - current) * rate;
    }

    emit metersChanged();
    emit spectrumChanged();
}
